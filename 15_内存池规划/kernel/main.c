#include "print.h"
#include "init.h"
#include "debug.h"
int main(){
	put_str("this is kernel\n");
	init_all();
	while(1);
	return 0;
}