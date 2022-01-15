echo "-----------------创建硬盘"
/bochs/bin/bximage -hd -mode="flat" -size=30 -q hd30M.img
echo "-----------------编译mbr loader"
nasm -I boot/include/ -o boot/mbr.bin boot/mbr.s
nasm -I boot/include/ -o boot/loader.bin boot/loader.s
echo "-----------------内核"
nasm -f elf -o lib/kernel/print.o lib/kernel/print.s
gcc -I lib/kernel/ -c -o kernel/main.o kernel/main.c
ld -Ttext 0xc0001200 -e main -o kernel/kernel.bin kernel/main.o lib/kernel/print.o
echo "-----------------写硬盘"
dd if=boot/mbr.bin of=./hd30M.img bs=512 count=1 conv=notrunc 
dd if=boot/loader.bin of=./hd30M.img bs=512 count=4 seek=2 conv=notrunc 
dd if=kernel/kernel.bin of=./hd30M.img bs=512 count=200 seek=9 conv=notrunc