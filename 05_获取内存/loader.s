%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR  ; loader加载到的内存地址 0x900
LOADER_STACK_TOP equ LOADER_BASE_ADDR   ; 设置栈顶 0x900

; -------------------------------------------------
; 创建GDT描述符表
; -------------------------------------------------

    GDT_BASE:   dd 0x00000000           ; GDT表中第0描述符GDT_BASE不使用
                dd 0x00000000
    CODE_DESC:  dd 0x0000ffff          
                dd DESC_CODE_HIGH4   
    DATA_STACK_DESC:    
                dd 0x0000ffff
                dd DESC_DATA_HIGH4      ; 数据段和栈段共用一个段

    VIDEO_DESC: dd 0x80000007           ; 0x0b 8000 limit=(0xbffff-0xb8000)/4k=0x7
                dd DESC_VIDEO_HIGH4

    GDT_SIZE    equ $ - GDT_BASE        ;GDT占字节数
    GDT_LIMIT   equ GDT_SIZE - 1        ;GDT以字节的偏移值
    times 60 dq 0                       ;预留60个描述符

; -------------------------------------------------
; 初始化段选择子
; -------------------------------------------------
    SELECTOR_CODE   equ (0x0001<<3) + TI_GDT + RPL0     
    SELECTOR_DATA   equ (0x0002<<3) + TI_GDT + RPL0
    SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

; -------------------------------------------------
; 保存获取的内存
; -------------------------------------------------
; total_mem_bytes用于保存内存容量,以字节为单位
; 当前偏移loader.bin文件头0x200字节,loader.bin的加载地址是0x900,
; (4+60)*8byte = 512byte = 0x200byte
; 故total_mem_bytes内存中的地址是0x900+0x200 = 0xb00.
    total_mem_bytes dd 0

; -------------------------------------------------
; 设置gdt指针
; -------------------------------------------------

    gdt_ptr     dw GDT_LIMIT            ;16位GDT以字节的界限
                dd GDT_BASE             ;GDT的32位起始地
;人工对齐:total_mem_bytes4字节+gdt_ptr6字节+ards_buf244字节+ards_nr2,共256字节即0x100字节
    ards_buf times 244 db 0
    ards_nr dw 0		      ;用于记录ards结构体数量

;loader_start_16相对loader起始处(0x900)偏移地址为0x200 + 0x100 = 0x300
;loader_start_16为loader第一条指令地址,在mbr中直接跳转到该地址执行、
;mbr.s中 ：jmp LOADER_BASE_ADDR + 0x300 跳转到当前标号运行
loader_start_16:
;-------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP') 获取内存布局  -------

    xor ebx, ebx		      ;第一次调用时，ebx值要为0
    mov edx, 0x534d4150	      ;edx只赋值一次，循环体中不会改变
    mov di, ards_buf	      ;ards结构缓冲区
    .e820_mem_get_loop:	      ;循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820	      ;执行int 0x15后,eax值变为0x534d4150,所以每次执行int前都要更新为子功能号。
    mov ecx, 20		      ;ARDS地址范围描述符结构大小是20字节
    int 0x15
    jc .e820_failed_so_try_e801   ;若cf位为1则有错误发生，尝试0xe801子功能
    add di, cx		      ;使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]	      ;记录ARDS数量
    cmp ebx, 0		      ;若ebx为0且cf不为1,这说明ards全部返回，当前已是最后一个
    jnz .e820_mem_get_loop

    ;在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量。
    mov cx, [ards_nr]	      ;遍历每一个ARDS结构体,循环次数是ARDS的数量
    mov ebx, ards_buf 
    xor edx, edx		      ;edx为最大的内存容量,在此先清0
    .find_max_mem_area:	      ;无须判断type是否为1,最大的内存块一定是可被使用
    mov eax, [ebx]	      ;base_add_low
    add eax, [ebx+8]	      ;length_low
    add ebx, 20		      ;指向缓冲区中下一个ARDS结构
    cmp edx, eax		      ;冒泡排序，找出最大,edx寄存器始终是最大的内存容量
    jge .next_ards
    mov edx, eax		      ;edx为总内存大小
    .next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok

    ;------  int 15h ax = E801h 获取内存大小,最大支持4G  ------
    ; 返回后, ax cx 值一样,以KB为单位,bx dx值一样,以64KB为单位
    ; 在ax和cx寄存器中为低16M,在bx和dx寄存器中为16MB到4G。
    .e820_failed_so_try_e801:
    mov ax,0xe801
    int 0x15
    jc .e801_failed_so_try88   ;若当前e801方法失败,就尝试0x88方法

    ;1 先算出低15M的内存,ax和cx中是以KB为单位的内存数量,将其转换为以byte为单位
    mov cx,0x400	     ;cx和ax值一样,cx用做乘数
    mul cx 
    shl edx,16
    and eax,0x0000FFFF
    or edx,eax
    add edx, 0x100000 ;ax只是15MB,故要加1MB
    mov esi,edx	     ;先把低15MB的内存容量存入esi寄存器备份

    ;2 再将16MB以上的内存转换为byte为单位,寄存器bx和dx中是以64KB为单位的内存数量
    xor eax,eax
    mov ax,bx		
    mov ecx, 0x10000	;0x10000十进制为64KB
    mul ecx		;32位乘法,默认的被乘数是eax,积为64位,高32位存入edx,低32位存入eax.
    add esi,eax		;由于此方法只能测出4G以内的内存,故32位eax足够了,edx肯定为0,只加eax便可
    mov edx,esi		;edx为总内存大小
    jmp .mem_get_ok

    ;-----------------  int 15h ah = 0x88 获取内存大小,只能获取64M之内  ----------
    .e801_failed_so_try88: 
    ;int 15后，ax存入的是以kb为单位的内存容量
    mov  ah, 0x88
    int  0x15
    jc .error_hlt
    and eax,0x0000FFFF
        
    ;16位乘法，被乘数是ax,积为32位.积的高16位在dx中，积的低16位在ax中
    mov cx, 0x400     ;0x400等于1024,将ax中的内存容量换为以byte为单位
    mul cx
    shl edx, 16	     ;把dx移到高16位
    or edx, eax	     ;把积的低16位组合到edx,为32位的积
    add edx,0x100000  ;0x88子功能只会返回1MB以上的内存,故实际内存大小要加上1MB

    .mem_get_ok:
    mov [total_mem_bytes], edx	 ;将内存换为byte单位后存入total_mem_bytes处。

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

    jmp dword SELECTOR_CODE:loader_print_32 ;code段基址0,偏移地址最大4G

.error_hlt:		                    ;获取内存失败,出错则挂起;未成功jmp到32位指令模式则挂起
    hlt

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
	mov byte [gs:160],'p'           ;protection mode
    jmp $