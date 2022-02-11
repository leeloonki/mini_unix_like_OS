
#include "stdint.h"
#include "thread.h"
#include "print.h"
#include "syscall.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#define syscall_nr  32      //子功能数
typedef void* syscall;
syscall syscall_table[syscall_nr];

// 系统调用syscall_handler汇编手动压入本文件的sys_func函数的参数
uint32_t sys_getpid(){
    return running_thread()->pid;
}

uint32_t sys_write(char* str){
    console_put_str(str);
    return strlen(str);
}

void syscall_init(){
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;  //在memory.h中声明该函数
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}