; kernel/main.c调用print.s中 put_char('a'),

TI_GDT equ 000b
RPL0 equ 00b
SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

section .data
put_int_buffer  dd 0
                dd 0            ;存储转化后的8个ASCII

[bits 32]
section .text
; -------------------------------------------------
; 输出字符串
; 调用示例 put_str("its kernel");
; -------------------------------------------------
global put_str
put_str:
    ; main函数调用put_str,put_str获取字符串起始地址压入栈中,
    ; 栈中已压入了字符串起始地址 put_str返回地址

    push ebx
    push ecx
    mov ebp,esp
    and ecx,0
    mov ebx,[ebp+12]                ;使用ebx取出字符串起始地址
    
.goon:
    mov cl,[ebx]                    ;cl 存放字符串字符
    cmp cl,0                        ;C语言中字符串末尾 '\0'其ASCII为0
    jz .str_over
    push ecx                        ;ecx为put_char参数
    call put_char
    add esp,4
    inc ebx
    jmp .goon 

.str_over:
    pop ecx
    pop ebx
    ret

; -------------------------------------------------
; 输出一个字符
; -------------------------------------------------
global put_char             ;put_char被main函数调用,通过global修饰将put_char导出为全局符号
put_char:
    pushad                  ;被调用者保存寄存器环境 这里压入全部8个双子长的寄存器
    mov ax,SELECTOR_VIDEO
    mov gs,ax               ;设置显存段选择子

    ; 获取光标位置
    ; Address Register    3D4h
    ; Data Register       3D5h
    ; 获取光标位置高8位
    mov dx,0x03d4           ;索引寄存器
    mov al,0x0e             ;Index 0Eh -- Cursor Location High Register
    out dx,al
    mov dx,0x03d5           ;数据寄存器
    in al,dx        
    mov ah,al               ;将光标高8位放到ah中
    ; 获取光标低8位         
    mov dx,0x03d4
    mov al,0x0f             ;Index 0Fh -- Cursor Location Low Register
    out dx,al
    mov dx,0x03d5
    in al,dx                ;此时ax寄存器既为光标位置
    mov bx,ax               ;bx也是
    ; 从栈中获取待打印的字符

        ; 00001000	8	    08	    BS (Backspace)	退格(回退一字符,删除原来光标前端一字符,)
        ; 00001101	13	    0D	    CR (Carriage Return)	回车键
        ; 00001010	10	    0A	    LF/NL(Line Feed/New Line)	换行键

    ; 栈中压入待打印字符 、put_char返回地址、 8个双字寄存器，esp+36 为待打印字符地址 
    mov ecx,[esp+36]        ;ecx存放待打印的字符    main中传入put_char字符为1字节 ,字符位于ecx最低1字节al中
    cmp cl,0x0d             ;是否为回车键
    jz .is_carriage_return
    cmp cl,0x0a             ;是否为换行键
    jz .is_line_feed        
    cmp cl,0x08             ;是否为退格
    jz .is_backspace

    ; 若为可打印字符则打印
    jmp .put_other

.is_backspace:
    dec bx                  ;bx为光标位置,80*25文本模式下 一页显示2000个字符,对应光标0-1999
                            ;回退时,光标位置回退一个,并将被回退的字符使用空格填充(在相应的显存写入空格对应的ASCII)
    shl bx,1                ;bx逻辑左移一位*2,bx从光标位置转换为被回退字符的显存位置
    ; 00100000	32	20	(Space)	空格
    mov byte [gs:bx],0x20   ;设置ASCII 将这个被回退字符 显存的ASCII修改为空格
    inc bx
    mov byte [gs:bx],0x07   ;设置属性 黑底白字
    dec bx                  ;dx为显存字符偶地址
    shr bx,1                  ;bx从显存地址变为光标位置
    jmp .set_cursor         ;设置光标位置            


.put_other:
    shl bx,1                ;光标位置转化为显存位置
    mov [gs:bx],cl          ;设置ASCII 写入打印的字符ASCII
    inc bx                  ;设置属性 
    mov byte [gs:bx],0x07
    shr bx,1
    inc bx                  ;写入字符后 光标位置加1
    ; 判断是否写满2000个字符
    cmp bx,2000
    jl .set_cursor          ;若小于,则设置光标,若大于则换行处理

; 对回车和换行 参照Linux全部进行换行处理

.is_carriage_return:         
.is_line_feed:

    ; 被除数	除数	商	余数
    ; AX	reg/mem8	AL	AH
    ; DX:AX	reg/mem16	AX	DX
    ; EDX:EAX	reg/mem32	EAX	EDX
    xor dx,dx               ;被除数高16位
    mov ax,bx               ;bx为当前光标位置,ax被除数低16位
    mov si,80
    div si                  ;dx:ax/si dx 为余数
    sub bx,dx               ;光标位置 减去除80的余数实现回车操作

    add bx,80               ;每行80个光标(字符)，实现光标换行
    cmp bx,2000
    jl .set_cursor

; 光标位置超出2000,滚动屏幕
.roll_screen:                
    ; 一屏80*25 对应显存80 *25 *2 = 4kb
    ; 屏幕 0-24 共 25 行 ，80列 每行 80*2 =160字节
    ; 滚屏时,若光标超出2000,将从第1-24 共24行向前移动一行（字符串拷贝）
    ; 并将原25行使用空格填充
    ; 同时把光标设置为25行起始处
    cld                     ;清空方向标志位
    ; 第1行偏移显存起始地址 80*2 = 160 = 0xa0
    mov ecx,1920                ;2000-80=1920字
    mov esi,0xc00b8000 + 0xa0   ;第1行行首
    mov edi,0xc00b8000 + 0      ;第0行行首
    rep movsw                   ;每次移动1字
    
    ; 将最后一行填充空格
    mov ebx,1920*2              ;第24行起始现存地址
    mov ecx,80
.cls:
    mov byte [gs:ebx],0x20      ;设置ASCII 写入打印的字符ASCII
    inc ebx                     ;设置属性 
    mov byte [gs:ebx],0x07
    inc ebx
    loop .cls
    mov bx,1920                 ;将光标重设成最后一行行首

.set_cursor:                     ;将光标位置bx写入显卡
    ; 设置高8位
    mov dx,0x03d4
    mov al,0x0e
    out dx,al
    mov dx,0x03d5
    mov al,bh                   ;bh高八位
    out dx,al

    ; 设置低8位
    mov dx,0x03d4
    mov al,0x0f
    out dx,al
    mov dx,0x03d5
    mov al,bl                   ;bh高八位
    out dx,al

    popad
    ret

; -------------------------------------------------
; 打印数字   
; 形参:栈中的int类型数
; 输出:输出int类型数字的十六进制
; -------------------------------------------------

; 0x 1    f    2     c    5    4    2   3           int形参
;   0001 1111 0010 1100 0101 0100 0010 0011 b      二进制
; 每次处理eax中的4位二进制数,将其转化为对应的ASCII后保存,最后调用put_char输出
; 共32/4 = 8个ASCII,定义8字节内存空间存储转化后的ASCII码
; 3

global put_int
put_int:
    pushad                      ;8个双字寄存器
    mov ebp,esp
    mov eax,[ebp+9*4]           ;获取int形参数字
    mov edx,eax                 ;edx即形参
    mov edi,7                   ;每次右移4位，先处理的是最低部分，
    ; 因此处理得到的ASCII在缓冲区从后向前放置,这里偏移7,表示第8个字符
    mov ecx,8                   ;8个4位二进制数 循环8次
    mov ebx,put_int_buffer      ;ebx 缓冲区起始地址

.16based_4bits:
    ; 每次处理形参int数据的四位,将其转化为ASCII
    and edx,0x0000000F          ;获取4位
    cmp edx,9                   ;是否 0-9 或者 （a-f）
    jg .is_AtoF                 ;字母a - f
    add edx,'0'                 ;数字加上字符0的偏移既是该数字对应的ASCII 
    jmp .store
.is_AtoF:
    sub edx,10                  
    add edx,'A'                 ;若a(1010) - 10  = 0  0+'A ' = 'A'
.store:
    mov [ebx+edi],dl            ;edx中dl字节为处理后数字的ASCII
    dec edi
    shr eax,4                   ;处理次低4位
    mov edx,eax                 ;
    loop .16based_4bits

.ready_to_print:
    inc edi                     ;loop退出时edi 被减为-1

;  
.skip_prefix_0:
    cmp edi,8
    je .full0
    mov cl,[put_int_buffer+edi]
    inc edi
    cmp cl,'0'                  ;若部分0,在.put_each_num从偏移edi的第一个非'0'输出
    je .skip_prefix_0
    dec edi
    jmp .put_each_num           ;最高位ASCII不为0

.full0:
    mov cl,'0'
.put_each_num:
    push ecx                    ;字符cl中 mov cl,[put_int_buffer+edi]
    call put_char
    add esp,4                   ;恢复栈
    inc edi
    mov cl,[put_int_buffer+edi]
    cmp edi,8
    jl .put_each_num
    popad
    ret




