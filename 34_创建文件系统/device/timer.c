#include "timer.h"
#include "print.h"
#include "stdint.h"
#include "io.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "global.h"
#define IRQ0_FREQUENCY  100             //中断频率100ms
#define INPUT_FREQUENCY 1193181
#define COUNTER0_VALUE  INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT   0x40
#define COUNTER_NO      0
#define COUNTER0_MODE   2       //方式2 可以自动连续工作，输出固定频率的脉冲信号
#define READ_WRITE_LATCH    3   //4-5位读写方式:先读低8位后高8
#define PIT_COUNTROL_PORT   0x43

#define mil_seconds_per_intr 1000/IRQ0_FREQUENCY    //该宏：多少毫秒发生一次中断  10ms

uint32_t ticks;         //记录内核自中断启动开始的总的滴答数

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


static void intr_timer_handler(){
    struct task_struct* cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x20000000);
    cur_thread->elapsed_ticks++;
    ticks++;
    if(cur_thread->ticks == 0){
        // put_str("time interrupt!!!\n");
        schedule();
    }else{
        cur_thread->ticks--;
    }
}

// 以ticks为单位的sleep
static void ticks_to_sleep(uint32_t sleep_ticks) {
   uint32_t start_tick = ticks;
   while (ticks - start_tick < sleep_ticks) {	   // 若间隔的ticks数不够便让出cpu
      thread_yield();
   }
}

// 以ms为单位的sleep
void mtime_sleep(uint32_t m_seconds) {
  uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
  ASSERT(sleep_ticks > 0);
  ticks_to_sleep(sleep_ticks); 
}
// 初始化8253
void timer_init(){
    put_str("timer_init start\n");

    // 参数 
    frequency_set(COUNTER0_PORT,COUNTER_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
    register_handler(0x20,intr_timer_handler);
    put_str("timer_init done\n");
}

