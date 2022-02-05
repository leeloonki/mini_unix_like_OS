#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "list.h"
#include "tss.h"
#include "interrupt.h"
#include "string.h"
#include "console.h"



#include "string.h"
#define PG_SIZE 4096

extern void intr_exit();
// 构建用户进程初始化上下文信息,该函数被switch_to返回到的kernel_thread调用
void start_process(void* filename_){//filename_用户程序名
    void* function = filename_;
    // 初始化中断栈intr_stack
    struct task_struct* cur = running_thread();
    // 我们这里时构造进程的上下文环境，而在thread_create中已经减去为创建线程预留的thread_stack的栈空间
    // 这里加上该struct thread_stack空间，使内核栈指针指向intr_stack
    cur->self_kstack += sizeof(struct thread_stack);//此时，self_kstack指向intr_stack栈之下
    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;//使proc_stack指向中断栈结构体intr_stack,方便进行初始化
    
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;//用户程序不使用gs，用户程序无法访问硬件显存段，选择子为0，若访问该段那么访问到gdt0项（空），引发异常
    proc_stack->fs = proc_stack->es = proc_stack->ds = SELECTOR_U_DATA; //设置段选择子为用户段
    proc_stack->eip = function;//eip为中断返回地址，当中断返回后ip被设置为function，使得进程得以执行
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_2 | EFLAGS_IF_1);
    proc_stack->esp = (void*)get_a_page(PF_USER,USER_STACK3_VADDR) + PG_SIZE ;//esp指向1页顶部，因此加PG_SIZE
    proc_stack->ss = SELECTOR_U_DATA;
    asm volatile("movl %0,%%esp;jmp intr_exit"::"g" (proc_stack) : "memory");//重设当前栈为该中断栈，在intr_exit进行出栈操作，弹出这里初始化的信息
}

// 激活页表，该函数是在switch_to后续被调的函数，在进行调度时，当前任务可能是线程也可能是进程
// 对于线程没有页表，因此不更新页表，如果是进程，则更新页表
void page_dir_activate(struct task_struct* p_thread){

    // 默认为内核的页目录物理地址
    uint32_t pagedir_phy_addr = 0x100000;
    if(p_thread->pgdir !=NULL){
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }
    // 将进程的页(目录)表物理地址赋给CR3寄存器
    asm volatile("movl %0,%%cr3" :: "r"(pagedir_phy_addr) : "memory");
}

// 激活进程页表并更新tss中0级栈为新进程PCB中的的0级栈
void process_activate(struct task_struct* p_thread){
    ASSERT(p_thread!=NULL);
    page_dir_activate(p_thread);
    if(p_thread->pgdir){
        update_tss_esp(p_thread);
    }
}


// 创建页目录表，由于3G-4G空间为内核空间，所有进程都一样
// 我们将当前进程的页目录项中的768-1023项拷贝到新进程的页表
uint32_t* create_page_dir(){
    uint32_t* page_dir_vaddr = get_kernel_pages(1);//用户进程的页表有操作系统管理，且从内核物理内存池分配
    if(page_dir_vaddr == NULL){
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
    }
    // 0x300 = 3*16^2 = 768  768*4即第768项页目录地址 共复制256项每项4字节
    // 将当前任务的页目录项768-1023复制到新进程的页表对应项处
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr+0x300*4),(uint32_t*)(0xfffff000 + 0x300*4),256*4);
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W |PG_P_1 ;//页目录项1023项为页表物理地址
    return page_dir_vaddr;
}

// 创建用户进程虚拟地址位图
void create_user_vaddr_bitmap(struct task_struct* user_prog){
    
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;//用户虚拟地址起始地址
    uint32_t bitmap_pg_cnt = (((0xc0000000 - USER_VADDR_START)/PG_SIZE/8) + PG_SIZE -1)/PG_SIZE;//用户虚拟池位图占用用户内存空间，该变量用来记录进程需要的内存页框数
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000- USER_VADDR_START) / PG_SIZE /8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

void process_execute(void* filename,char* name){
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread,name,default_prio);          //初始化task_struct
    create_user_vaddr_bitmap(thread);               //初始化进程位图
    thread_create(thread,start_process,filename);   //初始化线程切换栈
    thread->pgdir = create_page_dir();              //创建进程页表
    block_desc_init(thread->u_block_descs);
    //添加进程到内核任务队列中 
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);

    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_status(old_status);
}




