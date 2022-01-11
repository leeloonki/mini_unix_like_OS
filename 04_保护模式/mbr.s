; /*
;  * @Author: leeloonki
;  * @Bilibili: 李景芳_
;  * @Date: 2022-01-11 00:44:21
;  * @LastEditTime: 2022-01-11 11:29:19
;  * @LastEditors: leeloonki
;  * @Description: mbr读取4扇区的loader
;  * @FilePath: \code\04_保护模式\mbr.s
;  */

; mbr主引导程序
; ---------------------------------------------------------------
%include "boot.inc"
section mbr vstart=0x7c00
	mov ax,cs					;Bios进入MBR通过跳转指令jmp 0:7c00,此时cs = 0
	mov ds,ax
	mov es,ax					;es = 0
	mov ss,ax
	mov fs,ax
	mov sp,0x7c00
	mov ax,0xb800
	mov gs,ax

; 清屏
; ---------------------------------------------------------------
; 输入：
; AH = 0x06 功能号 利用0x06功能，上卷全部行
; AL 上卷行 0表示上卷全部
; (CL,CH) 窗口左上(x,y)位置
; (DL,DH) 窗口右下(x,y)位置
	mov ax,0600h
	mov bx,0700h
	mov cx,0		;左上 (0,0)
	mov dx,184fh 	;右下 (80，25) 
	int 10h
	

; 输出字符
; ---------------------------------------------------------------
	mov byte [gs:0x00],'m'
	mov byte [gs:0x01],'0xA4'
	mov byte [gs:0x02],'b'
	mov byte [gs:0x03],'0xA4'
	mov byte [gs:0x04],'r'
	mov byte [gs:0x05],'0xA4'

	mov eax,LOADER_START_SECTOR ;loader起始扇区28位LBA地址
	mov bx,LOADER_BASE_ADDR		;loader写入内存地址 bx = 0x900
	mov cx,4					;写入从LOADER_START_SECTOR开始的一个扇区
	call rd_disk				;读取磁盘扇区

	jmp LOADER_BASE_ADDR		;读入后从0x900开始的loader运行

rd_disk:
; 读取磁盘
; ---------------------------------------------------------------
	mov esi,eax					;保存eax原值
	mov di,cx					;保存cx原值
	
	; 1.设置读取磁盘扇区数
	mov dx,0x1f2				;虚拟硬盘 ata0-master 通过0x1f2端口访问
	mov al,cl					;向端口dx输出的al寄存器值cl表示读取1扇区
	out dx,al
	mov eax,esi					;恢复eax

; IO端口	端口用途
; primary通道	secondary通道	读操作时	 写操作时
; 0x1f0			0x170			data		data
; 0x1f1			0x171			error		features
; 0x1f2			0x172			sector countsector count
; 0x1f3			0x173			LBA low	 	LBA low
; ox1f4			0x174			LBA mid	 	LBA mid
; 0x1f5			0x175			LBA high 	LBA high
; 0x1f6			0x176			device		device
; 0x1f7			x177			status		command
	; 2.将eax 中LBA地址存入0x1f3 0x1f4 0x1f5中
	;写LBA 0-7位地址 
	mov dx,0x1f3
	out dx,al
	;写LBA 8-15位地址
	mov cl,8
	shr eax,cl					;逻辑右移8位
	mov dx,0x1f4
	out dx,al
	;写LBA 16-23位地址
	shr eax,cl					;逻辑右移8位
	mov dx,0x1f5
	out dx,al
	;写LBA 24-27位地址 ，写入 0x1f6端口低四位
	shr eax,cl					;此时eax低八位为初始时的31-24位
	and al,0x0f					;取27-24位
	or al,0xe0					;设置0x1f6端口高四位为e 1110
	; 0x1f6: 第4位0表示主盘,1表示从盘;5 7位 固定为1;6位位1 表示采用LBA寻址 
	; https://juejin.cn/post/6996933455514697742
	mov dx,0x1f6
	out dx,al					;输出al到该端口
	
	; 3.向0x1f7端口写读取状态命令
	; 0x20，读扇区命令
	; 0x30，写扇区命令
	; 0xc4，读多个扇区
	; 0xc5，写多个扇区
	mov dx,0x1f7
	mov al,0x20
	out dx,al

	; 4.检测硬盘状态
.not_ready:
	nop							;等待
	in al,dx					;读0x1f7端口
	; status寄存器 第7位bsy=1表示硬盘忙 第3位drq=1表示硬盘可以传输数据
	and al,0x88					;0x88 = 0b1000 1000 只对3 7 位判断 
	cmp al,0x08					;判断硬盘是否可以传输数据
	jnz .not_ready				;若al=0x08 zf = 1，表示可以传输数据，若zf!=1即jnz时轮询等待 

	; 5.从0x1f0读取数据
	mov ax,di					;ax设置读取扇区数
	mov dx,256					;
	mul dx						;ax * dx = 256 *  1 =256 字
	mov cx,ax					;mul 低16位 在ax中，cx = 256，每次读取一个字
	mov dx,0x1f0
.go_on_read:
	in ax,dx
	mov [bx],ax					;将读入的一个扇区读到0x900处开始的内存
	add bx,2
	loop .go_on_read
	
	ret							;读取完成函数返回

	times 510-($-$$) db 0
	db 0x55,0xaa
