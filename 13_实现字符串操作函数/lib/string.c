#include "stdint.h"
#include "debug.h"

// 将dst_起始的size个字节置为value
void memset(void* dst_, uint8_t value, uint32_t size){
    ASSERT( dst_ != NULL);                  //NULL为c编译器 对空指针的实现
    uint8_t* dst = (uint8_t*)dst_;          //dst无符号单字节整数类型数组
    while(size-- >0){
        *dst = value;
        dst++;
    }
}

// 将src_起始的size个字节复制到dst_
void memcpy(void* dst_, const void* src_, uint32_t size){
    ASSERT(dst_ != NULL && src_ !=NULL);
    uint8_t* dst = (uint8_t*)dst_;
    const uint8_t* src = (uint8_t*)src_;
    while(size-- > 0){
        *dst++ = *src++;
    }
}

// 比较a_ b_开头的size个字节 a_ > b_返回1；a_ <b_ 返回-1 相等返回0
int memcmp(const void* a_ , const void* b_ , uint32_t size){
    ASSERT(a_ != NULL && b_ != NULL);
    const uint8_t* a = (uint8_t*)a_;
    const uint8_t* b = (uint8_t*)b_;
    while(size-- >0){
        if(*a != *b){
            return *a > *b ? 1 : -1;
        }
        a++;
        b++;
    }
    return 0;
}

// 将src_开始的字符串复制到dst_开始的地址单元,返回目的地址dst_
char* strcpy(char* dst_, const char* src_ ){
    ASSERT(dst_!= NULL && src_ !=NULL){
        char *r = dst_;
        while(*dst_++ = *src_++);           // 当src起始地址的字符串复制到末尾时,将 '/0'即ASCII赋值给 *dst while(*dst) 条件为假退出循环
        return r;
    }
}

// 获取字符串长度
uint32_t strlen(const char* str){
    ASSERT(str != NULL );
    const char* p = str;
    uint32_t str_len = 0;
    while(*p++){
        str_len++;
    }
    return str_len;
}

// 比较a b起始的字符串，a中字符>b中字符 返回1 相等返回0 小于返回-1
int strcmp(const char * a_ , const char* b_){
    ASSERT(a!=NULL && b_ != NULL);
    while (*a_!='\0' && *b_!='\0' && *a==*b)
    {
        a_ ++;
        b_ ++;
    }
    if(*a_ == *b_){ return 0;}
    else{
        return *a_ >*b_ ? 1:-1 ;
    }
}

// 从左到右获取字符串中第一个ch字符的地址
char* strchr(const char* str_ , const uint8_t ch){
    ASSERT(str_!=NULL);
    while(*str_++!='\0'){
        if(*str_==ch){
            return (char*)str_;
        }
    }
    return NULL;
}

// 从右到左获取字符串中第一个ch字符的地址
char* strrchr(const char* str_ , const uint8_t ch){
    ASSERT(str_ != NULL);
    const char* last_char = NULL ;
    while (*str_++ !='\0'){   
        if(*str_==ch){
            last_char=str_;
        }
    }
    return (char*)last_char;    
}

// 字符串拼接,将字符串src拼接到dst_后
char* strcat(char* dst_, const char* src_){
    ASSERT(dst_ !=NULL && src_ !=NULL);
    char* str = dst_;
    while(*str++);
    --str;
    while(*str++ = *src_++);
    return dst_;
}

// 获取字符串中 ch字符出现的次数
uint32_t strchrs(const char* str_, const uint8_t ch){
    ASSERT(str!=NULL);
    uint32_t ch_cnt=0;
    while(*str_++ !='\0') {
        if(*str_==ch){
            ch_cnt++;
        }
    }
    return ch_cnt;
}