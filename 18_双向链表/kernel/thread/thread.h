#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"

// 进程状态
enum task_status{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 进程中断栈 intr_stack
// 该栈(内存区域)用来保存中断时进程或线程的上下文寄存器环境
struct intr_stack{
    // 该栈栈顶在PCB所在页的最高边界处
    // 注意变量定义与压栈时内存对应关系:该栈偏移越小处越位于内存低地址，越较后被压入栈
    uint32_t vec_no;        //kernel.s中intr%1entry压入的中断号     push %1
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
// CPU从低特权级到高特权级时
    uint32_t err_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

// 通用线程函数
typedef void thread_func(void*);

// 线程栈
// 该线程栈(内存数据结构)用于线程指令流初始化，线程间切换调度时保存原指令流执行位置
struct thread_stack{
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    
    // eip指针指向线程指令流起始函数处
    // 定义四个指针变量
    void(*eip)(thread_func* func,void* func_arg);
    uint32_t* ret_addr; //占位,ret返回时ip指向kernel_thread到kernel_thread执行，
    // 同时让kernel以为自己是被调用的，因此此时栈顶指针esp + 0处填入虚假的返回地址。
    // 占位，方便kernel在 esp+4 寻找参数
    thread_func* function;
    void* func_arg;
};

struct task_struct{
    uint32_t* self_kstack;  //内核栈
    enum task_status status;
    uint8_t priority;       //优先级
    char name[16];          //线程名
    uint32_t stack_magic;   //栈边界标记，防止栈溢出破坏PCB
};

void thread_create(struct task_struct* pthread,thread_func function,void* func_arg);
void init_thread(struct task_struct* pthread,char*name,int prio);
struct task_struct* thread_start(char* name,int prio,thread_func function,void* func_arg);

#endif