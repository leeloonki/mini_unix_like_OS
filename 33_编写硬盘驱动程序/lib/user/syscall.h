#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"
enum SYSCALL_NR{
    SYS_GETPID,   //获取pid的子功能号
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE
};

uint32_t getpid();//用户接口函数
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* ptr);
#endif