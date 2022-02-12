#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"
#include "memory.h"


int main(void) {
   put_str("I am kernel\n");
   init_all();
   while(1);
   return 0;
}

