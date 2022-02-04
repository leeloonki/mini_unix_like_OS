#include "syscall.h"
#include "stdint.h"


// NUMBER   系统调用子功能号
// ARG      系统调用子功能参数

// 无参数的系统调用
#define _syscall0(NUMBER) ({\
    int ret;                \
    asm volatile (          \
        "int $0x80"         \
        : "=a" (ret)         \
        : "a" (NUMBER)       \
        : "memory"           \
    );                      \
    ret;                    \
})


// 1参数的系统调用
#define _syscall1(NUMBER,ARG1) ({\
    int ret;                    \
    asm volatile (              \
        "int $0x80"             \
        :"=a" (ret)             \
        :"a" (NUMBER),"b" (ARG1)                \
        :"memory"               \
    );                          \
    ret;                        \
})


// 2参数的系统调用
#define _syscall2(NUMBER,ARG1,ARG2) ({  \
    int ret;                            \
    asm volatile (                      \
        "int $0x80"                     \
        : "=a" (ret)                     \
        : "a" (NUMBER),"b" (ARG1),"c"(ARG2)                 \
        : "memory"                       \
    );                                  \
    ret;                                \
})



// 3参数的系统调用
#define _syscall3(NUMBER,ARG1,ARG2,ARG3) ({\
    int ret;                               \
        asm volatile (                     \
        "int $0x80"                        \
        :"=a" (ret)                         \
        :"a" (NUMBER),"b" (ARG1),"c"(ARG2),"d"(ARG3)        \
        :"memory"                           \
    );                                      \
    ret;                                    \
})

uint32_t getpid(){
    return _syscall0(SYS_GETPID);
}
