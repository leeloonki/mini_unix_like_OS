#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#include "string.h"
#include "stdint.h"
#include "debug.h"

// 将dst_起始的size个字节置为value
void memset(void* dst_, uint8_t value, uint32_t size);

// 将src_起始的size个字节复制到dst_
void memcpy(void* dst_, const void* src_, uint32_t size);

// 比较a_ b_开头的size个字节
int8_t memcmp(const void* a_ , const void* b_ , uint32_t size);

// 将src_开始的字符串复制到dst_,返回目的字符串起始地址dst_
char* strcpy(char* dst_, const char* src_ );

// 获取字符串长度
uint32_t strlen(const char* str);
// 比较a b起始的字符串
int8_t strcmp(const char * a_ , const char* b_);

// 从左到右获取字符串中第一个ch字符的地址
char* strchr(const char* str_ , const uint8_t ch);

// 从右到左获取字符串中第一个ch字符的地址
char* strrchr(const char* str_ , const uint8_t ch);

// 字符串拼接,将字符串src拼接到dst_后
char* strcat(char* dst_, const char* src_);

// 获取字符串中 ch字符出现的次数
uint32_t strchrs(const char* str_, const uint8_t ch);

#endif