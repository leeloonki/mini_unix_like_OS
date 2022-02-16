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
extern idt_table

section .data
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
    
    ;硬件自动压入esp flags cs eip 
    %2

    ; 中断处理逻辑    
    ; 汇编调用c,保护寄存器上下文(段寄存器和8个通用寄存器)
    push ds
    push es
    push fs
    push gs
    pushad

    ; 发中断结束EOI
    ; 后续将在设置8259中断控制器时指定手动结束标志,这里通过向8259写入EOI中断处理结束标记
    ; 向8259A发送一个EOI，其对应的OCW2的值为0x20
    mov al,0x20
    out 0xa0,al                         ;从片ocw2 端口0xa0 主片ocw2端口0x20
    out 0x20,al

    ; 调用interrupt.c中的idt_table[]    ;%1 即中断号
    push %1                             ;通过idt_table[%1]调用
    call [idt_table + %1*4]
    jmp intr_exit 
    

section .data                           ;经链接后该.data和上面intr_entry_table所在.data合并
    dd  intr%1entry                     ;存储该中断入口地址 ,其中第一个dd空间紧邻intr_entry_table
%endmacro

section .text
global intr_exit
intr_exit:
    add esp,4                           ;跳过中断号  push%1
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp,4                           ;跳过栈顶error_code
    iretd

; 0x0- 0x22的中断处理函数  宏展开实现
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
VECTOR 0x20, ZERO           ;时钟中断对应入口  8259主片IR0引脚
VECTOR 0x21, ZERO           ;键盘中断对应入口  8259主片IR1引脚
VECTOR 0x22, ZERO           ;从片级联对应      8259主片IR2引脚
VECTOR 0x23, ZERO           ;COM2
VECTOR 0x24, ZERO           ;COM1
VECTOR 0x25, ZERO           
VECTOR 0x26, ZERO
VECTOR 0x27, ZERO
VECTOR 0x28, ZERO
VECTOR 0x29, ZERO
VECTOR 0x2a, ZERO
VECTOR 0x2b, ZERO
VECTOR 0x2c, ZERO           ;PS2 Mouse
VECTOR 0x2d, ZERO           ;FPU
VECTOR 0x2e, ZERO           ;ATA0 8259从片IR6Q引脚
VECTOR 0x2f, ZERO           ;ATA1 8259从片IRQ7引脚


; 0x80中断处理函数
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
    ; 1.保存上下文环境
    ; 0x80软中断并没有错误码压入，为和其他中断压入的栈的数据格式一致，我们首先压入0
    push 0 
    push ds
    push es
    push fs
    push gs
    pushad
    push 0x80

    ; 2 为系统调用子功能传入参数
    ; 不同的syscall参数并不一致 参数有可能0 1 2 3 为了实现方便我们对不同参数的系统调用，将参数存放的寄存器全部压入栈，
    ; 即按三个参数的系统调用压栈，根据c语言调用规则，参数从右向左压入，即先压入最右边的参数
    ; 对于三参数的系统调用 其参数分别在ebx ecx edx中，故我们先压入参数最右边的参数3
    ; syscall_handler汇编函数调用子功能c函数，手动压入c函数需要的参数edx ecx ebx
    push edx        ;压入参数3
    push ecx        ;压入参数2
    push ebx        ;压入参数1

    ; 3 调用子功能处理函数  我们系统调用子功能函数的入口地址存在syscall_table中，该表项为函数指针，每个表项4字节
    call [syscall_table + eax*4]
    add esp,12    ;越过栈中参数
    ; 4 call调用后返回值存于eax中，此时位于内核0级栈中，当iret返回到用户态后，寄存器中的值被pop出的值覆盖，因此我们将返回值写到0级栈中
    mov [esp + 8*4],eax
    jmp intr_exit
    