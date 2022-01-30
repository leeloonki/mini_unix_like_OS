#include "print.h"
#include "init.h"
#include "debug.h"
#include "thread.h"
#include "timer.h"
#include "interrupt.h"
#include "console.h"
void k_kernel_a(void*);
void k_kernel_b(void*);
int main(){
	put_str("this is kernel\n");
	init_all();

	// thread_start("k_kernel_a",30,k_kernel_a,"arg_A ");
	// thread_start("k_kernel_a",8,k_kernel_a,"arg_B ");
	// intr_enable();
	// while(1){
	// 	console_put_str("Main ");
	// }
	
	intr_enable();
	while(1);
	return 0;
}

void k_kernel_a(void* _arg){
	char* arg = (char*)_arg;
	while (1)
	{
		console_put_str(arg);
	}
}

void k_kernel_b(void* _arg){
	char* arg = (char*)_arg;
	while (1)
	{
		console_put_str(arg);
	}
}