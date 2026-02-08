#include "gdt.h"

struct gdt_entry gdt[7];
struct gdt_descriptor gdtr;
struct tss_entry tss;

static uint8_t ist1_stack[4096] __attribute__((aligned(16)));

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (granularity & 0xF0);
    gdt[num].access = access;
}

void tss_init() {
    uint64_t tss_base = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss) - 1;
    
    uint8_t* p = (uint8_t*)&tss;
    for(uint32_t i=0; i<sizeof(tss); i++) p[i] = 0;

    tss.iopb_offset = sizeof(tss);

    tss.ist1 = (uint64_t)&ist1_stack[4096];

    gdt_set_gate(5, (uint32_t)tss_base, (uint32_t)tss_limit, 0x89, 0x00); 
    
    struct gdt_entry* high = &gdt[6];
    high->limit_low = (uint16_t)((tss_base >> 32) & 0xFFFF);
    high->base_low = (uint16_t)((tss_base >> 48) & 0xFFFF);
    high->base_middle = 0;
    high->access = 0;
    high->granularity = 0;
    high->base_high = 0;
}

void gdt_init() {
    gdtr.size = (sizeof(struct gdt_entry) * 7) - 1;
    gdtr.offset = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);
    gdt_set_gate(2, 0, 0, 0x92, 0x00);
    gdt_set_gate(3, 0, 0, 0xF2, 0x00);
    gdt_set_gate(4, 0, 0, 0xFA, 0x20);

    tss_init();

    __asm__ volatile("lgdt %0" : : "m"(gdtr));
    __asm__ volatile("ltr %%ax" : : "a"(0x28));
}

void tss_set_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
