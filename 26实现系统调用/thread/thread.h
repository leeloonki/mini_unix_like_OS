#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "memory.h"
#include "stdint.h"

typedef int16_t pid_t;

// 进程(线程)状态
enum task_status{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 线程中断栈 intr_stack
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
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

// 通用线程函数(线程 entry)
typedef void thread_func(void*);

// 线程栈
// 该线程栈(内存数据结构)用于线程指令流初始化，线程间切换调度时保存原指令流执行位置
struct thread_stack{
    // schedule调度switch_to时 被调函数使用这几个寄存器，switch_to执行时保护这四个寄存器
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    
    // eip指针指向线程指令流起始函数处
    // 定义四个指针变量
    void(*eip)(thread_func* func,void* func_arg);

    //以下三个栈元素将该线程做成可以被调度的样子，
    // 分别为 （没用的）返回地址、 线程函数、线程函数参数,
    // 当别的线程发生时钟中断调度该线程时，通过pop将该线程栈帧元素ebp、ebx、edi、esi弹栈
    // (调度器以为这是在恢复该线程执行schedule->swithc_to时压入的那四个寄存器(当线程不是第一次被调度时的流程)
    // 然后栈顶在eip 再通过ret eip被指为该线程栈中eip即新线程的入口地址，新线程开始执行。
    // 补充：若线程执行过程中，发生时钟中断，进入时钟中断处理函数，该函数判断是否调度，若调度，则调度switch_to
    // switch_to将当前线程的上述四个寄存器压栈保存，同时保存栈顶指针，然后将换上cpu的这个线程的四个寄存器出栈
    // （之所以出栈，是因为这个线程上次在switch_to被换下CPU的），
    // 时钟中断 -> intr0x20entry -> intr_timer_handler -> schedule -> switch_to 
    // switch_to 执行完返回schedule 继续返回 intr_timer_handler 继续返回 中断处理函数 将栈中元素全部弹出，此时内核栈为空
    uint32_t* ret_addr;
    thread_func* function;
    void* func_arg;
};

struct task_struct{
    uint32_t* self_kstack;  //内核栈 该成员在PCB task_struct 偏移为0处
    pid_t pid;
    enum task_status status;
    uint8_t priority;       //优先级
    char name[16];          //线程名
    uint32_t stack_magic;   //栈边界标记，防止栈溢出破坏PCB

    uint8_t ticks;          //每次在处理器上执行的时间滴答数
    uint32_t elapsed_ticks; //任务在处理器上执行的总时间滴答数
    struct list_elem general_tag;           // 用来将PCB以链的形式串起来
    struct list_elem all_list_tag;          // 用来将PCB以链的形式串起来
    uint32_t* pgdir;                        //进程的页表虚拟地址（每个进程都有自己的内存地址的映射，都有自己的页表(页目录-页表)
    struct virtual_addr userprog_vaddr;     //该结构体用于管理用户进程自己的虚拟地址

};

extern struct list thread_ready_list;
extern struct list thread_all_list;


void thread_create(struct task_struct* pthread,thread_func function,void* func_arg);
void init_thread(struct task_struct* pthread,char*name,int prio);
struct task_struct* thread_start(char* name,int prio,thread_func function,void* func_arg);
struct task_struct* running_thread();
void schedule();
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_init();
#endif