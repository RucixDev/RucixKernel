#include "console.h"
#include "io.h"

#define COM1 0x3F8

static volatile uint16_t* video_memory = (volatile uint16_t*)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;

static void serial_early_init() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}

void kprint_init() {
    serial_early_init();
    kprint_clear();
}

void kprint_clear() {
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i] = (uint16_t)(' ' | (0x0f << 8));
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void scroll() {
    if (cursor_y >= 25) {
        for (int i = 0; i < 80 * 24; i++) {
            video_memory[i] = video_memory[i + 80];
        }
        for (int i = 80 * 24; i < 80 * 25; i++) {
            video_memory[i] = (uint16_t)(' ' | (0x0f << 8));
        }
        cursor_y = 24;
    }
}

void kprint_putc(char c) {
    serial_putc(c);
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = 79;
        }
    } else {
        video_memory[cursor_y * 80 + cursor_x] = (uint16_t)(c | (0x0f << 8));
        cursor_x++;
        if (cursor_x >= 80) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    scroll();
}

void kprint_str(const char* str) {
    while (*str) {
        kprint_putc(*str++);
    }
}

void kprint_hex(uint64_t value) {
    kprint_str("0x");
    for (int i = 15; i >= 0; i--) {
        int nibble = (value >> (i * 4)) & 0xF;
        char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        kprint_putc(c);
    }
}

void kprint_hex_byte(uint8_t value) {
    int nibble_hi = (value >> 4) & 0xF;
    int nibble_lo = value & 0xF;
    kprint_putc((nibble_hi < 10) ? ('0' + nibble_hi) : ('A' + nibble_hi - 10));
    kprint_putc((nibble_lo < 10) ? ('0' + nibble_lo) : ('A' + nibble_lo - 10));
}

void kprint_dec(uint64_t value) {
    if (value == 0) {
        kprint_putc('0');
        return;
    }
    
    char buffer[21];
    int i = 0;
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    while (i > 0) {
        kprint_putc(buffer[--i]);
    }
}

void kprint_newline() {
    kprint_putc('\n');
}
