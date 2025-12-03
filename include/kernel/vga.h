// screen.h
#ifndef SCREEN_H
#define SCREEN_H

#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdint.h"

#define VGA_TEXT_MODE_BUFFER 0xB8000
#define VGA_COLS             80
#define VGA_ROWS             25
#define VGA_ATTR_WHITE_ON_BLACK 0x07

void screen_clear(void);
void screen_put_char(char c);
void screen_put_uint64(uint64_t n);
void screen_print(const char *s);
void screen_println(const char *s);

#endif // SCREEN_H
