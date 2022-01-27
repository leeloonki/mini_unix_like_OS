
#include "keyboard.h"
#include "print.h"
#include "io.h"
#include "interrupt.h"
#include "stdint.h"
#define KBD_BUF_PORT 0x60       //Data  Port

static void intr_keyboard_handler(){
    uint8_t scancode = inb(KBD_BUF_PORT);          //读取输出缓冲区寄存器，否则8042不再继续响应键盘中断
    put_int(scancode);
    return;
}

void keyboard_init(){
    put_str("keyboard init start\n");
    register_handler(0x21,intr_keyboard_handler);
    put_str("keyboard init done\n");
}
