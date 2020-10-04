#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <defines.h>
#include <spi.h>

#include "ST7735.h"
#include "font.h"

static struct st7735_dev {
    struct spi_device_s spi_dev;
    int16_t tft_cursor;     // current cursor position
    int16_t tft_cursor_x; // top-left corner x-coord of cursor in pixels
    int16_t tft_cursor_y; // top-left corner y-coord of cursor in pixels
    uint16_t tft_text_color;     // text color
    uint16_t tft_text_bg_color;  // background color
    uint8_t tft_flags;
} st7735;

/* Select Display */
#define tft_sel() (bit_clear(*(st7735.spi_dev.cs.port), st7735.spi_dev.cs.pin_num))

/* Deselect Display */
#define tft_desel() (bit_set(*(st7735.spi_dev.cs.port), st7735.spi_dev.cs.pin_num))

/* Set Data mode */
#define tft_data_mode() (bit_set(*(st7735.spi_dev.a0.port), st7735.spi_dev.a0.pin_num))

/* Set Command mode */
#define tft_command_mode() (bit_clear(*(st7735.spi_dev.a0.port), st7735.spi_dev.a0.pin_num))

/*!
 * @brief Convert HSV-color to RGB
 * @param hue Hue [0:360]
 * @param sat Saturation [0:100]
 * @param val Value [0:100]
 * @return RGB color data
 */
color_rgb hsv_to_rgb(uint16_t hue, uint8_t sat, uint8_t val) {
    color_rgb rgb;

    if ((hue < 0) || (hue > 360) || (sat < 0) || (sat > 100) || (val < 0) || (val > 100)) {
        rgb.red = rgb.green = rgb.blue = 0;
        return rgb;
    }
    uint8_t h_i;
    float red, green, blue;
    float v_min, v_inc, v_dec, a;

    h_i = (hue / 60) % 6;

    v_min = (float)((100 - sat) * val) / 100.0F;
    a = ((float)val - v_min) * ((float)(hue % 60) / 60.0F);
    v_inc = v_min + a;
    v_dec = (float)val - a;

    switch (h_i) {
        case 0:
            red = val;
            green = v_inc;
            blue = v_min;
            break;
        case 1:
            red = v_dec;
            green = val;
            blue = v_min;
            break;
        case 2:
            red = v_min;
            green = val;
            blue = v_inc;
            break;
        case 3:
            red = v_min;
            green = v_dec;
            blue = val;
            break;
        case 4:
            red = v_inc;
            green = v_min;
            blue = val;
            break;
        case 5:
        default:
            red = val;
            green = v_min;
            blue = v_dec;
            break;
    }
    rgb.red = (uint8_t)lround((red * 255.0) / 100.0);
    rgb.green = (uint8_t)lround((green * 255.0) / 100.0);
    rgb.blue = (uint8_t)lround((blue * 255.0) / 100.0);

    return rgb;
}

static inline void swap_int16(int16_t *a, int16_t *b) {
    int16_t temp = *a;
    *a = *b;
    *b = temp;
}

static inline void swap_uint8(uint8_t *a, uint8_t *b) {
    uint8_t temp = *a;
    *a = *b;
    *b = temp;
}

/*!
 * @brief Send command
 * @param cmd Command
 */
static inline void write_command(uint8_t cmd) {
    tft_command_mode();
    spi_write(cmd);
    tft_data_mode();
}

/*!
 * @brief Send sequence command-data
 * @param cmd Command
 * @param data Sequence 8-bit data to write
 * @param count Number of bytes to write in \c data
 */
static inline void write_cmd_data(uint8_t cmd, uint8_t *data, size_t count) {
    tft_command_mode();
    spi_write(cmd);
    tft_data_mode();
    spi_write_buf(data, count);
}

/*!
 * @brief Write 8-bit data
 * @param data Data to write
 */
static inline void write_data(uint8_t data) {
    tft_data_mode();
    spi_write(data);
}

/*!
 * @brief Write 16-bit data
 * @param data Data to write
 */
static inline void write_data16(uint16_t data) {
    tft_data_mode();
    spi_write16(data);
}

/*!
 * @brief Write 24-bit data
 * @param data Data to write
 */
static inline void write_data24(uint32_t data) {
    tft_data_mode();
    spi_write24(data);
}

/*!
 * @brief Read 8-bit data after command
 * @param cmd Sending command
 * @return 8-bit data
 */
static inline uint8_t read8(uint8_t cmd) {
    uint8_t result;

    tft_command_mode();
    spi_write(cmd);

    tft_data_mode();
    spi_set_speed(TFT_READ_FREQ);
    set_input(PORTB, SPI_MOSI);
    result = spi_read_8();

    spi_set_speed(TFT_WRITE_FREQ);
    set_output(PORTB, SPI_MOSI);

    return result;
}

/*!
 * @brief SPI displays set an address window rectangle for blitting pixels
 * from left up to right down.
 * @param x Top left corner x coordinate
 * @param y Top left corner x coordinate
 * @param w Width of window
 * @param h Height of window
 */
static inline void set_addr_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint32_t xa = ((uint32_t)x << 16U) | (x + w - 1U);
    uint32_t ya = ((uint32_t)y << 16U) | (y + h - 1U);

    write_command(ST7735_CASET);
    spi_write32(xa);

    write_command(ST7735_RASET);
    spi_write32(ya);

    write_command(ST7735_RAMWR);
    /* It is assumed that the next instruction will be to write pixels. */
}

/*!
 * @brief Put a pixel to display on current coordinate.
 * Checking for entering the screen boundaries is not performed!
 * @param x Top left corner x coordinate
 * @param y Top left corner x coordinate
 * @param color 16-bit RGB565 color
 */
static inline void write_pixel(uint8_t x, uint8_t y, uint16_t color) {
    set_addr_window(x, y, 1, 1);
    spi_write16(color);
}

/*!
 * @brief Write a line. Bresenham's algorithm
 * @param x0  Start point x coordinate
 * @param y0  Start point y coordinate
 * @param x1  End point x coordinate
 * @param y1  End point y coordinate
 * @param color 16-bit 5-6-5 Color
 */
static inline void write_line(int16_t x0, int16_t y0,
                              int16_t x1, int16_t y1,
                              uint16_t color) {
    int16_t dx, dy, err;
    int8_t ystep;
    bool angle = abs(y1 - y0) > abs(x1 - x0);
    if (angle) {
        swap_int16(&x0, &y0);
        swap_int16(&x1, &y1);
    }

    if (x0 > x1) {
        swap_int16(&x0, &x1);
        swap_int16(&y0, &y1);
    }

    dx = x1 - x0;
    dy = abs(y1 - y0);
    err = dx / 2;
    ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        /* check that the pixels are within the screen */
        if (angle) {
            if ((x0 >= 0) && (x0 < TFT_HEIGHT) &&
                (y0 >= 0) && (y0 < TFT_WIDTH)) {
                write_pixel(y0, x0, color);
            }
        } else {
            if ((y0 >= 0) && (y0 < TFT_HEIGHT) &&
                (x0 >= 0) && (x0 < TFT_WIDTH)) {
                write_pixel(x0, y0, color);
            }
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

/*!
 * @brief Put a horizontal line from left to right.
 * Checking for entering the screen boundaries is not performed!
 * @param x0 Start X-coord
 * @param y0 Start Y-coord
 * @param w Length of line
 * @param color 16-bit RGB565 color
 */
static inline void write_Hline(uint8_t x0, uint8_t y0, uint8_t w, uint16_t color) {
    set_addr_window(x0, y0, w, 1);
    for (uint8_t i = 0; i < w; i++)
        spi_write16(color);
}

/*!
 * @brief Put a Vertical line from up to down.
 * Checking for entering the screen boundaries is not performed!
 * @param x0 Start X-coord
 * @param y0 Start Y-coord
 * @param h Length of line
 * @param color 16-bit RGB565 color
 */
static inline void write_Vline(uint8_t x0, uint8_t y0, uint8_t h, uint16_t color) {
    for (int16_t i = 0; i < h; i++)
        write_pixel(x0, y0 + i, color);
}

/*!
 * @brief Put a circle segments.
 * @param x_0 Center of circle. X-coord
 * @param y_0 Center of circle. Y-coord
 * @param x X-coord for drawing
 * @param y Y-coord for drawing
 * @param color 16-bit RGB565 color
 */
static inline void write_circle(int16_t x_0, int16_t y_0,
                                int16_t x, int16_t y, uint16_t color) {
    /* with check that the pixels are within the screen */
    int16_t y_tmp_pos = y_0 + y;
    int16_t y_tmp_neg = y_0 - y;

    int16_t x_tmp = x_0 + x;
    if ((x_tmp >= 0) && (x_tmp < TFT_WIDTH)) {
        if ((y_tmp_pos >= 0) && (y_tmp_pos < TFT_HEIGHT))
            write_pixel(x_tmp, y_tmp_pos, color);
        if ((y_tmp_neg >= 0) && (y_tmp_neg < TFT_HEIGHT))
            write_pixel(x_tmp, y_tmp_neg, color);
    }

    x_tmp = x_0 - x;
    if ((x_tmp >= 0) && (x_tmp < TFT_WIDTH)) {
        if ((y_tmp_pos >= 0) && (y_tmp_pos < TFT_HEIGHT))
            write_pixel(x_tmp, y_tmp_pos, color);
        if ((y_tmp_neg >= 0) && (y_tmp_neg < TFT_HEIGHT))
            write_pixel(x_tmp, y_tmp_neg, color);
    }
}

/*!
 * @brief Put a filled circle segments.
 * @param x_0 Center of circle. X-coord
 * @param y_0 Center of circle. Y-coord
 * @param x X-coord for drawing
 * @param y Y-coord for drawing
 * @param color 16-bit RGB565 color
 */
static inline void write_fill_circle(int16_t x_0, int16_t y_0,
                                     int16_t x, int16_t y, uint16_t color) {
    /* with heck that the pixels are within the screen */
    int16_t x_start = x_0 - x;
    if (x_start >= TFT_WIDTH)
        return;
    
    int16_t y_p = y_0 + y;
    int16_t y_n = y_0 - y;
    int16_t width = x * 2 + 1;
    if (x_start < 0) {
        if ((x_start + width) < 0)
            return;
        width += x_start;
        x_start = 0;
    }
    if ((x_start + width) >= TFT_WIDTH)
        width = TFT_WIDTH - x_start;

    if ((y_p < TFT_HEIGHT) && (y_p >= 0))
        write_Hline(x_start, y_p, width, color);
    if ((y_n < TFT_HEIGHT) && (y_n >= 0))
        write_Hline(x_start, y_n, width, color);
}

/*!
 * @param num if >0 - increment; if <0 - decrement
 */
static void cursor_upd(int8_t num) {
    if (st7735.tft_flags & _BV(TFT_PIX_TEXT)) {
        st7735.tft_cursor_x += num * (FONT_5X7_WIDTH + 1);
        return;
    }
    if (((st7735.tft_cursor % TFT_CURSOR_MAX_C) < (TFT_CURSOR_MAX_C - 1)) ||
         (st7735.tft_flags & _BV(TFT_WRAP_TEXT))) {
        st7735.tft_cursor += num;
    }

    st7735.tft_cursor_x = (st7735.tft_cursor % TFT_CURSOR_MAX_C) * (FONT_5X7_WIDTH + 1);
    st7735.tft_cursor_y = (st7735.tft_cursor / TFT_CURSOR_MAX_C) * (FONT_5X7_HEIGHT + 1);
}


/*!
 * @brief Read 32-bit data from display. !!!Does not work correctly at the moment!!!
 * @param cmd RDDID or RDDST
 * @return 32-bit receive data
 */
uint32_t ST7735_read_info(uint8_t cmd) {
    uint32_t result;

    tft_command_mode();
    tft_sel();
    spi_write(cmd);

    tft_data_mode();
    spi_off();
    set_input(PORTB, SPI_MOSI);
    spi_pulse();    // dummy clock

    spi_set_speed(TFT_READ_FREQ);
    spi_on();
    result = spi_read_32();

    tft_desel();
    spi_set_speed(TFT_WRITE_FREQ);
    set_output(PORTB, SPI_MOSI);

    return result;
}

/*!
 * @brief Read 8-bit data from display. !!!Does not work correctly at the moment!!!
 * @param cmd Command to read
 * @return 8-bit receive data
 */
uint8_t ST7735_read8(uint8_t cmd) {
    uint8_t result;
    tft_sel();
    result = read8(cmd);
    tft_desel();
    return result;
}

/*!
 * @brief Initial display sequence
 * @param cs_num Chip Select (SS) pin number
 * @param cs_port Chip Select port pointer
 * @param a0_num Data/Command (DC) pin number
 * @param a0_port Data/Command port pointer
 * @param rst_num Reset (RST) pin number
 * @param rst_port Reset port pointer
 */
void ST7735_init(uint8_t cs_num, volatile uint8_t *cs_port,
                 uint8_t a0_num, volatile uint8_t *a0_port,
                 uint8_t rst_num, volatile uint8_t *rst_port) {
    uint8_t sreg = SREG;

    cli();

    st7735.spi_dev.cs.pin_num = cs_num;
    st7735.spi_dev.cs.port = cs_port;
    st7735.spi_dev.a0.pin_num = a0_num;
    st7735.spi_dev.a0.port = a0_port;
    st7735.spi_dev.rst.pin_num = rst_num;
    st7735.spi_dev.rst.port = rst_port;
    st7735.spi_dev.intr.pin_num = 0;
    st7735.spi_dev.intr.port = NULL;

    tft_desel();        // deselect
    tft_data_mode();    // data mode

    set_output(*(st7735.spi_dev.cs.port - 1), st7735.spi_dev.cs.pin_num);
    set_output(*(st7735.spi_dev.a0.port - 1), st7735.spi_dev.a0.pin_num);
    set_output(*(st7735.spi_dev.rst.port - 1), st7735.spi_dev.rst.pin_num);
    
    bit_set(*(st7735.spi_dev.rst.port), st7735.spi_dev.rst.pin_num);    // toogle RST low to reset
    _delay_ms(120);
    bit_clear(*(st7735.spi_dev.rst.port), st7735.spi_dev.rst.pin_num);
    _delay_ms(20);
    bit_set(*(st7735.spi_dev.rst.port), st7735.spi_dev.rst.pin_num);
    _delay_ms(150);
    
    st7735.tft_cursor = 0;
    st7735.tft_cursor_x = st7735.tft_cursor_y = 0;
    st7735.tft_text_color = 0xFF;
    st7735.tft_text_bg_color = 0x00;
    st7735.tft_flags = 0;

    fdev_setup_stream(&st7735_stream, ST7735_put_char, NULL, _FDEV_SETUP_WRITE);

    /*
    Set SPI speed for this display. Write speed by default
    Max speed for write is 15.1515... MHz (66 ns).
    Max speed for read is 6.666... MHz (150 ns).
    So, the maximum allowable CPU frequency for
    writing - 1939.39 MHz. For reading - 853.33 MHz.
    */
    /* set write speed as default */
    spi_set_speed(TFT_WRITE_FREQ);

    tft_sel();

    write_command(ST7735_SLPOUT);
    _delay_ms(120);
    write_command(ST7735_DISPON);
    _delay_ms(120);

    uint8_t data = 0b101;
    write_cmd_data(ST7735_COLMOD, &data, 1);   // set 16-bit Color Mode

    uint8_t write_data[4] = {
        0x00, 0x00,     /* X Start = 0 */
        0x00, 0x7F      /* X End = 127 */
    };
    write_cmd_data(ST7735_CASET, write_data, 4);
    
    write_data[0] = write_data[1] = 0x00;   // Y Start = 0
    write_data[2] = 0x00; write_data[3] = 0x9F;   // Y End = 159
    write_cmd_data(ST7735_RASET, write_data, 4);

    data = 0x02;
    write_cmd_data(ST7735_GAMSET, &data, 1);
    
    tft_desel();

    SREG = sreg;
}

/*!
 * @brief Setting inversion color mode.
 * @param val \c true to set inversion mode; \c false to recover from inversion mode
 */
void ST7735_invert_display(bool val) {
    tft_sel();
    write_command(ST7735_INVOFF + (uint8_t)val);
    tft_desel();
}

/*!
 * @brief Setting IDLE mode. Color expression is reduced.
 * The primary and the secondary colors using MSB of each R, G and B in the Frame
 * Memory, 8 color depth data is displayed.
 * 8-Color mode frame frequency is applied.
 * @param val \c true to set IDLE; \c false to recover from IDLE mode
 */
void ST7735_idle_mode(bool val) {
    tft_sel();
    write_command(ST7735_IDMOFF + (uint8_t)val);
    tft_desel();
}

/*!
 * @brief Fill the screen with one color
 * @param rgb565 16-bit 5-6-5 Color to fill
 */
void ST7735_fill_screen(uint16_t rgb565) {
    tft_sel();

    set_addr_window(0, 0, TFT_WIDTH, TFT_HEIGHT);

    for (uint16_t i = 0; i < (TFT_WIDTH * TFT_HEIGHT); i++) {
        spi_write16(rgb565);
    }

    tft_desel();
}

/*!
 * @brief Draw the HSV-color palette
 */
void ST7735_draw_HSV(void) {
    float hue = 0.0F;
    float val;
    float sat;
    color_rgb rgb;

    tft_sel();

    set_addr_window(0, 0, TFT_WIDTH, TFT_HEIGHT);

    for (uint8_t y = 0; y < TFT_HEIGHT; y++){
        val = 100.0F;
        sat = 0.0F;
        for (uint8_t x = 0; x < (TFT_WIDTH / 2); x++) {
            rgb = hsv_to_rgb((uint16_t)hue, (uint8_t)sat, (uint8_t)val);
            spi_write16(color_565(rgb.red, rgb.green, rgb.blue));
            sat += (0.78125F * 2);
        }
        sat = 100.0F;
        for (uint8_t x = 0; x < (TFT_WIDTH / 2); x++) {
            rgb = hsv_to_rgb((uint16_t)hue, (uint8_t)sat, (uint8_t)val);
            spi_write16(color_565(rgb.red, rgb.green, rgb.blue));
            val -= (0.78125F * 2);
        }
        hue += 2.25F;
    }

    tft_desel();
}

/*!
 * @brief Draw a single pixel to the display
 * @param x X-coordinate to draw
 * @param y Y-coordinate to draw
 */
void ST7735_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if ((x >= TFT_WIDTH) || (x < 0) ||
        (y >= TFT_HEIGHT) || (y < 0))
        return;
    
    tft_sel();
    write_pixel((uint8_t)x, (uint8_t)y, color);
    tft_desel();
}

/*!
 * @brief Draw a line
 * @param x0 Start X-coord
 * @param y0 Start Y-coord
 * @param x1 End X-coord
 * @param y1 End Y-coord
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_line(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      uint16_t color) {
    tft_sel();
    write_line(x0, y0, x1, y1, color);
    tft_desel();
}

/*!
 * @brief Draw horizontal line
 * @param x Left x-coord
 * @param y Left y-coord
 * @param w Length (in pixels)
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_Hline(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if ((y >= TFT_HEIGHT) || (y < 0))
        return;
    if (w < 0) {    // if right to left then revert
        x += w;
        w = -w;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if ((x + w) >= TFT_WIDTH)
        w = TFT_WIDTH - x;

    tft_sel();
    write_Hline(x, y, w, color);
    tft_desel();
}

/*!
 * @brief Draw vertical line
 * @param x Left x-coord
 * @param y Left y-coord
 * @param h Height (in pixels)
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_Vline(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if ((x >= TFT_WIDTH) || (x < 0))
        return;
    if (h < 0) {
        y += h;
        h = -h;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((y + h) >= TFT_HEIGHT)
        h = TFT_HEIGHT - y;
    
    tft_sel();
    write_Vline(x, y, h, color);
    tft_desel();
}

/*!
 * @brief Draw a Circle. Bresenham's algorithm
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_circle_Bres(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    if (radius < 0)
        radius = -radius;
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 1 - 2 * radius;
    int16_t error;

    tft_sel();

    while (y >= 0) {
        write_circle(x0, y0, x, y, color);

        error = 2 * (delta + y) - 1;
        if ((delta < 0) && (error <= 0)) {
            delta += 2 * ++x;
            continue;
        }
        if ((delta > 0) && (error > 0)) {
            delta -= 2 * --y;
            continue;
        }
        delta += 2 * (++x - --y);
    }

    tft_desel();
}

/*!
 * @brief Draw a Circle. Michener's algorithm.
 * 15% faster than Bresenham's algorithm.
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_circle_Mich(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    if (radius < 0)
        radius = -radius;
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 3 - 2 * radius;

    tft_sel();

    while (x < y) {
        write_circle(x0, y0, x, y, color);
        write_circle(x0, y0, y, x, color);

        if (delta < 0)
            delta += 4 * x++ + 6;
        else
            delta += 4 * (x++ - y--) + 10;
    }
    if (x == y)
        write_circle(x0, y0, x, y, color);

    tft_desel();
}

/*!
 * @brief Draw a fill circle. Bresenham's algorithm
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_fill_circle_Bres(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    if (radius < 0)
        radius = -radius;
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 1 - 2 * radius;
    int16_t error;

    tft_sel();

    while (y >= 0) {
        write_fill_circle(x0, y0, x, y, color);

        error = 2 * (delta + y) - 1;
        if ((delta < 0) && (error <= 0)) {
            delta += 2 * ++x;
            continue;
        }
        if ((delta > 0) && (error > 0)) {
            delta -= 2 * --y;
            continue;
        }
        delta += 2 * (++x - --y);
    }

    tft_desel();
}

/*!
 * @brief Draw a fill circle. Michener's algorithm.
 * 15% faster than Bresenham's algorithm.
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_fill_circle_Mich(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
    if (radius < 0)
        radius = -radius;
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 3 - 2 * radius;

    tft_sel();

    while (x < y) {
        write_fill_circle(x0, y0, x, y, color);
        write_fill_circle(x0, y0, y, x, color);

        if (delta < 0)
            delta += 4 * x++ + 6;
        else
            delta += 4 * (x++ - y--) + 10;
    }
    if (x == y)
        write_fill_circle(x0, y0, x, y, color);

    tft_desel();
}

/*!
 * @brief Draw a rectangle
 * @param x X-corner of rectangle
 * @param y Y-corner of rectangle
 * @param w Width of rectangle
 * @param h Height of rectangle
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }

    int16_t x_temp = x;
    int16_t y_temp = y + h - 1;
    int16_t w_temp = w;
    int16_t h_temp = h;

    if (x < 0) {
        w_temp += x;
        x_temp = 0;
    }
    if ((x_temp + w_temp) >= TFT_WIDTH)
        w_temp = TFT_WIDTH - x_temp;

    tft_sel();

    if ((y < TFT_HEIGHT) && (y >= 0))
        write_Hline(x_temp, y, w_temp, color);
    if ((y_temp < TFT_HEIGHT) && (y_temp >= 0))
        write_Hline(x_temp, y_temp, w_temp, color);
    
    x_temp = x + w - 1;
    y_temp = y;

    if (y < 0) {
        h_temp += y;
        y_temp = 0;
    }
    if ((y_temp + h_temp) >= TFT_HEIGHT)
        h_temp = TFT_HEIGHT - y_temp;
    
    if ((x < TFT_WIDTH) && (x >= 0))
        write_Vline(x, y_temp, h_temp, color);
    if ((x_temp < TFT_WIDTH) && (x_temp >= 0))
        write_Vline(x_temp, y_temp, h_temp, color);

    tft_desel();
}

/*!
 * @brief Draw a fill rectangle
 * @param x X-corner of rectangle
 * @param y Y-corner of rectangle
 * @param w Width of rectangle
 * @param h Height of rectangle
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_fill_rect(int16_t x, int16_t y,
                           int16_t w, int16_t h, uint16_t color) {
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT) || ((x + w) < 0) || ((y + h) < 0))
        return;
    if ((x + w) >= TFT_WIDTH)
        w = TFT_WIDTH - x;
    if ((y + h) >= TFT_HEIGHT)
        h = TFT_HEIGHT - y;
    
    tft_sel();
    set_addr_window(x, y, w, h);
    for (uint16_t i = 0; i < w * h; i++)
        spi_write16(color);
    tft_desel();
}

/*!
 * @brief Draw a triangle
 * @param x0 Vertex #0 X coord
 * @param y0 Vertex #0 Y coord
 * @param x1 Vertex #1 X coord
 * @param y1 Vertex #1 Y coord
 * @param x2 Vertex #2 X coord
 * @param y2 Vertex #2 Y coord
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_triangle(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color) {
    tft_sel();

    write_line(x0, y0, x1, y1, color);
    write_line(x1, y1, x2, y2, color);
    write_line(x2, y2, x0, y0, color);

    tft_desel();
}

/*!
 * @brief Draw a filled triangle. Using Standard algorithm
 * (http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html)
 * (http://www-users.mat.uni.torun.pl/~wrona/3d_tutor/tri_fillers.html)
 * @param a_x Vertex #A X coord
 * @param a_y Vertex #A Y coord
 * @param b_x Vertex #B X coord
 * @param b_y Vertex #B Y coord
 * @param c_x Vertex #C X coord
 * @param c_y Vertex #C Y coord
 * @param color 16-bit RGB565 color to fill
 */
void ST7735_draw_fill_triangle(int16_t a_x, int16_t a_y,
                               int16_t b_x, int16_t b_y,
                               int16_t c_x, int16_t c_y, uint16_t color) {
    int16_t dx_c, dx_b, dx_a, dy_c, dy_b, dy_a;
    int32_t d_s, d_e;   // delta for start/end drawing line
    int16_t ls_x, l_y, le_x;    // coordinates for drawing line
    
    /* sort coordinates by order (a_y <= b_y <= c_y) */
    if (a_y > b_y) {
        swap_int16(&a_y, &b_y);
        swap_int16(&a_x, &b_x);
    }
    if (b_y > c_y) {
        swap_int16(&b_y, &c_y);
        swap_int16(&b_x, &c_x);
    }
    if (a_y > b_y) {
        swap_int16(&a_y, &b_y);
        swap_int16(&a_x, &b_x);
    }

    tft_sel();

    /* if triangle - horizontal line */
    if (a_y == c_y) {
        if ((a_y < 0) || (a_y) >= TFT_HEIGHT) {
            tft_desel();
            return;
        }
        /* sort x-coordinates from smallest to largest */
        ls_x = le_x = a_x;
        if (b_x < ls_x)
            ls_x = b_x;
        else if (b_x > le_x)
            le_x = b_x;
        if (c_x < ls_x)
            ls_x = c_x;
        else if (c_x > le_x)
            le_x = c_x;
        
        if (ls_x < 0)
            ls_x = 0;
        if (le_x >= TFT_WIDTH)
            le_x = TFT_WIDTH - 1;
        write_Hline(ls_x, a_y, le_x - ls_x + 1, color);
        tft_desel();
        return;
    }

    /* if triangle - vertical line */
    if ((a_x == b_x) && (a_x == c_x)) {
        if ((a_x < 0) || (a_x) >= TFT_WIDTH) {
            tft_desel();
            return;
        }
        write_Vline(a_x, a_y, c_y - a_y + 1, color);
        tft_desel();
        return;
    }

    dx_c = b_x - a_x;
    dy_c = b_y - a_y;
    dx_b = c_x - a_x;
    dy_b = c_y - a_y;
    dx_a = c_x - b_x;
    dy_a = c_y - b_y;

    ls_x = le_x = a_x;
    l_y = a_y;

    d_s = d_e = 0;

    /* upper part of triangle */
    for (; l_y < b_y; l_y++) {
        ls_x = a_x + (uint16_t)lround((double)((float)d_s / (float)dy_c));
        le_x = a_x + (uint16_t)lround((double)((float)d_e / (float)dy_b));
        d_s += dx_c;
        d_e += dx_b;
        if (ls_x > le_x)
            swap_int16(&ls_x, &le_x);
        if (ls_x < 0)
            ls_x = 0;
        if (le_x >= TFT_WIDTH)
            le_x = TFT_WIDTH - 1;
        if ((l_y >= 0) && (l_y < TFT_HEIGHT))
            write_Hline(ls_x, l_y, le_x - ls_x + 1, color);
    }

    /* lower part of triangle */
    /* division by 0 protection */
    if (dy_a && dy_b) {
        d_s = (uint32_t)dx_a * (l_y - b_y);
        d_e = (uint32_t)dx_b * (l_y - a_y);
        for (; l_y <= c_y; l_y++) {
            ls_x = b_x + (uint16_t)lround((double)((float)d_s / (float)dy_a));
            le_x = a_x + (uint16_t)lround((double)((float)d_e / (float)dy_b));
            d_s += dx_a;
            d_e += dx_b;
            if (ls_x > le_x)
                swap_int16(&ls_x, &le_x);
            if (ls_x < 0)
                ls_x = 0;
            if (le_x >= TFT_WIDTH)
                le_x = TFT_WIDTH - 1;
            if ((l_y >= 0) && (l_y < TFT_HEIGHT))
                write_Hline(ls_x, l_y, le_x - ls_x + 1, color);
        }
    } else {
        ls_x = b_x;
        le_x = c_x;
        if (ls_x > le_x)
            swap_int16(&ls_x, &le_x);
        if (ls_x < 0)
            ls_x = 0;
        if (le_x >= TFT_WIDTH)
            le_x = TFT_WIDTH - 1;
        if ((l_y >= 0) && (l_y < TFT_HEIGHT))
            write_Hline(ls_x, l_y, le_x - ls_x + 1, color);
    }

    tft_desel();
}

/*!
 * @brief Set the cursor position by \p x & \p y coordinates
 * @param x Horizontal cursor position (pix if pixel mode; column else)
 * @param y Vertical cursor position (pix if pixel mode; column else)
 */
void ST7735_set_cursor(int16_t x, int16_t y) {
    if (st7735.tft_flags & _BV(TFT_PIX_TEXT)) {
        /* if pixel mode */
        st7735.tft_cursor_x = x;
        st7735.tft_cursor_y = y;
    } else {
        /* if char-pos mode */
        st7735.tft_cursor = TFT_CURSOR_MAX_C * y + x;
        
        st7735.tft_cursor_x = x * (FONT_5X7_WIDTH + 1); // +1 is blank line
        st7735.tft_cursor_y = y * (FONT_5X7_HEIGHT + 1);    // +1 is blank line
    }
}

/*!
 * @brief Get the cursor position
 * @return Cursor position in number of chars
 */
int16_t ST7735_get_cursor(void) {
    return st7735.tft_cursor;
}

/*!
 * @brief Get X-coord of the cursor position in pix
 * @return X-coordinate of cursor
 */
int16_t ST7735_get_cursor_x(void) {
    return st7735.tft_cursor_x;
}

/*!
 * @brief Get Y-coord of the cursor position in pix
 * @return Y-coordinate of cursor
 */
int16_t ST7735_get_cursor_y(void) {
    return st7735.tft_cursor_y;
}

/*!
 * @brief Set the text color
 * @param color 16-bit RGB565 color
 */
void ST7735_set_text_color(uint16_t color) {
    st7735.tft_text_color = color;
}

/*!
 * @brief Set the background color of the text pad.
 * @param color 16-bit RGB565 color
 */
void ST7735_set_text_bg_color(uint16_t color) {
    st7735.tft_text_bg_color = color;
}

/*!
 * @brief Set transparent mode of text
 * @param mode If \c true, the characters will be printed
 * without a pad (transparent). Else characters will be printed
 * on the selected background color.
 */
void ST7735_transp_text(bool mode) {
    bit_write(st7735.tft_flags, TFT_TRANSP_TEXT, mode);
}

/*!
 * @brief Set text wrap
 * @param mode \c true or \c false
 */
void ST7735_wrap_text(bool mode) {
    bit_write(st7735.tft_flags, TFT_WRAP_TEXT, mode);
}

/*!
 * @brief Set custom text position
 * @param mode \c true or \c false
 */
void ST7735_pix_text(bool mode) {
    bit_write(st7735.tft_flags, TFT_PIX_TEXT, mode);
}

/*!
 * @brief Set printing mode (symbols by CP437 or char by ASCII)
 * @param mode \c true for symbols or \c false for chars
 */
void ST7735_symbol_text(bool mode) {
    bit_write(st7735.tft_flags, TFT_SYM_TEXT, mode);
}

/*!
 * @brief Send one character to the screen.
 * @param c Sending char
 * @param stream Stream to sending
 */
int ST7735_put_char(char c, FILE *stream) {
    if ((st7735.tft_cursor_x >= TFT_WIDTH) ||
        (st7735.tft_cursor_y >= TFT_HEIGHT) ||
        ((st7735.tft_cursor_x + FONT_5X7_WIDTH + 1) < 0) ||
        ((st7735.tft_cursor_y + FONT_5X7_HEIGHT + 1) < 0)) {
        /* checking if the given character is printed
           outside the screen boundaries */
        cursor_upd(1);
        
        return 0;
    }
    
    if (!(st7735.tft_flags & _BV(TFT_SYM_TEXT))) {
        uint8_t tmp_val;
        switch (c) {
            case 0x00:  // ^@ \0 NULL
                return 0;
            case 0x01:  // ^A
            case 0x02:  // ^B
            case 0x03:  // ^C
            case 0x04:  // ^D
            case 0x05:  // ^E
            case 0x06:  // ^F
            case 0x07:  // ^G \a
                break;
            case 0x08:  // ^H \b BackSpase
                if (stream->buf[stream->len] == '\t')
                    cursor_upd(-4);
                else
                    cursor_upd(-1);
                return 0;
            case 0x09:  // ^I \t TAB
                tmp_val = (st7735.tft_cursor % TFT_CURSOR_MAX_C) + 1;  // curr column
                if ((tmp_val / 4) < 5)
                    cursor_upd(4 - (tmp_val % 4));
                return 0;
            case 0x0A:  // ^J \n New Line
                tmp_val = (st7735.tft_cursor % TFT_CURSOR_MAX_C) + 1;  // curr column
                cursor_upd(TFT_CURSOR_MAX_C - (tmp_val % TFT_CURSOR_MAX_C) + 1);
                return 0;
            case 0x0B:  // ^K \v
            case 0x0C:  // ^L \f
                break;
            case 0x0D:  // ^M \r Carriage Return
                return 0;
            case 0x0E:  // ^N
            case 0x0F:  // ^O
            case 0x10:  // ^P
            case 0x11:  // ^Q
            case 0x12:  // ^R
            case 0x13:  // ^S
            case 0x14:  // ^T
            case 0x15:  // ^U
            case 0x16:  // ^V
            case 0x17:  // ^W
            case 0x18:  // ^X
            case 0x19:  // ^Y
            case 0x1A:  // ^Z
            case 0x1B:  // ^[ \e ESCAPE
            case 0x1C:  /* ^\ */
            case 0x1D:  // ^]
            case 0x1E:  // ^^
            case 0x1F:  // ^_
            case 0x7F:  // ^?
                break;
            
            default:
                break;
        }
    }
    
    uint8_t tmp_ch;
    
    tft_sel();
    
    if (!(st7735.tft_flags & _BV(TFT_TRANSP_TEXT))) {
        /* if transparency is off, check and restrict
           the address window for the symbol */
        int16_t tmp_x = st7735.tft_cursor_x;
        int16_t tmp_y = st7735.tft_cursor_y;
        uint8_t tmp_w = FONT_5X7_WIDTH + 1;
        uint8_t tmp_h = FONT_5X7_HEIGHT + 1;

        if (tmp_x < 0) {
            tmp_w = (uint8_t)(tmp_w + tmp_x);
            tmp_x = 0;
        }
        if (tmp_y < 0) {
            tmp_h = (uint8_t)(tmp_h + tmp_y);
            tmp_y = 0;
        }
        if ((tmp_x + tmp_w) >= TFT_WIDTH)
            tmp_w = (uint8_t)(TFT_WIDTH - tmp_x);
        if ((tmp_y + tmp_h) >= TFT_HEIGHT)
            tmp_h = (uint8_t)(TFT_HEIGHT - tmp_y);

        set_addr_window(tmp_x, tmp_y,
                        tmp_w, tmp_h);
    }

    for (uint8_t row = 0; row <= FONT_5X7_HEIGHT; row++) {
        tmp_ch = pgm_read_byte(&font5x7_cp437[(uint8_t)c][row]);
        for (uint8_t i = 0; i <= FONT_5X7_WIDTH; i++) {
            if (((st7735.tft_cursor_x + i) < 0) || ((st7735.tft_cursor_x + i) >= TFT_WIDTH) ||
                ((st7735.tft_cursor_y + row) < 0) || ((st7735.tft_cursor_y + row) >= TFT_HEIGHT)) {
                /* skip if the pixel is outside the screen */
                continue;
                }
            if (st7735.tft_flags & _BV(TFT_TRANSP_TEXT)) {
                /* if transparent mode on */
                if (tmp_ch & _BV(i)) {
                    write_pixel(st7735.tft_cursor_x + i,
                                st7735.tft_cursor_y + row,
                                st7735.tft_text_color);
                }
            } else {
                /* if transparent mode off */
                if (tmp_ch & _BV(i))
                    spi_write16(st7735.tft_text_color);
                else
                    spi_write16(st7735.tft_text_bg_color);
            }
        }
    }
    
    tft_desel();
    cursor_upd(1);
    
    return 0;
}

/*!
 * @brief Set ST7735 as std out
 */
void ST7735_set_stdout() {
    stdout = &st7735_stream;
}
