#include <stdint.h>

#include "py/binary.h"
#include <string.h>

#include "extmod/font_petme128_8x8.h"
#include "hal_data.h"
#include "lcd_board.h"
#include "py/mperrno.h"
#include "py/runtime.h"

typedef struct {
    mp_obj_base_t base;
    LCD_DeviceType *lcd;
    LCD_BoardMode mode;
} machine_lcd_obj_t;

/* The display remains initialized across MicroPython soft resets. */
static machine_lcd_obj_t s_machine_lcd;

static LCD_DeviceType *machine_lcd_get_device(machine_lcd_obj_t *self)
{
    if (self->lcd == NULL) {
        mp_raise_OSError(MP_ENODEV);
    }
    return self->lcd;
}

static mp_obj_t machine_lcd_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    const char *name = mp_obj_str_get_str(args[0]);
    LCD_BoardMode mode;
    if (strcmp(name, "hdmi") == 0) {
        mode = LCD_STARTUP_TFP410;
    } else if (strcmp(name, "mipi") == 0) {
        mode = LCD_STARTUP_ST7796U_MIPI;
    } else if (strcmp(name, "spi") == 0) {
        mode = LCD_STARTUP_ST7796U_SPI;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid LCD mode"));
    }

    if (s_machine_lcd.lcd != NULL) {
        if (s_machine_lcd.mode != mode) {
            mp_raise_OSError(MP_EBUSY);
        }
        return MP_OBJ_FROM_PTR(&s_machine_lcd);
    }

    LCD_DeviceType *lcd = LCD_BoardInitialize(mode);
    if (lcd == NULL) {
        mp_raise_OSError(MP_EIO);
    }

    s_machine_lcd.base.type = type;
    s_machine_lcd.lcd = lcd;
    s_machine_lcd.mode = mode;
    return MP_OBJ_FROM_PTR(&s_machine_lcd);
}

static mp_obj_t machine_lcd_fill(mp_obj_t self_in, mp_obj_t color_in)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(color_in);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    uint32_t err = LCD_Fill(lcd, color);
    if (err == FSP_SUCCESS && lcd->peri_model_1 == LCD_PERI_GLCDC) {
        err = LCD_Present(lcd);
    }
    if (err == FSP_ERR_IN_USE) {
        mp_raise_OSError(MP_EBUSY);
    }
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(machine_lcd_fill_obj, machine_lcd_fill);

static mp_obj_t machine_lcd_pixel(size_t n_args, const mp_obj_t *args)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    if (x < 0 || x >= lcd->x_length || y < 0 || y >= lcd->y_length) {
        mp_raise_ValueError(MP_ERROR_TEXT("pixel outside display"));
    }

    if (n_args == 4) {
        uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[3]);
        uint32_t err = LCD_DrawPoint(lcd, (uint16_t)x, (uint16_t)y, color);
        if (err == FSP_SUCCESS && lcd->peri_model_1 == LCD_PERI_GLCDC) {
            err = LCD_Present(lcd);
        }
        if (err == FSP_ERR_IN_USE) {
            mp_raise_OSError(MP_EBUSY);
        }
        if (err != FSP_SUCCESS) {
            mp_raise_OSError(MP_EIO);
        }
        return mp_const_none;
    }

    if (lcd->peri_model_1 != LCD_PERI_GLCDC || lcd->graphic_mem_1 == NULL) {
        mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("pixel read requires a framebuffer"));
    }

    uint8_t *buffer = lcd->graphic_writeable;
    if (lcd->graphic_mem_2 != NULL && !lcd->present_pending) {
        bool is_dirty = lcd->dirty_current.valid
            && x >= lcd->dirty_current.x
            && x - lcd->dirty_current.x < lcd->dirty_current.length
            && y >= lcd->dirty_current.y
            && y - lcd->dirty_current.y < lcd->dirty_current.width;
        if (!is_dirty) {
            buffer = buffer == lcd->graphic_mem_1 ? lcd->graphic_mem_2 : lcd->graphic_mem_1;
        }
    }
    if (buffer == NULL) {
        mp_raise_OSError(MP_EIO);
    }

    size_t index = (size_t)y * (lcd->x_length + lcd->x_dummy) + x;
    switch (lcd->color_format) {
        case LCD_COLOR_FORMAT_RGB565:
            return mp_obj_new_int(((uint16_t *)buffer)[index]);
        case LCD_COLOR_FORMAT_RGB888:
        case LCD_COLOR_FORMAT_ARGB8888:
            return mp_obj_new_int_from_uint(((uint32_t *)buffer)[index]);
        default:
            mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("unsupported pixel format"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_pixel_obj, 3, 4, machine_lcd_pixel);

static mp_obj_t machine_lcd_hline(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t width = mp_obj_get_int(args[3]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[4]);

    if (width <= 0 || y < 0 || y >= lcd->y_length || x >= lcd->x_length) {
        return mp_const_none;
    }
    if (x < 0) {
        if (x <= -width) {
            return mp_const_none;
        }
        width += x;
        x = 0;
    }
    if (width > lcd->x_length - x) {
        width = lcd->x_length - x;
    }

    uint32_t err = LCD_FillRectangle(lcd, (uint16_t)x, (uint16_t)y, (uint16_t)width, 1, color);
    if (err == FSP_SUCCESS && lcd->peri_model_1 == LCD_PERI_GLCDC) {
        err = LCD_Present(lcd);
    }
    if (err == FSP_ERR_IN_USE) {
        mp_raise_OSError(MP_EBUSY);
    }
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_hline_obj, 5, 5, machine_lcd_hline);

static mp_obj_t machine_lcd_vline(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t height = mp_obj_get_int(args[3]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[4]);

    if (height <= 0 || x < 0 || x >= lcd->x_length || y >= lcd->y_length) {
        return mp_const_none;
    }
    if (y < 0) {
        if (y <= -height) {
            return mp_const_none;
        }
        height += y;
        y = 0;
    }
    if (height > lcd->y_length - y) {
        height = lcd->y_length - y;
    }

    uint32_t err = LCD_FillRectangle(lcd, (uint16_t)x, (uint16_t)y, 1, (uint16_t)height, color);
    if (err == FSP_SUCCESS && lcd->peri_model_1 == LCD_PERI_GLCDC) {
        err = LCD_Present(lcd);
    }
    if (err == FSP_ERR_IN_USE) {
        mp_raise_OSError(MP_EBUSY);
    }
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_vline_obj, 5, 5, machine_lcd_vline);

static uint8_t machine_lcd_line_outcode(int64_t x, int64_t y, int64_t max_x, int64_t max_y)
{
    uint8_t code = 0;
    if (x < 0) {
        code |= 1;
    } else if (x > max_x) {
        code |= 2;
    }
    if (y < 0) {
        code |= 4;
    } else if (y > max_y) {
        code |= 8;
    }
    return code;
}

static uint32_t machine_lcd_draw_line(LCD_DeviceType *lcd, int64_t x1, int64_t y1,
    int64_t x2, int64_t y2, uint32_t color)
{
    int64_t max_x = lcd->x_length - 1;
    int64_t max_y = lcd->y_length - 1;

    for (;;) {
        uint8_t code1 = machine_lcd_line_outcode(x1, y1, max_x, max_y);
        uint8_t code2 = machine_lcd_line_outcode(x2, y2, max_x, max_y);
        if ((code1 | code2) == 0) {
            break;
        }
        if (code1 & code2) {
            return FSP_SUCCESS;
        }

        uint8_t code = code1 != 0 ? code1 : code2;
        int64_t x;
        int64_t y;
        if (code & 4) {
            x = x1 + (x2 - x1) * (0 - y1) / (y2 - y1);
            y = 0;
        } else if (code & 8) {
            x = x1 + (x2 - x1) * (max_y - y1) / (y2 - y1);
            y = max_y;
        } else if (code & 2) {
            y = y1 + (y2 - y1) * (max_x - x1) / (x2 - x1);
            x = max_x;
        } else {
            y = y1 + (y2 - y1) * (0 - x1) / (x2 - x1);
            x = 0;
        }
        if (code1 != 0) {
            x1 = x;
            y1 = y;
        } else {
            x2 = x;
            y2 = y;
        }
    }

    int32_t dx = x2 >= x1 ? x2 - x1 : x1 - x2;
    int32_t dy = y2 >= y1 ? y2 - y1 : y1 - y2;
    int32_t sx = x1 < x2 ? 1 : -1;
    int32_t sy = y1 < y2 ? 1 : -1;
    int32_t error = dx - dy;
    for (;;) {
        uint32_t err = LCD_DrawPoint(lcd, (uint16_t)x1, (uint16_t)y1, color);
        if (err != FSP_SUCCESS || (x1 == x2 && y1 == y2)) {
            return err;
        }
        int32_t doubled = 2 * error;
        if (doubled > -dy) {
            error -= dy;
            x1 += sx;
        }
        if (doubled < dx) {
            error += dx;
            y1 += sy;
        }
    }
}

static mp_obj_t machine_lcd_finish(LCD_DeviceType *lcd, uint32_t err)
{
    if (err == FSP_SUCCESS && lcd->peri_model_1 == LCD_PERI_GLCDC) {
        err = LCD_Present(lcd);
    }
    if (err == FSP_ERR_IN_USE) {
        mp_raise_OSError(MP_EBUSY);
    }
    if (err != FSP_SUCCESS) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}

static mp_obj_t machine_lcd_line(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    int64_t x1 = mp_obj_get_int(args[1]);
    int64_t y1 = mp_obj_get_int(args[2]);
    int64_t x2 = mp_obj_get_int(args[3]);
    int64_t y2 = mp_obj_get_int(args[4]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[5]);
    return machine_lcd_finish(lcd, machine_lcd_draw_line(lcd, x1, y1, x2, y2, color));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_line_obj, 6, 6, machine_lcd_line);

static uint32_t machine_lcd_draw_rect(LCD_DeviceType *lcd, int64_t x, int64_t y,
    int64_t width, int64_t height, uint32_t color)
{
    if (width <= 0 || height <= 0) {
        return FSP_SUCCESS;
    }
    int64_t right = x + width;
    int64_t bottom = y + height;
    if (right <= 0 || bottom <= 0 || x >= lcd->x_length || y >= lcd->y_length) {
        return FSP_SUCCESS;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (right > lcd->x_length) {
        right = lcd->x_length;
    }
    if (bottom > lcd->y_length) {
        bottom = lcd->y_length;
    }
    return LCD_FillRectangle(lcd, (uint16_t)x, (uint16_t)y,
        (uint16_t)(right - x), (uint16_t)(bottom - y), color);
}

static uint32_t machine_lcd_draw_point_checked(LCD_DeviceType *lcd, int64_t x, int64_t y, uint32_t color)
{
    if (x < 0 || x >= lcd->x_length || y < 0 || y >= lcd->y_length) {
        return FSP_SUCCESS;
    }
    return LCD_DrawPoint(lcd, (uint16_t)x, (uint16_t)y, color);
}

static mp_obj_t machine_lcd_rect(size_t n_args, const mp_obj_t *args)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    int64_t x = mp_obj_get_int(args[1]);
    int64_t y = mp_obj_get_int(args[2]);
    int64_t width = mp_obj_get_int(args[3]);
    int64_t height = mp_obj_get_int(args[4]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[5]);
    if (width <= 0 || height <= 0) {
        return mp_const_none;
    }

    uint32_t err;
    if (n_args == 7 && mp_obj_is_true(args[6])) {
        err = machine_lcd_draw_rect(lcd, x, y, width, height, color);
    } else {
        err = machine_lcd_draw_rect(lcd, x, y, width, 1, color);
        if (err == FSP_SUCCESS) {
            err = machine_lcd_draw_rect(lcd, x, y + height - 1, width, 1, color);
        }
        if (err == FSP_SUCCESS) {
            err = machine_lcd_draw_rect(lcd, x, y, 1, height, color);
        }
        if (err == FSP_SUCCESS) {
            err = machine_lcd_draw_rect(lcd, x + width - 1, y, 1, height, color);
        }
    }
    return machine_lcd_finish(lcd, err);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_rect_obj, 6, 7, machine_lcd_rect);

static uint32_t machine_lcd_ellipse_points(LCD_DeviceType *lcd, int64_t cx, int64_t cy,
    int64_t x, int64_t y, uint32_t color, uint8_t mask, bool fill)
{
    uint32_t err = FSP_SUCCESS;
    if (fill) {
        if (mask & 1) {
            err = machine_lcd_draw_rect(lcd, cx, cy - y, x + 1, 1, color);
        }
        if (err == FSP_SUCCESS && (mask & 2)) {
            err = machine_lcd_draw_rect(lcd, cx - x, cy - y, x + 1, 1, color);
        }
        if (err == FSP_SUCCESS && (mask & 4)) {
            err = machine_lcd_draw_rect(lcd, cx - x, cy + y, x + 1, 1, color);
        }
        if (err == FSP_SUCCESS && (mask & 8)) {
            err = machine_lcd_draw_rect(lcd, cx, cy + y, x + 1, 1, color);
        }
    } else {
        if (mask & 1) {
            err = machine_lcd_draw_point_checked(lcd, cx + x, cy - y, color);
        }
        if (err == FSP_SUCCESS && (mask & 2)) {
            err = machine_lcd_draw_point_checked(lcd, cx - x, cy - y, color);
        }
        if (err == FSP_SUCCESS && (mask & 4)) {
            err = machine_lcd_draw_point_checked(lcd, cx - x, cy + y, color);
        }
        if (err == FSP_SUCCESS && (mask & 8)) {
            err = machine_lcd_draw_point_checked(lcd, cx + x, cy + y, color);
        }
    }
    return err;
}

static mp_obj_t machine_lcd_ellipse(size_t n_args, const mp_obj_t *args)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    int64_t cx = mp_obj_get_int(args[1]);
    int64_t cy = mp_obj_get_int(args[2]);
    int64_t xr = mp_obj_get_int(args[3]);
    int64_t yr = mp_obj_get_int(args[4]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[5]);
    bool fill = n_args > 6 && mp_obj_is_true(args[6]);
    uint8_t mask = n_args > 7 ? mp_obj_get_int(args[7]) & 15 : 15;
    if (xr < 0 || yr < 0 || xr > 4096 || yr > 4096) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid ellipse radius"));
    }
    if (mask == 0) {
        return mp_const_none;
    }

    uint32_t err = FSP_SUCCESS;
    if (xr == 0 || yr == 0) {
        int64_t radius = xr == 0 ? yr : xr;
        for (int64_t i = 0; i <= radius && err == FSP_SUCCESS; i++) {
            err = machine_lcd_ellipse_points(lcd, cx, cy, xr == 0 ? 0 : i,
                yr == 0 ? 0 : i, color, mask, fill);
        }
        return machine_lcd_finish(lcd, err);
    }

    int64_t two_a = 2 * xr * xr;
    int64_t two_b = 2 * yr * yr;
    int64_t x = xr;
    int64_t y = 0;
    int64_t xchange = yr * yr * (1 - 2 * xr);
    int64_t ychange = xr * xr;
    int64_t ellipse_error = 0;
    int64_t stopping_x = two_b * xr;
    int64_t stopping_y = 0;
    while (stopping_x >= stopping_y && err == FSP_SUCCESS) {
        err = machine_lcd_ellipse_points(lcd, cx, cy, x, y, color, mask, fill);
        y++;
        stopping_y += two_a;
        ellipse_error += ychange;
        ychange += two_a;
        if (2 * ellipse_error + xchange > 0) {
            x--;
            stopping_x -= two_b;
            ellipse_error += xchange;
            xchange += two_b;
        }
    }

    x = 0;
    y = yr;
    xchange = yr * yr;
    ychange = xr * xr * (1 - 2 * yr);
    ellipse_error = 0;
    stopping_x = 0;
    stopping_y = two_a * yr;
    while (stopping_x <= stopping_y && err == FSP_SUCCESS) {
        err = machine_lcd_ellipse_points(lcd, cx, cy, x, y, color, mask, fill);
        x++;
        stopping_x += two_b;
        ellipse_error += xchange;
        xchange += two_b;
        if (2 * ellipse_error + ychange > 0) {
            y--;
            stopping_y -= two_a;
            ellipse_error += ychange;
            ychange += two_a;
        }
    }
    return machine_lcd_finish(lcd, err);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_ellipse_obj, 6, 8, machine_lcd_ellipse);

typedef struct {
    int64_t x;
    int64_t y;
} machine_lcd_point_t;

static mp_obj_t machine_lcd_poly(size_t n_args, const mp_obj_t *args)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    int64_t origin_x = mp_obj_get_int(args[1]);
    int64_t origin_y = mp_obj_get_int(args[2]);
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(args[3], &buffer, MP_BUFFER_READ);
    size_t element_size = mp_binary_get_size('@', buffer.typecode, NULL);
    if (element_size == 0 || buffer.len % (2 * element_size) != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid polygon coordinates"));
    }
    size_t count = buffer.len / (2 * element_size);
    if (count < 3 || count > 64) {
        mp_raise_ValueError(MP_ERROR_TEXT("polygon needs 3 to 64 points"));
    }
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[4]);
    bool fill = n_args > 5 && mp_obj_is_true(args[5]);

    machine_lcd_point_t points[64];
    int64_t nodes[64];
    int64_t min_y = INT64_MAX;
    int64_t max_y = INT64_MIN;
    for (size_t i = 0; i < count; i++) {
        points[i].x = origin_x + mp_obj_get_int(mp_binary_get_val_array(buffer.typecode, buffer.buf, 2 * i));
        points[i].y = origin_y + mp_obj_get_int(mp_binary_get_val_array(buffer.typecode, buffer.buf, 2 * i + 1));
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }
        if (points[i].y > max_y) {
            max_y = points[i].y;
        }
    }

    uint32_t err = FSP_SUCCESS;
    if (fill) {
        if (min_y < 0) {
            min_y = 0;
        }
        if (max_y >= lcd->y_length) {
            max_y = lcd->y_length - 1;
        }
        for (int64_t row = min_y; row <= max_y && err == FSP_SUCCESS; row++) {
            size_t n_nodes = 0;
            for (size_t i = 0; i < count; i++) {
                machine_lcd_point_t a = points[i];
                machine_lcd_point_t b = points[(i + 1) % count];
                if ((a.y <= row && b.y > row) || (b.y <= row && a.y > row)) {
                    nodes[n_nodes++] = a.x + (b.x - a.x) * (row - a.y) / (b.y - a.y);
                }
            }
            for (size_t i = 1; i < n_nodes; i++) {
                int64_t value = nodes[i];
                size_t j = i;
                while (j > 0 && nodes[j - 1] > value) {
                    nodes[j] = nodes[j - 1];
                    j--;
                }
                nodes[j] = value;
            }
            for (size_t i = 0; i + 1 < n_nodes && err == FSP_SUCCESS; i += 2) {
                err = machine_lcd_draw_rect(lcd, nodes[i], row, nodes[i + 1] - nodes[i] + 1, 1, color);
            }
        }
    }
    for (size_t i = 0; i < count && err == FSP_SUCCESS; i++) {
        machine_lcd_point_t a = points[i];
        machine_lcd_point_t b = points[(i + 1) % count];
        err = machine_lcd_draw_line(lcd, a.x, a.y, b.x, b.y, color);
    }
    return machine_lcd_finish(lcd, err);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_poly_obj, 5, 6, machine_lcd_poly);

static mp_obj_t machine_lcd_text(size_t n_args, const mp_obj_t *args)
{
    (void)n_args;
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    LCD_DeviceType *lcd = machine_lcd_get_device(self);
    const uint8_t *str = (const uint8_t *)mp_obj_str_get_str(args[1]);
    int64_t x = mp_obj_get_int(args[2]);
    int64_t y = mp_obj_get_int(args[3]);
    uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[4]);
    uint32_t err = FSP_SUCCESS;

    for (; *str != 0 && err == FSP_SUCCESS; str++, x += 8) {
        uint8_t chr = *str;
        if (chr < 32 || chr > 127) {
            chr = 127;
        }
        const uint8_t *glyph = &font_petme128_8x8[(chr - 32) * 8];
        for (int col = 0; col < 8 && err == FSP_SUCCESS; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 8 && err == FSP_SUCCESS; row++, bits >>= 1) {
                if (bits & 1) {
                    err = machine_lcd_draw_point_checked(lcd, x + col, y + row, color);
                }
            }
        }
    }
    return machine_lcd_finish(lcd, err);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lcd_text_obj, 5, 5, machine_lcd_text);

static mp_obj_t machine_lcd_deinit(mp_obj_t self_in)
{
    machine_lcd_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->lcd != NULL) {
        uint32_t err = LCD_BoardDeinitialize(self->lcd);
        if (err == FSP_ERR_UNSUPPORTED) {
            mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("deinit does not support SPI"));
        }
        if (err != FSP_SUCCESS) {
            mp_raise_OSError(MP_EIO);
        }
        self->lcd = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_lcd_deinit_obj, machine_lcd_deinit);

static const mp_rom_map_elem_t machine_lcd_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_lcd_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_ellipse), MP_ROM_PTR(&machine_lcd_ellipse_obj)},
    {MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&machine_lcd_fill_obj)},
    {MP_ROM_QSTR(MP_QSTR_hline), MP_ROM_PTR(&machine_lcd_hline_obj)},
    {MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&machine_lcd_line_obj)},
    {MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&machine_lcd_pixel_obj)},
    {MP_ROM_QSTR(MP_QSTR_poly), MP_ROM_PTR(&machine_lcd_poly_obj)},
    {MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&machine_lcd_rect_obj)},
    {MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&machine_lcd_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_vline), MP_ROM_PTR(&machine_lcd_vline_obj)},
};
static MP_DEFINE_CONST_DICT(machine_lcd_locals_dict, machine_lcd_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(machine_lcd_type, MP_QSTR_LCD, MP_TYPE_FLAG_NONE, make_new, machine_lcd_make_new, locals_dict, &machine_lcd_locals_dict);
