#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
void k_kernel_a(void*);
void k_kernel_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int test_var_a=0,test_var_b=0;

int main(){
	put_str("this is kernel\n");
	init_all();
	put_str("init_all done\n");
	thread_start("k_kernel_a",30,k_kernel_a,"arg_A ");
	thread_start("k_kernel_b",30,k_kernel_b,"arg_B ");
	process_execute(u_prog_a,"user_prog_a");
	process_execute(u_prog_b,"user_prog_b");
	
	intr_enable();
	
	while(1);
	return 0;
}

void k_kernel_a(void* _arg){
	while (1)
	{
		console_put_str("v_a:0x ");
		console_put_int(test_var_a);
	}
}

void k_kernel_b(void* _arg){
	while (1)
	{
		console_put_str("v_b:0x ");
		console_put_int(test_var_b);
	}
}

void u_prog_a(){
	while(1){
		test_var_a++;
	}
}

void u_prog_b(){
	while(1){
		test_var_b++;
	}
}