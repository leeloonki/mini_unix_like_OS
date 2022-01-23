#include "print.h"
#include "init.h"
#include "debug.h"
#include "thread.h"

void k_kernel_a(void*);

int main(){
	put_str("this is kernel\n");
	init_all();

	thread_start("k_kernel_a",2,k_kernel_a,"arg_A");
	while(1);
	return 0;
}

void k_kernel_a(void* _arg){
	char* arg = (char*)_arg;
	while (1)
	{
		put_str(arg);
	}
	
}