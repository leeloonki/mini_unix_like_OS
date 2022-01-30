[bits 32]
section .text
global switch_to
switch_to:
    push esi
    push edi
    push edx
    push ebp
    mov eax,[esp+20]    ;获取cur
    mov [eax],esp       ;cur是PCB指针，[eax]为偏移PCB为0的地址，也就是 self_kstack成员

    mov eax,[esp +24 ]  ;nextPCB的self_kstack成员 ，即获取到下一个进程的内核栈栈顶指针位置
    mov esp,[eax]
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret                 ;switch_to返回到schedule时恢复其使用的这4个寄存器