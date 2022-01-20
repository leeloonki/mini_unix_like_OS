

#---------------------------------- 定义变量 ------------------------------------

BUILD_DIR = ./build
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I kernel/thread/
ENTRY_POINT = 0xc0001200
ASFLAGS = -f elf
LDBOOTFLAGS = -I boot/include/
CFLAGS = -Wall $(LIB) -c -fno-builtin -fno-stack-protector # -W -Wstrict-prototypes -Wmissing-prototypes 
LDFLAGS = -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/print.o \
$(BUILD_DIR)/kernel.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o \
$(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/thread.o

#---------------------------------  编译内核汇编文件 ---------------------------
 
$(BUILD_DIR)/kernel.o: kernel/kernel.s
		$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.s
		$(AS) $(ASFLAGS) $< -o $@
		

#---------------------------------- 编译内核C ----------------------------------

$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h kernel/init.h kernel/debug.h kernel/thread/thread.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/init.o: kernel/init.c kernel/interrupt.h device/timer.h lib/kernel/print.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h lib/stdint.h kernel/global.h \
		lib/kernel/io.h lib/kernel/print.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/debug.o: kernel/debug.c kernel/interrupt.h lib/kernel/print.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/kernel/print.h lib/stdint.h lib/kernel/io.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/string.o: lib/string.c lib/string.h lib/stdint.h kernel/debug.h kernel/global.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/stdint.h kernel/global.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h lib/stdint.h lib/kernel/bitmap.h \
		kernel/global.h lib/string.h
		$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/thread.o: kernel/thread/thread.c kernel/thread/thread.h kernel/memory.h lib/string.h
		$(CC) $(CFLAGS) $< -o $@

#---------------------------------  编译 mbr.bin loader.bin --------------------

$(BUILD_DIR)/mbr.bin: boot/mbr.s boot/include/boot.inc
		$(AS) $(LDBOOTFLAGS) -o $@  $<
$(BUILD_DIR)/loader.bin: boot/loader.s boot/include/boot.inc
		$(AS) $(LDBOOTFLAGS) -o $@  $<

#---------------------------------  链接内核kernel.bin --------------------------

$(BUILD_DIR)/kernel.bin:$(OBJS)
		$(LD) $(LDFLAGS) $^ -o $@

#---------------------------------  伪目标 --------------------------------------

.PHONY: mk_dir hd clean build all

mk_dir:
		if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR);fi
hd:	
		/bochs/bin/bximage -hd -mode="flat" -size=30 -q build/hd30M.img
		dd if=$(BUILD_DIR)/mbr.bin of=$(BUILD_DIR)/hd30M.img bs=512 count=1 conv=notrunc 
		dd if=$(BUILD_DIR)/loader.bin of=$(BUILD_DIR)/hd30M.img bs=512 count=4 seek=2 conv=notrunc 
		dd if=$(BUILD_DIR)/kernel.bin of=$(BUILD_DIR)/hd30M.img bs=512 count=200 seek=9 conv=notrunc
clean:
		cd $(BUILD_DIR) && rm -f ./*
build:	$(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin 
all:	mk_dir build hd