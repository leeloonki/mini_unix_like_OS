nasm -f elf -o lib/kernel/print.o lib/kernel/print.s
gcc -I lib/kernel/ -c -o kernel/main.o kernel/main.c
ld -Ttext 0xc0001200 -e main -o kernel.bin kernel/main.o lib/kernel/print.o

