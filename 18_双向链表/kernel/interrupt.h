#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
void idt_init();

// 定义枚举类型的中断状态
enum intr_status{
    INTR_OFF,       //关中断 IF = 0
    INTR_ON         //开中断 IF = 1
};
enum intr_status intr_disable();
enum intr_status intr_enable();
enum intr_status intr_get_status();
enum intr_status intr_set_status(enum intr_status status);
#endif
