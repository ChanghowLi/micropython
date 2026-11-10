#include "stubs.h"
#include "camera_board.h"

const int g_ceu_ov5640 = 5640;
const int g_ceu_ov7725 = 7725;
static unsigned camera_init_count, camera_stop_count;
static uint32_t camera_init_error, camera_stop_error;
static i2c_t *camera_bus;

uint32_t CameraInit(Camera *camera, const CameraConfig *config)
{
    ++camera_init_count;
    assert(config->i2c != NULL && config->i2c->opened);
    assert(config->interface == CAMERA_INTERFACE_DVP);
    assert(config->length == 640 && config->width == 480);
    assert(config->peripheral_inst == (config->model == CAMERA_MODEL_OV5640
        ? (const void *)&g_ceu_ov5640 : (const void *)&g_ceu_ov7725));
    camera_bus = config->i2c;
    camera->model = config->model;
    return camera_init_error;
}

uint32_t CameraStopCapture(Camera *camera)
{
    (void)camera;
    ++camera_stop_count;
    return camera_stop_error;
}

void test_camera_board(void)
{
    Camera *camera = NULL, *same = NULL;
    i2c_t *lcd = NULL;
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV5640, NULL) == FSP_ERR_INVALID_ARGUMENT);
    assert(Camera_BoardInitialize((CameraModelEnum)99, &camera) == FSP_ERR_INVALID_ARGUMENT);
    assert(IIC_BoardAcquire(&lcd) == FSP_SUCCESS);
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV5640, &camera) == FSP_SUCCESS);
    assert(camera_bus == lcd && camera != NULL);
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV5640, &same) == FSP_SUCCESS);
    assert(same == camera && camera_init_count == 1);
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV7725, &same) == FSP_ERR_IN_USE);
    camera_stop_error = FSP_ERR_IN_USE;
    assert(Camera_BoardDeinitialize() == FSP_ERR_IN_USE);
    assert(camera_bus->opened);
    camera_stop_error = FSP_SUCCESS;
    assert(Camera_BoardDeinitialize() == FSP_SUCCESS && lcd->opened);
    assert(Camera_BoardDeinitialize() == FSP_SUCCESS);
    assert(IIC_BoardRelease(lcd) == FSP_SUCCESS && !lcd->opened);

    /* Camera-first startup; failed sensor initialization must drop only its reference. */
    camera_init_error = FSP_ERR_ABORTED;
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV7725, &camera) == FSP_ERR_ABORTED);
    assert(camera == NULL && !camera_bus->opened);
    camera_init_error = FSP_SUCCESS;
    assert(Camera_BoardInitialize(CAMERA_MODEL_OV7725, &camera) == FSP_SUCCESS);
    assert(IIC_BoardAcquire(&lcd) == FSP_SUCCESS && lcd == camera_bus);
    assert(IIC_BoardRelease(lcd) == FSP_SUCCESS && camera_bus->opened);
    camera_stop_error = FSP_ERR_NOT_OPEN;
    assert(Camera_BoardDeinitialize() == FSP_SUCCESS && !camera_bus->opened);
    assert(camera_stop_count == 3);
}
