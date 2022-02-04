#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "process.h"
#include "sync.h"

#define PG_SIZE 4096

struct task_struct*main_thread; //主线程PCB
struct list thread_ready_list;  //就绪队列
struct list thread_all_list;    //所有任务队列#define PG_SIZE 4096
static struct list_elem* thread_tag;//PCB中 对应队列的结点信息

// 进程或线程的pid全局唯一，属于临界资源需要互斥访问
struct lock pid_lock;


extern void switch_to(struct task_struct*cur,struct task_struct* next);

static void kernel_thread(thread_func* func,void* func_arg){
    // 新线程开始后需要打开中断,避免后面的时钟中断被屏蔽，无法调度到其他进程
    intr_enable();
    func(func_arg);
}

static pid_t allocate_pid(){
    static pid_t next_pid = 0;
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);
    return next_pid;
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
    pthread->pid = allocate_pid();
    strcpy(pthread->name,name);
    if(pthread==main_thread){
        pthread->status =TASK_RUNNING;    
    }else{
        pthread->status = TASK_READY;
    }
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); //初始化内核栈栈顶
    pthread->priority = prio;
    pthread->ticks = prio;    //优先级越大，tick越大
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;      //线程没有页表
    pthread->stack_magic = PG_SIZE / 2 ;                    //任意
}


// 线程名 优先级 线程执行函数 及其参数
struct task_struct* thread_start(char* name,int prio,thread_func function,void* func_arg){
    struct task_struct* thread = get_kernel_pages(1);   //申请一页做PCB thread为该PCB起始
    init_thread(thread,name,prio);                      //初始化进程（线程）PCB
    thread_create(thread,function,func_arg);            //创建一线程（函数执行流）
    //此时 thread->self_kstack为线程栈栈顶
    // 内嵌汇编将线程栈出栈到线程栈内存结构体中
    // asm volatile("movl %0,%%esp; pop %%ebp ; pop %%ebx; pop %%edi; pop %%esi; ret ":: "g" (thread->self_kstack) : "memory");
    
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);  
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    return thread;
}


// 获取当前线程的PCB
// 线程切换一定在内核态下运行，内核态使用的栈一定为0级内核栈，内核栈就位于当前线程PCB顶端
struct task_struct* running_thread(){
    // 获取当前线程栈顶指针，将该栈顶指针低12位置为0就是该PCB页起始地址
    uint32_t esp;
    asm ("mov %%esp,%0" : "=g"(esp));
    return (struct task_struct* )(esp & 0xfffff000);

}

// 内核中main.c中的main函数既是第一个内核线程，也是内核主线程其栈顶在loader.c中被设置为0xc009f000 该栈顶刚好位于一页的顶端
// 我们将该页初始化为PCB，完善main线程的PCB
static void make_main_thread(){
    // 获取main函数PCB地址，不用像创建线程一样为其分配一页物理内存作为PCB
    main_thread = running_thread();
    // 初始化main_thread
    init_thread(main_thread,"main",30);


    // main线程不在all_list_thread中
    ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));
    list_append(&thread_all_list,&main_thread->all_list_tag);
}

void schedule(){
    // 调度时关闭中断
    // put_str("schedule\n");
    ASSERT(intr_get_status()==INTR_OFF);
    struct task_struct* cur = running_thread();
    if(cur->status == TASK_RUNNING){
        // 时间片到
        ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
        list_append(&thread_ready_list,&cur->general_tag);  
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    }else{
        // 阻塞
    }
    ASSERT(!list_empty(&thread_ready_list)); //有可供调度运行的进程
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);// next进程的tag
    struct task_struct* next = elem2entry(struct task_struct,general_tag,thread_tag);//获取下个进程的PCB指针
    next->status = TASK_RUNNING;            //调度该进程，改变该进程状态

    //如果是进程则更新页表
    process_activate(next);
    
    switch_to(cur,next);
}

// 当前线程将自己阻塞，标志其状态为TASK_BLOCKED, TASK_WAITING,TASK_HANGING,
// thread_block
void thread_block(enum task_status stat){
    // 线程阻塞只有这三种情况
    ASSERT(((stat==TASK_BLOCKED) || (stat==TASK_WAITING) || (stat==TASK_HANGING)));
    enum intr_status old_status = intr_disable(); //关中断
    struct task_struct* cur_thread = running_thread(); //获取当前线程、将其状态置为stat
    cur_thread->status = stat;
    schedule();
    intr_set_status(old_status);
}

// 将线程pthread 唤醒
void thread_unblock(struct task_struct* pthread){
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status==TASK_BLOCKED) || (pthread->status==TASK_WAITING) || (pthread->status==TASK_HANGING)));
    // 将该进程设为就绪态
    if(pthread->status!=TASK_READY){
        // 断言：当前线程不在就绪队列
        ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
        if(elem_find(&thread_ready_list,&pthread->general_tag)){
            PANIC("thread_unblock: blocked thread in ready list\n");
        }
        // 将当前线程加入到就绪队列
        list_push(&thread_ready_list,&pthread->general_tag);
        pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

// 初始化线程环境
void thread_init(){
    put_str("thread_init start\n");
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    lock_init(&pid_lock);
    make_main_thread();
    put_str("thread_init done\n");
}