// 封装对端口操作的的函数

#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

// 向端口port写入一个字符
static inline void outb(uint16_t port,uint8_t data){
    // data ：a寄存器约束 指定 al\ax\eax ; port N立即数约束,d 寄存器约束 指定 dl\dx\edx   
    // %0 b约束 指定al %1 w约束指定dx
    asm volatile ( "outb %b0 ,%w1" : : "a" (data), "Nd" (port));  
}

// 将addr起始的word_cnt个字写入端口port
static inline void outsw(uint16_t port,const void *addr,uint32_t word_cnt){
    // loader加载进入保护模式后已将ds es ss段选择子置为0,addr为偏移地址 
    // +S esi约束,且esi即做输入，也做输出 +c 指定ecx同理
    asm volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));  
}


// 获取从端口读入的一个字节
static inline int8_t inb(uint16_t port){
    uint8_t data;
    // 输入参数port 约束 立即数 edx
    // 输出参数data 约束 eax
    asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));  
    return data;
}


// 获取从端口读入的word_cnt个字写入addr
static inline void insw(uint16_t port,void *addr,uint32_t word_cnt){
    // addr 约束寄存器EDI 
    asm volatile ("cld; rep insw" : "+D"(addr), "+c" (word_cnt) : "d" (port) : "memory");  
}
#endif
