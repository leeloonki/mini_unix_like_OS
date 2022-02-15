#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char *filename,int line,const char* func,const char* condition);

#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__);    //__FILE__等为编译器定义的宏可直接使用
// ASSERT宏定义

// ASSERT是程序开发时使用,在预处理阶段处理通过编译时是否指定NDEBUG来决定是否将ASSERT编译到程序中
#ifdef NDEBUG
    #define ASSERT(condition) ((void)0)                                 //如果没有开启编译的 NDEBUG宏, 断言什么也不做 ((void)0) = NULL
#else
    #define ASSERT(condition)       \
        if (condition) {} else{     \
            PANIC(#condition);      \
        }               
#endif

#endif