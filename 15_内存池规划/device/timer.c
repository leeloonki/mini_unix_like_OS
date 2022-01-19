    // 任务：将时钟中断频率提高到100hz
    // 1.向8253控制寄存器端口写控制字，指定8253内部的计数器、该计数器工作方式
    // 2.向计数器端口写计数初值
#include "timer.h"
#include "print.h"
#include "stdint.h"
#include "io.h"

#define IRQ0_FREQUENCY  100
#define INPUT_FREQUENCY 1193181
#define COUNTER0_VALUE  INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT   0x40
#define COUNTER_NO      0
#define COUNTER0_MODE   2       //方式2 可以自动连续工作，输出固定频率的脉冲信号
#define READ_WRITE_LATCH    3   //4-5位读写方式:先读低8位后高8
#define PIT_COUNTROL_PORT   0x43

// 设置8253
// 参数:控制端口：计数器端口、计数器编号、读写性质、计数器工作模式
//     :数据端口：计数初值
static void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value){
    uint8_t cr = (uint8_t)(counter_no << 6 | rwl <<4 | counter_mode << 1); 
    // 写控制字
    outb(PIT_COUNTROL_PORT,cr);
    // 写低8位
    outb(counter_port,(uint8_t)counter_value);
    // 写高8
    outb(counter_port,(uint8_t)(counter_value >> 8));
}

// 初始化8253
void timer_init(){
    put_str("timer_init start\n");
    // 参数 
    frequency_set(COUNTER0_PORT,COUNTER_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
    put_str("timer_init done\n");
}

