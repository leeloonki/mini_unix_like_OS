/bochs/bin/bximage -hd -mode="flat" -size=30 -q hd30M.img
nasm -I include/ -o mbr.bin mbr.s
nasm -I include/ -o loader.bin loader.s
dd if=mbr.bin of=./hd30M.img bs=512 count=1 conv=notrunc 
dd if=loader.bin of=./hd30M.img bs=512 count=4 seek=2 conv=notrunc 
