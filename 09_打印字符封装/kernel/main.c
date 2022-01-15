#include "print.h"
int main(){
	put_char('k');
	put_char('e');
	put_char('r');
	put_char('n');
	put_char('e');
	put_char('l');
	put_char('!');
	put_char('\n');
	put_str("it's kernel\n");
	put_str("string\n");
	put_int(0x3fffffff);
	put_char('\n');
	put_int(0x12345678);
	put_char('\n');
	put_int(0x00345678);
	put_char('\n');
	put_int(0x00000000);
	
	while(1);
}
