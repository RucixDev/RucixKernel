#include "drivers/pit.h"
#include "console.h"
#include "io.h"

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    
    kprint_str("PIT Initialized\n");
}

uint16_t pit_read_count(void) {
    uint16_t count;
     
    outb(0x43, 0x00);
    count = inb(0x40);
    count |= (inb(0x40) << 8);
    return count;
}
