#include "drivers/input.h"
#include "io.h"
#include "drivers/irq.h"
#include "console.h"
#include "heap.h"
#include "string.h"

#define MOUSE_IRQ 12

#define PS2_CMD_PORT    0x64
#define PS2_DATA_PORT   0x60

#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_WRITE_AUX  0xD4

 
#define MOUSE_RESET        0xFF
#define MOUSE_DEFAULT      0xF6
#define MOUSE_ENABLE       0xF4
#define MOUSE_SET_SAMPLE   0xF3

static struct input_dev *mouse_dev;
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[4];  
static int mouse_pkt_idx = 0;

static inline void ps2_wait_write(void) {
    int timeout = 100000;
    while ((inb(PS2_CMD_PORT) & 2) && timeout--);
}

static inline void ps2_wait_read(void) {
    int timeout = 100000;
    while (!(inb(PS2_CMD_PORT) & 1) && timeout--);
}

static void ps2_write_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_CMD_PORT, cmd);
}

static void ps2_write_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}

static void mouse_write(uint8_t write) {
    ps2_write_cmd(PS2_CMD_WRITE_AUX);
    ps2_write_data(write);
    
    ps2_wait_read();
    uint8_t ack = inb(PS2_DATA_PORT);
     
}

static irqreturn_t mouse_handler(int irq, void *dev_id) {
    (void)irq;
    (void)dev_id;
    
    uint8_t status = inb(PS2_CMD_PORT);
    if (!(status & 0x20)) {
         
        return IRQ_NONE;
    }
    
    uint8_t data = inb(PS2_DATA_PORT);
    
    mouse_byte[mouse_pkt_idx++] = data;
    
    if (mouse_pkt_idx == 3) {
        mouse_pkt_idx = 0;
        
        uint8_t flags = mouse_byte[0];
        int x = mouse_byte[1];
        int y = mouse_byte[2];

        if (flags & 0xC0) return IRQ_HANDLED;

        int rel_x = x - ((flags << 4) & 0x100);
        int rel_y = y - ((flags << 3) & 0x100);

        int left = flags & 1;
        int right = (flags >> 1) & 1;
        int middle = (flags >> 2) & 1;
        
        input_report_rel(mouse_dev, REL_X, rel_x);
        input_report_rel(mouse_dev, REL_Y, -rel_y);  
        
        input_report_key(mouse_dev, 0x110, left);  
        input_report_key(mouse_dev, 0x111, right);  
        input_report_key(mouse_dev, 0x112, middle);  
        
        input_sync(mouse_dev);
    }
    
    return IRQ_HANDLED;
}

void mouse_init(void) {
    mouse_dev = (struct input_dev*)kmalloc(sizeof(struct input_dev));
    memset(mouse_dev, 0, sizeof(struct input_dev));
    mouse_dev->name = "PS/2 Mouse";

    mouse_dev->evbit[0] = (1 << EV_KEY) | (1 << EV_REL);
    mouse_dev->relbit[0] = (1 << REL_X) | (1 << REL_Y);
    
    input_register_device(mouse_dev);
    
    ps2_write_cmd(PS2_CMD_ENABLE_AUX);
    ps2_write_cmd(0x20);  
    ps2_wait_read();

    uint8_t status = inb(PS2_DATA_PORT);
    status |= 2;  
    ps2_write_cmd(0x60);  
    ps2_write_data(status);
    
    mouse_write(MOUSE_DEFAULT);
    mouse_write(MOUSE_ENABLE);

    request_irq(MOUSE_IRQ, mouse_handler, 0, "ps2_mouse", mouse_dev);
    kprint_str("PS/2 Mouse Initialized\n");
}
