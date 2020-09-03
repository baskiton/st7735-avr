#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include <defines.h>
#include <spi.h>

#include "ST7735.h"


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
    double red, green, blue;
    double v_min, v_inc, v_dec, a;

    h_i = (hue / 60) % 6;

    v_min = ((100 - sat) * val) / 100.0;
    a = (val - v_min) * ((hue % 60) / 60.0);
    v_inc = v_min + a;
    v_dec = val - a;

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
    set_pin_mode(tft_port, SPI_MOSI, INPUT);
    result = spi_read_8();
    set_pin_mode(tft_port, SPI_MOSI, OUTPUT);

    return result;
}

/*!
 * @brief SPI displays set an address window rectangle for blitting pixels
 * @param x Top left corner x coordinate
 * @param y Top left corner x coordinate
 * @param w Width of window
 * @param h Height of window
 */
static inline void set_addr_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
    uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);

    write_command(ST7735_CASET);
    spi_write32(xa);

    write_command(ST7735_RASET);
    spi_write32(ya);

    write_command(ST7735_RAMWR);
}


static inline void write_pixel(uint8_t x, uint8_t y, uint16_t color) {
    // if ((x >= TFT_WIDTH) || (y >= TFT_HEIGTH))
    //     return;
    
    set_addr_window(x, y, 1, 1);
    spi_write16(color);
}

/*!
 * @brief    Write a line. Bresenham's algorithm
 * @param    x0  Start point x coordinate
 * @param    y0  Start point y coordinate
 * @param    x1  End point x coordinate
 * @param    y1  End point y coordinate
 * @param    color 16-bit 5-6-5 Color
 */
static inline void write_line(uint8_t x0, uint8_t y0,
                              uint8_t x1, uint8_t y1,
                              uint16_t color) {
    bool angle = abs(y1 - y0) > abs(x1 - x0);
    if (angle) {
        swap_uint8(&x0, &y0);
        swap_uint8(&x1, &y1);
    }

    if (x0 > x1) {
        swap_uint8(&x0, &x1);
        swap_uint8(&y0, &y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int8_t err = dx / 2;
    int8_t ystep;

    ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        angle ? write_pixel((uint8_t)y0, (uint8_t)x0, color) : write_pixel((uint8_t)x0, (uint8_t)y0, color);
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

static inline void write_Hline(uint8_t x0, uint8_t y0, uint8_t w, uint16_t color) {
    set_addr_window(x0, y0, w, 1);
    for (uint8_t i = 0; i < w; i++)
        spi_write16_precheck(color);
}

static inline void write_Vline(uint8_t x0, uint8_t y0, uint8_t h, uint16_t color) {
    write_line(x0, y0, x0, y0 + h - 1, color);
}

static inline void write_circle(int16_t x_0, int16_t y_0,
                                int16_t x, int16_t y, uint16_t color) {
    // write_pixel(x_0 + x, y_0 + y, color);
    // write_pixel(x_0 + x, y_0 - y, color);
    // write_pixel(x_0 - x, y_0 + y, color);
    // write_pixel(x_0 - x, y_0 - y, color);

    // Check that the pixels are within the screen
    int16_t y_t_p = y_0 + y;
    int16_t y_t_n = y_0 - y;

    int16_t x_t = x_0 + x;
    if ((x_t >= 0) && (x_t < TFT_WIDTH)) {
        if ((y_t_p >= 0) && (y_t_p < TFT_HEIGTH))
            write_pixel((uint8_t)x_t, (uint8_t)y_t_p, color);
        if ((y_t_n >= 0) && (y_t_n < TFT_HEIGTH))
            write_pixel((uint8_t)x_t, (uint8_t)y_t_n, color);
    }

    x_t = x_0 - x;
    if ((x_t >= 0) && (x_t < TFT_WIDTH)) {
        if ((y_t_p >= 0) && (y_t_p < TFT_HEIGTH))
            write_pixel((uint8_t)x_t, (uint8_t)y_t_p, color);
        if ((y_t_n >= 0) && (y_t_n < TFT_HEIGTH))
            write_pixel((uint8_t)x_t, (uint8_t)y_t_n, color);
    }
}

/*!
 * @brief 
 * @param 
 * @return 
 */
uint32_t ST7735_read_info(uint8_t cmd) {
    uint32_t result;

    tft_command_mode();
    tft_sel();
    spi_write(cmd);

    tft_data_mode();
    spi_off();
    set_pin_mode(tft_port, SPI_MOSI, INPUT);
    bit_set(PORTB, PORTB5);
    bit_clear(PORTB, PORTB5);
    spi_on();

    result = spi_read_32();
    // spi_read_buf(&result, sizeof(result));

    tft_desel();
    set_pin_mode(tft_port, SPI_MOSI, OUTPUT);

    return result;
}

/*!
 * @brief Initial display sequence
 * @param cs Chip Select (SS) pin number of \c port
 * @param a0 Data/Command (DC) pin number of \c port
 * @param rst Reset (RST) pin number of \c port
 * @param port Pointer to port (B, C or D)
 */
void ST7735_init(int8_t cs, int8_t a0, int8_t rst, volatile uint8_t *port) {
    tft_cs = cs;
    tft_a0 = a0;
    tft_rst = rst;
    tft_port = port;

    set_output(*(port - 1), cs);
    set_output(*(port - 1), a0);
    set_output(*(port - 1), rst);
    
    bit_set(*port, cs);  // deselect
    bit_set(*port, a0);  // data mode

    bit_set(*port, rst); // toogle RST low to reset
    _delay_ms(120);
    bit_clear(*port, rst);
    _delay_ms(20);
    bit_set(*port, rst);
    _delay_ms(150);
    
    tft_sel();

    write_command(ST7735_SLPOUT);
    _delay_ms(120);
    write_command(ST7735_DISPON);
    _delay_ms(120);

    uint8_t data = 0b101;
    write_cmd_data(ST7735_COLMOD, &data, 1);   // set 16-bit Color Mode

    uint8_t write_data[4] = {
        0x00, 0x00,     // X Start = 0
        0x00, 0x7F      // X End = 127
    };
    write_cmd_data(ST7735_CASET, write_data, 4);
    
    write_data[0] = write_data[1] = 0x00;   // Y Start = 0
    write_data[2] = 0x00; write_data[3] = 0x9F;   // Y End = 159
    write_cmd_data(ST7735_RASET, write_data, 4);

    data = 0x02;
    write_cmd_data(ST7735_GAMSET, &data, 1);
    
    tft_desel();
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

    set_addr_window(0, 0, TFT_WIDTH, TFT_HEIGTH);

    for (uint16_t i = 0; i < (TFT_WIDTH * TFT_HEIGTH); i++) {
        spi_write16_precheck(rgb565);
    }

    tft_desel();
}

/*!
 * @brief Draw the HSV-color palette
 */
void ST7735_draw_HSV(void) {
    double hue = 0.0;
    double val = 100.0;
    double sat = 0.0;
    color_rgb rgb;

    tft_sel();

    set_addr_window(0, 0, TFT_WIDTH, TFT_HEIGTH);

    for (uint8_t y = 0; y < TFT_HEIGTH; y++){
        val = 100.0;
        sat = 0.0;
        for (uint8_t x = 0; x < (TFT_WIDTH / 2); x++) {
            rgb = hsv_to_rgb((uint16_t)hue, (uint8_t)sat, (uint8_t)val);
            spi_write16_precheck(color_565(rgb.red, rgb.green, rgb.blue));
            sat += (0.78125 * 2);
        }
        sat = 100.0;
        for (uint8_t x = 0; x < (TFT_WIDTH / 2); x++) {
            rgb = hsv_to_rgb((uint16_t)hue, (uint8_t)sat, (uint8_t)val);
            spi_write16_precheck(color_565(rgb.red, rgb.green, rgb.blue));
            val -= (0.78125 * 2);
        }
        hue += 2.25;
    }

    tft_desel();
}

/*!
 * @brief Draw a single pixel to the display
 */
void ST7735_draw_pixel(uint8_t x, uint8_t y, uint16_t color) {
    // if ((x >= TFT_WIDTH) || (y >= TFT_HEIGTH))
    //     return;
    
    tft_sel();

    set_addr_window(x, y, 1, 1);
    spi_write16(color);

    tft_desel();
}

/*!
 *
 */
void ST7735_draw_line(uint8_t x0, uint8_t y0,
                      uint8_t x1, uint8_t y1,
                      uint16_t color) {
    tft_sel();
    write_line(x0, y0, x1, y1, color);
    tft_desel();
}

/*!
 * @brief Draw horizontal line
 * @param x Left x-coord
 * @param y Left y-coord
 * @param w Width (pixels)
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_Hline(uint8_t x, uint8_t y, uint8_t w, uint16_t color) {
    tft_sel();
    write_Hline(x, y, w, color);
    tft_desel();
}

/*!
 * @brief Draw horizontal line
 * @param x Left x-coord
 * @param y Left y-coord
 * @param h Height (pixels)
 * @param color 16-bit RGB565 color
 */
void ST7735_draw_Vline(uint8_t x, uint8_t y, uint8_t h, uint16_t color) {
    tft_sel();
    write_line(x, y, x, y + h - 1, color);
    tft_desel();
}

/*!
 * @brief Draw a Circle. Bresenham's algorithm
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_circle_Bres(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t color) {
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 1 - 2 * radius;
    int16_t error = 0;

    tft_sel();

    while (y >= 0) {
        write_circle(x0, y0, x, y, color);

        error = 2 * (delta + y) - 1;
        if ((delta < 0) && (error <= 0)) {
            delta += 2 * ++x + 1;
            continue;
        }
        error = 2 * (delta - x) - 1;
        if ((delta > 0) && (error > 0)) {
            delta -= 2 * --y + 1;
            continue;
        }
        delta += 2 * (++x - --y);
    }

    tft_desel();
}

/*!
 * @brief Draw a Circle. Michener's algorithm
 * @param x0 Center point, x coord
 * @param y0 Center point, y coord
 * @param radius Radius of circle
 * @param color 16-bit RGB565 draw color
 */
void ST7735_draw_circle_Mich(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t color) {
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
 *
 */
void ST7735_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    tft_sel();

    write_Hline(x, y, w, color);
    write_Hline(x, y + h - 1, w, color);
    write_Vline(x, y, h, color);
    write_Vline(x + w - 1, y, h, color);

    tft_desel();
}

/*!
 *
 */
void ST7735_draw_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    tft_sel();

    set_addr_window(x, y, w, h);
    for (uint16_t i = 0; i < w * h; i++)
        spi_write16_precheck(color);
    
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
void ST7735_draw_triangle(uint8_t x0, uint8_t y0,
                          uint8_t x1, uint8_t y1,
                          uint8_t x2, uint8_t y2, uint16_t color) {
    tft_sel();

    write_line(x0, y0, x1, y1, color);
    write_line(x1, y1, x2, y2, color);
    write_line(x2, y2, x0, y0, color);

    tft_desel();
}
