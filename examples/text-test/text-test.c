/*
 * This example was tested on Arduino nano platform with
 * 128x160 pixels TFT-display and ST7735 controller.
 * 
 * Connection diagram:
 *      TFT     NANO
 *      SCK     D13 (SPI SCK)
 *      SDA     D11 (SPI MOSI)
 *      SDA*    D12 (SPI MISO)
 *      A0      D9
 *      RESET   D8
 *      CS      D10 (SPI !SS)
 *
 *      note: SDA also connects to MISO for display reading.
 *            Not used in this example, you do not need to connect.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <defines.h>
#include <spi.h>
#include <ST7735.h>

#define TFT_CS   PORTB2     /* pin10 */
#define TFT_A0   PORTB1     /* pin9 */
#define TFT_RST  PORTB0     /* pin8 */
#define TFT_PORT PORTB


int main(void) {
    spi_init();
    ST7735_init(TFT_CS, TFT_A0, TFT_RST, &TFT_PORT);
    ST7735_set_stdout();    /* set display as std out */
    ST7735_fill_screen(0x00);   /* clear screen */

    ST7735_draw_fill_circle_Mich(63, -15, 70, color_565(210, 0, 0));

    ST7735_set_text_color(0xFFFF);
    ST7735_set_text_bg_color(color_565(0, 0, 168));
    ST7735_set_cursor(0, 0);

    ST7735_wrap_text(true);
    printf_P(PSTR("This is normal text with the wrap and color pad\n\n"));

    ST7735_transp_text(true);
    printf_P(PSTR("And this text with transparent pad\n"));

    ST7735_pix_text(true);

    ST7735_set_cursor(13, 126);
    printf_P(PSTR("custom position 1"));
    
    ST7735_set_text_color(color_565(0, 255, 0));
    ST7735_set_cursor(-30, 20);
    printf_P(PSTR("custom position 2"));

    ST7735_set_text_bg_color(0xFFFF);
    ST7735_set_text_color(0x00);
    ST7735_transp_text(false);
    ST7735_set_cursor(30, 8);
    printf_P(PSTR("custom position 3"));
}
