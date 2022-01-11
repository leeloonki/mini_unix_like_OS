/bochs/bin/bximage -hd -mode="flat" -size=30 -q hd30M.img
nasm -o mbr.bin mbr.s
dd if=mbr.bin of=./hd30M.img bs=512 count=1 conv=notrunc 
