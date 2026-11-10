/* MicroPython controls single-camera HDMI preview or dual-camera capture.
 * The constructor does not start hardware. Board code owns all resources. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "camera_board.h"
#include "FreeRTOS.h"
#include "modmachine.h"
#include "portable.h"
#include "task.h"
#include "py/mperrno.h"
#include "py/runtime.h"

#define CAMERA_FRAME_SDRAM_BASE_ADDR 0x68000000UL
#define CAMERA_FRAME_SDRAM_END_ADDR 0x6A000000UL
#define CAMERA_PREVIEW_SNAPSHOT_TIMEOUT_MS 5000U
#define CAMERA_STOP_TIMEOUT_MS 5000U

#if !MICROPY_ENABLE_FINALISER
#error "CameraFrame requires MicroPython GC finalisers"
#endif

typedef struct {
    mp_obj_base_t base;
    CameraModelEnum model;
} machine_camera_obj_t;

/* Frame metadata lives in the MicroPython GC heap, but image pixels are
 * owned by the FreeRTOS SDRAM heap. Never expose the live CEU/VIN ring. */
typedef struct _machine_camera_frame_obj_t machine_camera_frame_obj_t;
typedef struct _machine_camera_frame_storage_t {
    struct _machine_camera_frame_storage_t *next;
    struct _machine_camera_frame_storage_t *prev;
    machine_camera_frame_obj_t *owner;
} machine_camera_frame_storage_t;

struct _machine_camera_frame_obj_t {
    mp_obj_base_t base;
    machine_camera_frame_storage_t *storage;
    uint8_t *pixels;
    size_t length;
};

static const mp_obj_type_t machine_camera_frame_type;
/* The list nodes live OUTSIDE the MP heap. Linking Python objects to one
 * another would accidentally keep otherwise unreachable frames alive. */
static machine_camera_frame_storage_t *s_camera_frames;

static void machine_camera_frame_release_data(machine_camera_frame_obj_t *frame)
{
    machine_camera_frame_storage_t *storage = frame->storage;
    if (storage == NULL) {
        return;
    }
    if (storage->prev != NULL) {
        storage->prev->next = storage->next;
    } else {
        s_camera_frames = storage->next;
    }
    if (storage->next != NULL) {
        storage->next->prev = storage->prev;
    }
    frame->storage = NULL;
    frame->pixels = NULL;
    frame->length = 0U;
    vPortFree(storage);
}

/* Called before the MicroPython heap is reinitialised on a soft reset.
 * Stop using all old CameraFrame objects after the reset. */
void machine_camera_deinit_all(void)
{
    while (s_camera_frames != NULL) {
        machine_camera_frame_release_data(s_camera_frames->owner);
    }
}

static mp_obj_t machine_camera_frame_release(mp_obj_t self_in)
{
    machine_camera_frame_obj_t *frame = MP_OBJ_TO_PTR(self_in);
    machine_camera_frame_release_data(frame);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_frame_release_obj, machine_camera_frame_release);

static mp_obj_t machine_camera_frame_readinto(size_t n_args, const mp_obj_t *args)
{
    machine_camera_frame_obj_t *frame = MP_OBJ_TO_PTR(args[0]);
    if (frame->pixels == NULL) {
        mp_raise_OSError(MP_ENODEV);
    }
    mp_int_t offset = n_args == 3 ? mp_obj_get_int(args[2]) : 0;
    if (offset < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("negative frame offset"));
    }
    mp_buffer_info_t target;
    mp_get_buffer_raise(args[1], &target, MP_BUFFER_WRITE);
    size_t pos = (size_t)offset;
    if (pos >= frame->length) {
        return MP_OBJ_NEW_SMALL_INT(0);
    }
    size_t count = frame->length - pos;
    if (count > target.len) {
        count = target.len;
    }
    if (count != 0U) {
        memcpy(target.buf, frame->pixels + pos, count);
    }
    return mp_obj_new_int_from_uint(count);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_camera_frame_readinto_obj, 2, 3,
    machine_camera_frame_readinto);

static mp_obj_t machine_camera_frame_size(mp_obj_t self_in)
{
    machine_camera_frame_obj_t *frame = MP_OBJ_TO_PTR(self_in);
    if (frame->pixels == NULL) {
        mp_raise_OSError(MP_ENODEV);
    }
    return mp_obj_new_int_from_uint(frame->length);
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_frame_size_obj, machine_camera_frame_size);

static const mp_rom_map_elem_t machine_camera_frame_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&machine_camera_frame_release_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&machine_camera_frame_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_release), MP_ROM_PTR(&machine_camera_frame_release_obj) },
    { MP_ROM_QSTR(MP_QSTR_size), MP_ROM_PTR(&machine_camera_frame_size_obj) },
};
static MP_DEFINE_CONST_DICT(machine_camera_frame_locals_dict,
    machine_camera_frame_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(machine_camera_frame_type, MP_QSTR_CameraFrame,
    MP_TYPE_FLAG_NONE, locals_dict, &machine_camera_frame_locals_dict);

/* One controller owns the preview or dual-capture session.
 * Store the object statically so it survives MicroPython soft reset. */
static machine_camera_obj_t s_machine_camera;
static bool s_dual_mode;
static uint32_t s_dual_error;

static void machine_camera_check_error(uint32_t err)
{
    s_dual_error = err;
    if (err == FSP_SUCCESS) {
        return;
    }
    int error = MP_EIO;
    if (err == FSP_ERR_IN_USE) {
        error = MP_EBUSY;
    } else if (err == FSP_ERR_OUT_OF_MEMORY) {
        error = MP_ENOMEM;
    } else if (err == FSP_ERR_TIMEOUT) {
        error = MP_ETIMEDOUT;
    } else if (err == FSP_ERR_NOT_OPEN) {
        error = MP_ENODEV;
    } else if (err == FSP_ERR_UNSUPPORTED) {
        error = MP_EOPNOTSUPP;
    }
    mp_raise_OSError(error);
}

static mp_obj_t machine_camera_make_new(const mp_obj_type_t *type, size_t n_args,
    size_t n_kw, const mp_obj_t *args)
{
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    CameraModelEnum model = CAMERA_MODEL_OV5640;
    if (n_args == 1) {
        const char *name = mp_obj_str_get_str(args[0]);
        if (strcmp(name, "ov5640") == 0) {
            model = CAMERA_MODEL_OV5640;
        } else if (strcmp(name, "ov7725") == 0) {
            model = CAMERA_MODEL_OV7725;
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid camera model"));
        }
    }
    /* One singleton controls the hardware; do not switch its sensor while
     * either preview or dual capture owns the camera peripherals. */
    if ((CameraPreview_IsRunning() && model != CameraPreview_GetModel()) ||
        (CameraDual_IsActive() && model != CAMERA_MODEL_OV5640)) {
        mp_raise_OSError(MP_EBUSY);
    }
    s_machine_camera.base.type = type;
    s_machine_camera.model = model;
    return MP_OBJ_FROM_PTR(&s_machine_camera);
}

static mp_obj_t machine_camera_start(mp_obj_t self_in)
{
    machine_camera_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (CameraDual_IsActive()) {
        mp_raise_OSError(MP_EBUSY);
    }
    if (CameraPreview_IsRunning()) {
        return mp_const_false;
    }
    if (machine_lcd_is_initialized()) {
        mp_raise_OSError(MP_EBUSY);
    }
    s_dual_mode = false;
    if (!CameraPreview_StartModel(self->model)) {
        machine_camera_check_error(CameraPreview_GetLastError());
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_start_obj, machine_camera_start);

/* Keep preview start() compatible; dual capture does not initialize HDMI. */
static mp_obj_t machine_camera_start_dual(mp_obj_t self_in)
{
    machine_camera_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->model != CAMERA_MODEL_OV5640) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }
    if (CameraDual_IsRunning()) {
        return mp_const_false;
    }
    s_dual_mode = true;
    if (CameraPreview_IsRunning() || CameraDual_IsActive()) {
        machine_camera_check_error(FSP_ERR_IN_USE);
    }
    Camera *dvp = NULL;
    Camera *mipi = NULL;
    machine_camera_check_error(CameraDual_Start(&dvp, &mipi));
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_start_dual_obj, machine_camera_start_dual);

/* Without an interface, snapshot() returns one CameraFrame from single-camera
 * HDMI preview. With DVP/MIPI, retain the existing dual-mode tuple result.
 * Both paths own independent SDRAM copies, never the live DMA buffers. */
static mp_obj_t machine_camera_snapshot(size_t n_args, const mp_obj_t *args)
{
    bool single = n_args == 1;
    mp_int_t interface = CAMERA_INTERFACE_DVP;
    if (single) {
        if (!CameraPreview_IsRunning() || CameraDual_IsActive()) {
            mp_raise_OSError(MP_ENODEV);
        }
        machine_camera_obj_t *self = MP_OBJ_TO_PTR(args[0]);
        if (self->model != CameraPreview_GetModel()) {
            mp_raise_OSError(MP_EBUSY);
        }
    } else {
        interface = mp_obj_get_int(args[1]);
        if (interface != CAMERA_INTERFACE_DVP && interface != CAMERA_INTERFACE_MIPI_CSI) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid camera interface"));
        }
        s_dual_mode = true;
        if (!CameraDual_IsRunning()) {
            machine_camera_check_error(FSP_ERR_NOT_OPEN);
        }
    }
    size_t width = interface == CAMERA_INTERFACE_DVP ? 640U : 1024U;
    size_t height = interface == CAMERA_INTERFACE_DVP ? 480U : 600U;
    size_t bytes = width * height * 2U;
    /* Allocate the finalisable Python wrapper first, before RTOS memory.
     * No Python allocation or exception may intervene between allocating
     * RTOS memory and linking its owner into the live-frame list. */
    machine_camera_frame_obj_t *frame =
        mp_obj_malloc_with_finaliser(machine_camera_frame_obj_t, &machine_camera_frame_type);
    frame->storage = NULL;
    frame->pixels = NULL;
    frame->length = 0U;
    size_t allocation_bytes = sizeof(machine_camera_frame_storage_t) + bytes;
    machine_camera_frame_storage_t *storage = pvPortMalloc(allocation_bytes);
    if (storage == NULL) {
        machine_camera_check_error(FSP_ERR_OUT_OF_MEMORY);
    }
    /* Match the existing uncached SDRAM heap used for CEU/VIN frames. */
    uintptr_t address = (uintptr_t)storage;
    if (address < CAMERA_FRAME_SDRAM_BASE_ADDR ||
        address > CAMERA_FRAME_SDRAM_END_ADDR - allocation_bytes) {
        vPortFree(storage);
        machine_camera_check_error(FSP_ERR_INVALID_ADDRESS);
    }
    frame->storage = storage;
    frame->pixels = (uint8_t *)(storage + 1);
    frame->length = bytes;
    storage->owner = frame;
    storage->prev = NULL;
    storage->next = s_camera_frames;
    if (s_camera_frames != NULL) {
        s_camera_frames->prev = storage;
    }
    s_camera_frames = storage;
    uint32_t err = single
        ? CameraPreview_CopyFrame(frame->pixels, frame->length,
            CAMERA_PREVIEW_SNAPSHOT_TIMEOUT_MS)
        : CameraDual_CopyFrame((CameraInterfaceEnum)interface,
            frame->pixels, frame->length, 1000U);
    if (err != FSP_SUCCESS) {
        machine_camera_frame_release_data(frame);
        machine_camera_check_error(err);
    }
    if (single) {
        return MP_OBJ_FROM_PTR(frame);
    }
    mp_obj_t result[] = {
        MP_OBJ_FROM_PTR(frame), mp_obj_new_int_from_uint(width),
        mp_obj_new_int_from_uint(height),
        MP_OBJ_NEW_QSTR(MP_QSTR_RGB565),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(result), result);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_camera_snapshot_obj, 1, 2,
    machine_camera_snapshot);

/* Used by both Camera.stop() and machine_deinit() on soft reset. Do not
 * raise a Python exception: the soft-reset path has already deinitialised
 * the VM. Retain hardware resources if a stop cannot complete safely. */
uint32_t machine_camera_stop_all(void)
{
    if (CameraDual_IsActive()) {
        return CameraDual_Stop();
    }
    if (CameraPreview_IsRunning()) {
        CameraPreview_RequestStop();
        TickType_t begin = xTaskGetTickCount();
        while (CameraPreview_IsRunning()) {
            if ((uint32_t)(xTaskGetTickCount() - begin) * portTICK_PERIOD_MS >=
                CAMERA_STOP_TIMEOUT_MS) {
                return FSP_ERR_TIMEOUT;
            }
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
    return CameraPreview_Cleanup();
}

static mp_obj_t machine_camera_stop(mp_obj_t self_in)
{
    (void)self_in;
    if (CameraDual_IsActive()) {
        s_dual_mode = true;
    }
    machine_camera_check_error(machine_camera_stop_all());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_stop_obj, machine_camera_stop);

static mp_obj_t machine_camera_is_running(mp_obj_t self_in)
{
    (void)self_in;
    return mp_obj_new_bool(CameraPreview_IsRunning() || CameraDual_IsRunning());
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_is_running_obj, machine_camera_is_running);

static mp_obj_t machine_camera_last_error(mp_obj_t self_in)
{
    (void)self_in;
    return mp_obj_new_int_from_uint(s_dual_mode ? s_dual_error : CameraPreview_GetLastError());
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_camera_last_error_obj, machine_camera_last_error);

/* init() starts preview; deinit() stops either mode and releases resources.
 * Keep both aliases for users familiar with other machine peripherals. */
static const mp_rom_map_elem_t machine_camera_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_DVP), MP_ROM_INT(CAMERA_INTERFACE_DVP) },
    { MP_ROM_QSTR(MP_QSTR_MIPI), MP_ROM_INT(CAMERA_INTERFACE_MIPI_CSI) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_camera_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_camera_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_running), MP_ROM_PTR(&machine_camera_is_running_obj) },
    { MP_ROM_QSTR(MP_QSTR_last_error), MP_ROM_PTR(&machine_camera_last_error_obj) },
    { MP_ROM_QSTR(MP_QSTR_snapshot), MP_ROM_PTR(&machine_camera_snapshot_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&machine_camera_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_start_dual), MP_ROM_PTR(&machine_camera_start_dual_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&machine_camera_stop_obj) },
};
static MP_DEFINE_CONST_DICT(machine_camera_locals_dict, machine_camera_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(machine_camera_type, MP_QSTR_Camera, MP_TYPE_FLAG_NONE,
    make_new, machine_camera_make_new, locals_dict, &machine_camera_locals_dict);
