%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR  ; loader加载到的内存地址 0x900
LOADER_STACK_TOP equ LOADER_BASE_ADDR   ; 设置栈顶 0x900
jmp loader_start_16

; -------------------------------------------------
; 创建GDT描述符表
; -------------------------------------------------
    ; GDT每个表项8个字节
    ; 每个描述符 低四字节中最低二位为段界限 次低两字节为16位段基址
    ; 保护模式下，采用平坦模式 段基址定义为0
    
    GDT_BASE:   dd 0x00000000           ; GDT表中第0描述符GDT_BASE不使用
                dd 0x00000000
    CODE_DESC:  dd 0x0000ffff           ; 代码段描述符。低四字节segment base 16..0 = 0
                dd DESC_CODE_HIGH4      ; CODE(executable)段描述符高四字节
    ; G位 = 1,粒度4Kb，32位地址线寻址范围0-0xffffffff,段基址=0
    ; segment limit = [(0xffffffff-0+1)b = 4Gb] / 4Kb = 1Mb = 2^20 = 0xfffff
    ; segment limit 15..0 = 0xffff,segment limit 19..16 = 0x0f

    DATA_STACK_DESC:    
                dd 0x0000ffff
                dd DESC_DATA_HIGH4      ; 数据段和栈段共用一个段

    VIDEO_DESC: dd 0x80000007           ; 0x0b 8000
                dd DESC_VIDEO_HIGH4
    ; 进入保护模式后为直接操纵显存，对显存所在段 不采用平坦模式，直接将段基址设为文本模式下显存的起始地址0xb8000
    ; 段大小为(0xbffff-0xb8000+1) = 0x7000b = 7*2^12b,将G位设1,粒度4Kb 则segment limit = 7*2^12b/4kb = 7
    ; 即VIDEO_DESC中 segment limit 15..0 为0x0007
    ; segment base = 0xb 8000,segment base 15..0 为0x8000
    ; base  23..16 = 0x0b
    ; limit 19..16 = 0x00

    GDT_SIZE    equ $ - GDT_BASE        ;GDT占字节数
    GDT_LIMIT   equ GDT_SIZE - 1        ;GDT以字节的偏移值
    times 30 dq 0                       ;预留30个描述符

; -------------------------------------------------
; 初始化段选择子
; -------------------------------------------------
    SELECTOR_CODE   equ (0x0001<<3) + TI_GDT + RPL0     
    SELECTOR_DATA   equ (0x0002<<3) + TI_GDT + RPL0
    SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0
    ;0x0001 = 0000 0000 0000 0001 <<3 = 0000 0000 0000 1000 选择子占前13位
        
    ;     TARGET SEGMENT SELECTOR 
    ;  +-----------------------+-+----+
    ;  |         INDEX         |TI|RPL|
    ;  +-----------------------+-+----+ 
    ;   properties  position    function
    ;   INDEX       15..3       select a segment
    ;   TI          2           TI=0 search GDT;TI=1 search LDT 
    ;   RPL         1..0        request privilege 00 01 10 11

    loadermsg   db 'loader in real mode'

    ; 以下两行供48位寄存器GDTR加载GDT表使用
    gdt_ptr     dw GDT_LIMIT            ;16位GDT以字节的界限
                dd GDT_BASE             ;GDT的32位起始地

; -------------------------------------------------
; 实模式下输出字符
; 使用INT 0x10  功能号AH = 0x13 子功能:打印字符串
; -------------------------------------------------


loader_start_16:
    mov sp,LOADER_BASE_ADDR
    mov bp,loadermsg        ;字符串地址es:bp
    mov cx,19   
    mov ax,0x1301           ;ah = 0x13 al = 01h
    mov bx,0x001f           ;页号 BH= 0
    mov dx,0x1800           ;行数 dh = 0x18(16+8=24 文本模式下屏幕25*80) 列数 0x00 
                            ;该字符串将出现在屏幕最后一行行首起始处
    int 0x10


; -------------------------------------------------
; 准备进入保护模式
; 1. 打开A20总线
; 2.加载GDT
; 3.设置CR0 PE位
; -------------------------------------------------

; 1.打开A20地址线
    in al,0x92                 ;将0x92端口第1位(下标0开始),置1即可
    or al,0x02
    out 0x92,al

; -------------------------------------------------
; 2.加载GDT
    lgdt [gdt_ptr]

; -------------------------------------------------
; 3.设置CR0寄存器PE位
    mov eax,cr0
    or eax,0x00000001                 ;cr0 0位置1
    mov cr0,eax

    jmp dword SELECTOR_CODE:loader_print_32

; -------------------------------------------------
; 保护模式下输出字符
; -------------------------------------------------

[bits 32]
loader_print_32:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax
	mov byte [gs:160],'p'
    jmp $