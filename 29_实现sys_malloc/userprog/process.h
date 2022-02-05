#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H
#include "thread.h"
#include "stdint.h"
#define default_prio 30
#define USER_STACK3_VADDR (0xc0000000 - 0x1000) //用户三级栈位于用户空间最高一页
#define USER_VADDR_START 0x8048000

void start_process(void* filename_);
void page_dir_activate(struct task_struct* p_thread);
void process_activate(struct task_struct* p_thread);
uint32_t* create_page_dir();
void create_user_vaddr_bitmap(struct task_struct* user_prog);
void process_execute(void* filename,char* name);
#endif