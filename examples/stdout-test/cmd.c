#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdio.h>

#include <uart.h>
#include <ST7735.h>

void cmd_help(void) {
    printf_P(PSTR(
        "Commands list:\n"
        " - help:    display this message\n"
        " - cls:     clear screen/display\n"
        " - invert:  invert display colors\n"
        " - sp:      Stack Pointer\n"
        " - reboot:  reboot system/device\n"
        " - exit:    quit and halt system\n"
    ));
}

void cmd_cls(void) {
    ST7735_fill_screen(tft_text_bg_color);
    ST7735_set_cursor(0, 0);
    ST7735_draw_fill_circle_Mich(63, -35, 100, color_565(92, 0, 0));
}

void cmd_reboot(void) {
    printf_P(PSTR("Reboot in  "));
    for (char i = 3; i > 0; i--) {
        printf_P(PSTR("\b%d"), i);
        _delay_ms(1000);
    }
    printf_P(PSTR("\b%d\n"), 0);
    _delay_ms(100);
    uart_end();
    asm volatile("jmp 0"::);
}
