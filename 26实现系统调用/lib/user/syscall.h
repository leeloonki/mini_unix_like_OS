#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"
enum SYSCALL_NR{
    SYS_GETPID   //获取pid的子功能号
};

uint32_t getpid();//用户接口函数

#endif