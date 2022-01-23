#include "memory.h"
#include "thread.h"
#include "string.h"
#define PG_SIZE 4096
static void kernel_thread(thread_func* func,void* func_arg){
    func(func_arg);
}

void thread_create(struct task_struct* pthread,thread_func function,void* func_arg){
    pthread->self_kstack -=sizeof(struct intr_stack);       //预留中断栈
    pthread->self_kstack -=sizeof(struct thread_stack);     //预留线程栈
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;//线程栈起始地址
    
    // 线程栈初始化
    kthread_stack->eip = kernel_thread;                     //线程切换或执行时执行ip指向的内存地址
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->edi = kthread_stack->esi = 0;
}

void init_thread(struct task_struct* pthread,char*name,int prio){
    memset(pthread,0,sizeof(*pthread));
    strcpy(pthread->name,name);
    pthread->status = TASK_RUNNING;
    pthread->priority = prio;
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); //初始化内核栈栈顶
    pthread->stack_magic = PG_SIZE / 2 ;                    //任意
}


// 线程名 优先级 线程执行函数 及其参数
struct task_struct* thread_start(char* name,int prio,thread_func function,void* func_arg){
    struct task_struct* thread = get_kernel_pages(1);   //申请一页做PCB thread为该PCB起始
    init_thread(thread,name,prio);                      //初始化进程（线程）PCB
    thread_create(thread,function,func_arg);            //创建一线程（函数执行流）

    //此时 thread->self_kstack为线程栈栈顶
    // 内嵌汇编将线程栈出栈到线程栈内存结构体中
    asm volatile("movl %0,%%esp; pop %%ebp ; pop %%ebx; pop %%edi; pop %%esi; ret ":: "g" (thread->self_kstack) : "memory");
    return thread;
}


