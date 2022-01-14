/bochs/bin/bximage -hd -mode="flat" -size=30 -q hd30M.img
nasm -I include/ -o boot/mbr.bin boot/mbr.s
nasm -I include/ -o boot/loader.bin boot/loader.s
gcc -c -o kernel/main.o kernel/main.c
ld kernel/main.o -Ttext 0xc0001000 -e main -o kernel/kernel.bin 
dd if=boot/mbr.bin of=./hd30M.img bs=512 count=1 conv=notrunc 
dd if=boot/loader.bin of=./hd30M.img bs=512 count=4 seek=2 conv=notrunc 
dd if=kernel/kernel.bin of=./hd30M.img bs=512 count=200 seek=9 conv=notrunc