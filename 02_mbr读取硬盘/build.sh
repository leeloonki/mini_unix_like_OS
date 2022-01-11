/bochs/bin/bximage -hd -mode="flat" -size=60 -q hd60M.img
nasm -o mbr.bin mbr.s
dd if=mbr.bin of=./hd60M.img bs=512 count=1 conv=notrunc 
