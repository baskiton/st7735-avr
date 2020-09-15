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
#include <string.h>

#include <defines.h>
#include <spi.h>
#include <uart.h>
#include <ST7735.h>

#define TFT_CS   PORTB2     /* pin10 */
#define TFT_A0   PORTB1     /* pin9 */
#define TFT_RST  PORTB0     /* pin8 */
#define TFT_PORT PORTB

extern void cmd_help();
extern void cmd_cls();
extern void cmd_reboot();


/* getting string from std input and putting chars to stdout */
int get_string(char *str, int size) {
    char ch;
    int8_t i = 0;

    while (1) {
        putchar('_');
        putchar('\b');
        ch = getchar();

        switch (ch) {
            case '\0':
                break;
            case '\n':
                putchar(' ');
                putchar('\n');
                return 0;
            case '\t':
                ch = ' ';
                str[i] = ch;
                i++;
                putchar(ch);
                break;
            case '\b':
            case '\x7F':
                if (i > 0) {
                    i--;
                    str[i] = '\0';
                    putchar(' ');
                    putchar('\b');
                    putchar('\b');
                    putchar(' ');
                    putchar('\b');
                }
                break;
            
            default:
                if (i < (size - 1)) {
                    str[i] = ch;
                    i++;
                    putchar(ch);
                }
                break;
        }
    }
    return 0;
}

void cmd() {
    static uint8_t inv_mode = 0;
    char buf[20];   /* buffer for input */

    while (1) {
        memset(buf, 0, sizeof buf);
        putchar('>');

        get_string(buf, sizeof buf);

        if (buf[0] == 0)
            continue;
        else if (!strcmp_P(buf, PSTR("help"))) {
            cmd_help();
        }
        else if (!strcmp_P(buf, PSTR("cls"))) {
            cmd_cls();
        }
        else if (!strcmp_P(buf, PSTR("invert"))) {
            inv_mode ^= 1U;
            ST7735_invert_display(inv_mode);
        }
        else if (!strcmp_P(buf, PSTR("sp"))) {
            /* print Stack Pointer */
            printf_P(PSTR("SP: 0x%04X\n"), SP);
        }
        else if (!strcmp_P(buf, PSTR("reboot"))) {
            cmd_reboot();
        }
        else if (!strcmp_P(buf, PSTR("exit"))) {
            printf_P(PSTR("Exiting...\n"));
            return;
        }
        else {
            printf_P(PSTR("Unknown command: %s\n"), buf);
        }
    }
}

int main(void) {
    uart_init(9600);    // init uart
    uart_set_stdin();   // as std input

    spi_init();
    ST7735_init(TFT_CS, TFT_A0, TFT_RST, &TFT_PORT);
    ST7735_set_stdout();    // set display as std out
    ST7735_fill_screen(color_565(0, 0, 168));   // clear screen

    ST7735_set_text_color(0xFFFF);
    ST7735_set_text_bg_color(color_565(0, 0, 168));
    ST7735_set_cursor(0, 0);
    ST7735_wrap_text(true);

    printf_P(PSTR("Hello there! Type \"help\" to get help =)\n"));

    cmd();  // command line

    printf_P(PSTR("Now you can disconnect.\n"));
    uart_end();
}
