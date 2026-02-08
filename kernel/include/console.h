#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void kprint_init();
void kprint_clear();
void kprint_str(const char* str);
void kprint_hex(uint64_t value);
void kprint_hex_byte(uint8_t value);
void kprint_dec(uint64_t value);
void kprint_newline();

#endif
