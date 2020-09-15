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

    ST7735_fill_screen(0x00);   /* clear screen */
    ST7735_draw_HSV();  /* draw a HSV-palette */

    _delay_ms(2000);    /* wait 2 sec */

    /* draw a fill rect with CYAN color */
    ST7735_draw_fill_rect(20, 20, 88, 120, color_565(0, 168, 168));

    _delay_ms(2000);    /* wait 2 sec */

    /* draw a two white cross-lines */
    ST7735_draw_line(20, 20, 107, 139, 0xFFFF);
    ST7735_draw_line(107, 20, 20, 139, 0xFFFF);

    _delay_ms(2000);    /* wait 2 sec */

    /* draw a YELLOW circles with Michener's algorithm */
    for (int8_t i = 40; i > 0; i -= 2)
        ST7735_draw_circle_Mich(64, 80, i, color_565(255, 255, 85));
    
    /* draw 4 filled circles */
    ST7735_draw_fill_circle_Mich(20, 20, 10, color_565(168, 168, 0));
    ST7735_draw_fill_circle_Mich(107, 20, 10, color_565(0, 0, 168));
    ST7735_draw_fill_circle_Mich(107, 139, 10, color_565(168, 0, 168));
    ST7735_draw_fill_circle_Mich(20, 139, 10, color_565(168, 0, 0));
}
