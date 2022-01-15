; kernel/main.c调用print.s中 put_char('a'),
; 根据cdecl函数调用ABI main函数将形参 一个字符压入栈中,压入put_char的返回地址
; put_char获取中的字符,并输出到屏幕

; 对于待打印的字符
    ; 若为不可打印的控制字符 :回车符\换行符\退格符 则进入相应处理流程
        ; 二进制	十进制	十六进制 字符/缩写	解释
        ; 00001000	8	    08	    BS (Backspace)	退格
        ; 00001101	13	    0D	    CR (Carriage Return)	回车键
        ; 00001010	10	    0A	    LF/NL(Line Feed/New Line)	换行键
    ; 对其他可打印字符,进行输出



; 设置显存段段描述符
TI_GDT equ 000b
RPL0 equ 00b
SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text
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
