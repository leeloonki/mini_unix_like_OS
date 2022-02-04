#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"			//内核态下线程调用的函数
#include "syscall.h"				//用户态下进程调用
void k_kernel_a(void*);
void k_kernel_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid =0,prog_b_pid =0;

int main(){
	put_str("this is kernel\n");
	init_all();
	
	process_execute(u_prog_a,"user_prog_a");
	process_execute(u_prog_b,"user_prog_b");
	
	intr_enable();
	
	console_put_str(" main_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');

	thread_start("k_kernel_a",30,k_kernel_a,"arg_A ");
	thread_start("k_kernel_b",30,k_kernel_b,"arg_B ");
	

	while(1);
	return 0;
}

void k_kernel_a(void* _arg){
	console_put_str(" thread_a_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');

	console_put_str(" prog_a_pid:0x");
	console_put_int(prog_a_pid);
	console_put_char('\n');
	while(1);
}

void k_kernel_b(void* _arg){
	console_put_str(" thread_b_pid:0x");
	console_put_int(sys_getpid());
	console_put_char('\n');

	console_put_str(" prog_b_pid:0x");
	console_put_int(prog_b_pid);
	console_put_char('\n');
	while(1);
}

void u_prog_a(){
	prog_a_pid = getpid();
	while(1);
}

void u_prog_b(){
	prog_b_pid = getpid();
	while(1);
}

// 用户进程在用户态下使用 getpid();接口函数
// 内核线程在内核态下，直接使用内核函数sys_getpid();