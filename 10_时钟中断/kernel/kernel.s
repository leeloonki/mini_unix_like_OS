; ----------------------------------1. 编写中断处理程序-----------------------------
; 编写33个中断(异常)处理程序,对应33个中断号  ,根据Linux中断类型号分配:
;       从0~31 的向量对应于异常和非屏蔽中断。
;       从32~47 的向量（即由I/O 设备引起的中断，共16个，对应两个8259A级联所通产生的最大中断个数）分配给可屏蔽中断。
; 这里只设置 0-31号 对应32个系统中断(异常)处理程序 + 32号 对应 待实现的时钟中断 
; 中断(异常)处理程序为简单的打印字符串intr_str作为中断(异常)处理逻辑
[bits 32]
; 以下两行指令实现栈顶格式一致
%define ERROR_CODE  nop                 ;会自动压入错误码的异常 入栈不做操作
%define ZERO push 0                     ;无错误码的异常，进入中断函数首先push 0，统一栈顶
extern put_str                          ;声明外部函数

section .data
intr_str db "interrupt occur!",0xa,0  ;ASCII = 0xa换行,ASCII= 0 字符串结尾标志;该字符串被put_str输出
global intr_entry_table                 ; 中断(异常)处理程序表
intr_entry_table:
; 对中断处理程序编写宏进行定义

%macro VECTOR 2 
section .text
intr%1entry:                            ;%1对应宏VECTOR 第一个参数 %2对应第二个参数

    ;部分中断(异常)会压入错误码error_code,部分不会, 造成栈指针不同,
    ; 中断返回时iret指令执行时,栈顶指针必须为eip
    ; 为屏蔽有错误码和无错误码中断栈顶指针不一致,
    ; 已知哪些中断会压入errorcode,对于那些无errocode的中断
    ; 我们手动向栈中压入一个数,实现有无错误码的中断(异常)栈顶位置一致
    %2
    ; 中断处理逻辑    
    push intr_str
    call put_str
    add esp,4

    ; 后续将在设置8259中断控制器时指定手动结束标志,这里通过向8259写入EOI中断处理结束标记
    ; 向8259A发送一个EOI，其对应的OCW2的值为0x20
    mov al,0x20
    out 0xa0,al                         ;从片ocw2 端口0xa0 主片ocw2端口0x20
    out 0x20,al
    add esp,4                           ;跨过error_code,将栈顶指向eip
    iret

section .data                           ;经链接后该.data和上面intr_entry_table所在.data合并
    dd  intr%1entry                     ;存储该中断入口地址 ,其中第一个dd空间紧邻intr_entry_table
%endmacro


VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ZERO
VECTOR 0x09, ZERO
VECTOR 0x0a, ZERO
VECTOR 0x0b, ZERO
VECTOR 0x0c, ZERO
VECTOR 0x0d, ZERO
VECTOR 0x0e, ZERO
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ZERO
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1a, ZERO
VECTOR 0x1b, ZERO
VECTOR 0x1c, ZERO
VECTOR 0x1d, ZERO
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO




