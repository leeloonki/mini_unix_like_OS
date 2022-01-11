section mbr vstart=0x7c00
	mov ax,cs
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov fs,ax
	mov sp,0x7c00
	mov ax,0xb800
	mov gs,ax

	mov ax,0600h
	mov bx,0700h
	mov cx,0
	mov dx,184fh

	int 10h
	
	
	mov byte [gs:0x00],'l'
	mov byte [gs:0x01],'0xA4'
	mov byte [gs:0x02],'e'
	mov byte [gs:0x03],'0xA4'
	mov byte [gs:0x04],'e'
	mov byte [gs:0x05],'0xA4'
	mov byte [gs:0x06],'l'
	mov byte [gs:0x07],'0xA4'
	mov byte [gs:0x08],'o'
	mov byte [gs:0x09],'0xA4'
	mov byte [gs:0x0a],'o'
	mov byte [gs:0x0b],'0xA4'
	mov byte [gs:0x0c],'n'
	mov byte [gs:0x0d],'0xA4'
	mov byte [gs:0x0e],'k'
	mov byte [gs:0x0f],'0xA4'
	mov byte [gs:0x10],'i'
	mov byte [gs:0x11],'0xA4'

	jmp $
	times 510-($-$$) db 0
	db 0x55,0xaa
