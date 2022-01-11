; 
; loader加载到内存0x900处
LOADER_BASE_ADDR equ 0x900
; 指定loader在硬盘上的逻辑扇区地址 LBA地址为2(第三个扇区)
LOADER_START_SECTOR equ 0x2