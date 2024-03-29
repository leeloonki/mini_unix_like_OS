#include "interrupt.h"
#include "print.h"
// ASSERT断言成立时打印输出 文件名、行号、函数名、ASSERT的输入条件表达式
void panic_spin(char *filename,int line,const char*func,const char*condition){
    // 首先关中断
    intr_disable();
    put_str("\n\n\n!!! ERROR !!!\n");
    put_str("filename:");put_str(filename);put_str("\n");
    put_str("line:0x");put_int(line);put_str("\n");
    put_str("function:");put_str((char *)func);put_str("\n");
    put_str("condition:");put_str((char *)condition);put_str("\n");
    while(1);
}