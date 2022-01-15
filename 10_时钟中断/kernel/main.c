#include "print.h"
#include "init.h"
int main(){
	put_str("clock interrupt test\n");
	init_all();
	asm volatile("sti");
	while(1);
}

/*
init_all()调用:
	idt_init()
idt_init()调用:
    idt_desc_init();
    pic_init(); 
*/