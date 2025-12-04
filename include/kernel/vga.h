// screen.h
#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

#define VGA_TEXT_MODE_BUFFER 0xB8000
#define VGA_COLS             80
#define VGA_ROWS             25
#define VGA_ATTR_WHITE_ON_BLACK 0x07

void screen_clear(void);
void screen_put_char(char c);
void screen_put_uint64(uint64_t n);
void screen_print(const char *s);
void screen_println(const char *s);
void screen_put_hex8(uint8_t v);
int screen_printf(const char *fmt, ...);


#endif // SCREEN_H
