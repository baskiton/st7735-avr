/*
    Library for TFT-display on ST7735 controller (128x160)
    Datasheet: https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
*/
#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stdbool.h>

#include <defines.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_rgb;

typedef struct {
    int16_t hue;
    int8_t sat;
    int8_t val;
} color_hsv;

extern color_rgb hsv_to_rgb(uint16_t hue, uint8_t sat, uint8_t val);

/* Convert color:
 * color_565 - RGB to 565 (16bit color, 16bit data)
 * color_666 - RGB to 666 (18bit color, 24bit data)
 */

#define color_565(red, green, blue) ((((red) & 0xF8) << 8) | (((green) & 0xFC) << 3) | ((blue) >> 3))
#define color_666(red, green, blue) ((((red) & 0xFC) << 16) | (((green) & 0xFC) << 8) | ((blue) & 0xFC))

#define TFT_WIDTH 128
#define TFT_HEIGTH 160

/* System function Command List
 *     Undefined commands are treated as NOP (00 h) command.
 *     Commands 10h, 12h, 13h, 20h, 21h, 26h, 28h, 29h, 30h, 36h (ML parameter only),
 *     38h and 39h are updated during V-sync when Module is in Sleep Out Mode
 *     to avoid abnormal visual effects. During Sleep In mode,
 *     these commands are updated immediately.
 *     Read status (09h),
 *     Read Display Power Mode (0Ah),
 *     Read Display MADCTL (0Bh),
 *     Read Display Pixel Format (0Ch),
 *     Read Display Image Mode (0Dh),
 *     Read Display Signal Mode (0Eh).
 */
#define ST7735_NOP          0x00    // No Operation
#define ST7735_SWRESET      0x01    // Software reset
#define ST7735_RDDID        0x04    // Read Display ID
#define ST7735_RDDST        0x09    // Read Display Status
#define ST7735_RDDPM        0x0A    // Read Display Power
#define ST7735_RDDMADCTL    0x0B    // Read Display
#define ST7735_RDDCOLMOD    0x0C    // Read Display Pixel
#define ST7735_RDDIM        0x0D    // Read Display Image
#define ST7735_RDDSM        0x0E    // Read Display Signal
#define ST7735_SLPIN        0x10    // Sleep in & booster off
#define ST7735_SLPOUT       0x11    // Sleep out & booster on
#define ST7735_PTLON        0x12    // Partial mode on
#define ST7735_NORON        0x13    // Partial off (Normal)
#define ST7735_INVOFF       0x20    // Display inversion off
#define ST7735_INVON        0x21    // Display inversion on
#define ST7735_GAMSET       0x26    // Gamma curve select
#define ST7735_DISPOFF      0x28    // Display off
#define ST7735_DISPON       0x29    // Display on
#define ST7735_CASET        0x2A    // Column address set
#define ST7735_RASET        0x2B    // Row address set
#define ST7735_RAMWR        0x2C    // Memory write
#define ST7735_RAMRD        0x2E    // Memory read
#define ST7735_PTLAR        0x30    // Partial start/end address set
#define ST7735_TEOFF        0x34    // Tearing effect line off
#define ST7735_TEON         0x35    // Tearing effect mode set & on
#define ST7735_MADCTL       0x36    // Memory data access control
#define ST7735_IDMOFF       0x38    // Idle mode off
#define ST7735_IDMON        0x39    // Idle mode on
#define ST7735_COLMOD       0x3A    // Interface pixel format
#define ST7735_RDID1        0xDA    // Read ID1
#define ST7735_RDID2        0xDB    // Read ID2
#define ST7735_RDID3        0xDC    // Read ID3

/* Panel Function Command List */
#define ST7735_FRMCTR1      0xB1    // In normal mode (Full colors)
#define ST7735_FRMCTR2      0xB2    // In Idle mode (8-colors)
#define ST7735_FRMCTR3      0xB3    // In partial mode + Full colors
#define ST7735_INVCTR       0xB4    // Display inversion control
#define ST7735_DISSET5      0xB6    // Display function setting
#define ST7735_PWCTR1       0xC0    // Power control setting
#define ST7735_PWCTR2       0xC1    // Power control setting
#define ST7735_PWCTR3       0xC2    // In normal mode (Full colors)
#define ST7735_PWCTR4       0xC3    // In Idle mode (8-colors)
#define ST7735_PWCTR5       0xC4    // In partial mode + Full colors
#define ST7735_VMCTR1       0xC5    // VCOM control 1
#define ST7735_VMOFCTR      0xC7    // Set VCOM offset control
#define ST7735_WRID2        0xD1    // Set LCM version code
#define ST7735_WRID3        0xD2    // Customer Project code
#define ST7735_PWCTR6       0xFC    // In partial mode + Idle
#define ST7735_NVCTR1       0xD9    // EEPROM control status
#define ST7735_NVCTR2       0xDE    // EEPROM Read NVCTR2 10.2.17 Command
#define ST7735_NVCTR3       0xDF    // EEPROM Write Command
#define ST7735_GAMCTRP1     0xE0    // Set Gamma adjustment (+ polarity)
#define ST7735_GAMCTRN1     0xE1    // Set Gamma adjustment (- polarity)
#define ST7735_EXTCTRL      0xF0    // Extension Command Control
#define ST7735_VCOM4L       0xFF    // Vcom 4 Level control


uint8_t tft_cs;
uint8_t tft_a0;
uint8_t tft_rst;
volatile uint8_t* tft_port;

/* Select Display */
#define tft_sel() (bit_clear(*tft_port, tft_cs))

/* Deselect Display */
#define tft_desel() (bit_set(*tft_port, tft_cs))

/* Set Data mode */
#define tft_data_mode() (bit_set(*tft_port, tft_a0))

/* Set Command mode */
#define tft_command_mode() (bit_clear(*tft_port, tft_a0))


extern void ST7735_init(int8_t cs, int8_t a0, int8_t rst, volatile uint8_t *port);

extern void ST7735_invert_display(bool val);
extern void ST7735_idle_mode(bool val);
extern uint32_t ST7735_read_info(uint8_t cmd);
extern void ST7735_fill_screen(uint16_t rgb565);
extern void ST7735_draw_HSV(void);

extern void ST7735_draw_pixel(uint8_t x, uint8_t y, uint16_t color);
extern void ST7735_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);
extern void ST7735_draw_Hline(uint8_t x, uint8_t y, uint8_t w, uint16_t color);
extern void ST7735_draw_Vline(uint8_t x, uint8_t y, uint8_t h, uint16_t color);
extern void ST7735_draw_circle_Bres(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t color);
extern void ST7735_draw_circle_Mich(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t color);
extern void ST7735_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
extern void ST7735_draw_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
extern void ST7735_draw_triangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color);

#endif  /* !ST7735_H */