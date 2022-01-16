echo "-----------------创建硬盘"
/bochs/bin/bximage -hd -mode="flat" -size=30 -q build/hd30M.img
echo "-----------------编译mbr loader"
nasm -I boot/include/ -o build/mbr.bin boot/mbr.s
nasm -I boot/include/ -o build/loader.bin boot/loader.s
echo "-----------------内核"

nasm -f elf -o build/print.o lib/kernel/print.s
nasm -f elf -o build/kernel.o kernel/kernel.s

gcc -I lib/kernel/ -I lib/ -c -fno-stack-protector -o build/timer.o device/timer.c
gcc -I lib/kernel/ -I lib/ -I kernel/ -c -fno-stack-protector -o build/main.o kernel/main.c
gcc -I lib/kernel/ -I lib/ -I kernel/ -c -fno-stack-protector -o build/interrupt.o kernel/interrupt.c
gcc -I lib/kernel/ -I lib/ -I kernel/ -I device/ -c -fno-stack-protector -o build/init.o kernel/init.c

ld -Ttext 0xc0001200 -e main -o build/kernel.bin build/main.o build/init.o \
build/interrupt.o build/print.o  build/kernel.o build/timer.o
echo "-----------------写硬盘"
dd if=build/mbr.bin of=build/hd30M.img bs=512 count=1 conv=notrunc 
dd if=build/loader.bin of=build/hd30M.img bs=512 count=4 seek=2 conv=notrunc 
dd if=build/kernel.bin of=build/hd30M.img bs=512 count=200 seek=9 conv=notrunc