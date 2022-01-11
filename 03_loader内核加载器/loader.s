%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR	;LOADER加载到内存0x900处
	
; 清屏
	mov ax,0600h
	mov bx,0700h
	mov cx,0		;左上 (0,0)
	mov dx,184fh 	;右下 (80，25) 
	int 10h


; 输出字符
	mov byte [gs:0x00],'l'
	mov byte [gs:0x01],'0xA4'
	mov byte [gs:0x02],'o'
	mov byte [gs:0x03],'0xA4'
	mov byte [gs:0x04],'a'
	mov byte [gs:0x05],'0xA4'
	mov byte [gs:0x06],'d'
	mov byte [gs:0x07],'0xA4'
	mov byte [gs:0x08],'e'
	mov byte [gs:0x09],'0xA4'
	mov byte [gs:0x0a],'r'
	mov byte [gs:0x0b],'0xA4'

	jmp $

	; 02_mbr操作硬盘 中的hd60M第0扇区mbr扇区的指令 将读取硬盘第2扇区
	; 读入内存0x900中，并跳转该处执行
	; 因此需要将本loader.s编译后写入硬盘hd60M第2扇区