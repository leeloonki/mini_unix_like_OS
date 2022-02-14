#include "stdio-kernel.h"
#include "stdio.h"      //va_list数据结构、vsprintf声明
#include "console.h"
#include "global.h"     //NULL定义
#define va_start(ap,last) ap=(va_list)&last     //format为char*类型，是栈中指向一个字符串的指针变量，获取format参数地址，将其转化为为va_list
#define va_end(ap) ap=NULL
// 供内核使用的格式化输出函数
void printk(const char*format,...){
    va_list args;
    va_start(args,format);      //args指向format
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    console_put_str(buf);
}