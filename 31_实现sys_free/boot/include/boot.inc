; --------------------------------------
;               loader加载 
; --------------------------------------
LOADER_BASE_ADDR equ 0x900
; 指定loader在硬盘上的逻辑扇区地址 LBA地址为2(第三个扇区)
LOADER_START_SECTOR equ 0x2
LOADER_STACK_TOP equ LOADER_BASE_ADDR   ; 设置栈顶 0x900

; --------------------------------------
;               页表加载地址
; --------------------------------------
PAGE_DIR_TABLE_POS  equ 0x100000        ;将页目录定义在内存地址1Mb处，1k个页表紧邻其后内存

; --------------------------------------
;               GDT描述符属性
; --------------------------------------


;                            DATA SEGMENT DESCRIPTOR

;   31                23                15                7               0
;  +-----------------+-+-+-+-+---------+-+-----+---------+-----------------+
;  |#################|#|#|#|A| LIMIT   |#|     |  TYPE   |#################|
;  |###BASE 31..24###|G|B|0|V| 19..16  |P| DPL |         |###BASE 23..16###| 4
;  |#################|#|#|#|L|         |#|     |1|0|E|W|A|#################|
;  |-----------------+-+-+-+-+---------+-+-----+-+-+-+-+-+-----------------|
;  |###################################|                                   |
;  |########SEGMENT BASE 15..0#########|        SEGMENT LIMIT 15..0        | 0
;  |###################################|                                   |
;  +-----------------+-----------------+-----------------+-----------------+

;                         EXECUTABLE SEGMENT DESCRIPTOR

;   31                23                15                7               0
;  +-----------------+-+-+-+-+---------+-+-----+---------+-----------------+
;  |#################|#|#|#|A| LIMIT   |#|     |  TYPE   |#################|
;  |###BASE 31..24###|G|D|0|V| 19..16  |P| DPL |         |###BASE 23..16###| 4
;  |#################|#|#|#|L|         |#|     |1|1|C|R|A|#################|
;  |-----------------+-+-+-+-+---------+-+-----+-+-+-+-+-+-----------------|
;  |###################################|                                   |
;  |########SEGMENT BASE 15..0#########|        SEGMENT LIMIT 15..0        | 0
;  |###################################|                                   |
;  +-----------------+-----------------+-----------------+-----------------+

;                          SYSTEM SEGMENT DESCRIPTOR

;   31                23                15                7               0
;  +-----------------+-+-+-+-+---------+-+-----+-+-------+-----------------+
;  |#################|#|#|#|A| LIMIT   |#|     | |       |#################|
;  |###BASE 31..24###|G|X|0|V| 19..16  |P| DPL |0| TYPE  |###BASE 23..16###| 4
;  |#################|#|#|#|L|         |#|     | |       |#################|
;  |-----------------+-+-+-+-+---------+-+-----+-+-------+-----------------|
;  |###################################|                                   |
;  |########SEGMENT BASE 15..0#########|       SEGMENT LIMIT 15..0         | 0
;  |###################################|                                   |
;  +-----------------+-----------------+-----------------+-----------------+
;         A   - ACCESSED                              E   - EXPAND-DOWN
;         AVL - AVAILABLE FOR PROGRAMMERS USE         G   - GRANULARITY
;         B   - BIG                                   P   - SEGMENT PRESENT
;         C   - CONFORMING                            R   - READABLE
;         D   - DEFAULT                               W   - WRITABLE
;         DPL - DESCRIPTOR PRIVILEGE LEVEL


DESC_G_4K   equ 0x01<<23            ;0000 0000 1000 0000 0000 0000 0000 0000  
DESC_D_32   equ 0x01<<22            ;0000 0000 0100 0000 0000 0000 0000 0000 

DESC_L      equ 0x00<<21            ;0000 0000 0000 0000 0000 0000 0000 0000
DESC_AVL    equ 0x00<<20            ;0000 0000 0000 0000 0000 0000 0000 0000
DESC_LIMIT_CODE2  equ 0xf0000       ;0000 0000 0000 1111 0000 0000 0000 0000
DESC_LIMIT_DATA2  equ DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ 0x00000
DESC_P      equ 0x01<<15            ;0000 0000 0000 0000 1000 0000 0000 0000       

DESC_DPL_0  equ 0x00<<13            ;0000 0000 0000 0000 0000 0000 0000 0000
DESC_DPL_3  equ 0x11<<13            ;0000 0000 0000 0000 0110 0000 0000 0000

; TYPE最高位在DATA 和EXECUTABLE段均为1
DESC_S_CODE    equ 0x01<<12         ;0000 0000 0000 0000 0001 0000 0000 0000
DESC_S_DATA    equ DESC_S_CODE      ;系统段S位=0，其他段描述符为1
DESC_S_SYS     equ 0x00<<12         ;0000 0000 0000 0000 0000 0000 0000 0000

; 0 E W A
; E = 0 向上扩展
; W = 1 数据段可写
; A = 0 未访问过
DESC_TYPE_DATA  equ 0x200           ;0000 0000 0000 0000 0000 0010 0000 0000
; 1 C R A
; C = 0 非一致性
; R = 0 只执行，不可读
; A = 0 未访问过
DESC_TYPE_CODE  equ 0x800           ;0000 0000 0000 0000 0000 1000 0000 0000

DESC_TYPE_VIDEO equ DESC_TYPE_DATA  ;显存段VIDEO和DATA段TYPE属性一致

DESC_BASE_2_CODE    equ 0x00
DESC_BASE_2_DATA    equ DESC_BASE_2_CODE
DESC_BASE_2_VIDEO   equ 0x0b        ;limit 19..16

DESC_CODE_HIGH4     equ DESC_LIMIT_CODE2 + DESC_S_CODE + DESC_TYPE_CODE + DESC_BASE_2_CODE + \
(0x00<<24) + DESC_G_4K + DESC_D_32 +  DESC_L +  DESC_AVL +  DESC_P +  DESC_DPL_0
DESC_DATA_HIGH4     equ DESC_LIMIT_DATA2 + DESC_S_DATA + DESC_TYPE_DATA + DESC_BASE_2_DATA + \
(0x00<<24) + DESC_G_4K + DESC_D_32 +  DESC_L +  DESC_AVL +  DESC_P +  DESC_DPL_0
DESC_VIDEO_HIGH4    equ DESC_LIMIT_VIDEO2+ DESC_S_DATA + DESC_TYPE_VIDEO + DESC_BASE_2_VIDEO + \
(0x00<<24) + DESC_G_4K + DESC_D_32 +  DESC_L +  DESC_AVL +  DESC_P +  DESC_DPL_0

; --------------------------------------
;           选择子属性
; --------------------------------------

RPL0 equ 00b
RPL3 equ 11b

TI_GDT equ 000b
TI_LDT equ 100b

; --------------------------------------
;               页表相关属性
; --------------------------------------


PG_P    equ 1b  ;存在
PG_RW_R equ 00b ;只读
PG_RW_W equ 10b ;可读写
PG_US_U equ 100b ;允许所有特权级
PG_US_S equ 000b ;不允许特权级3访问

; --------------------------------------
;               kernel加载
; --------------------------------------
KERNEL_START_SECTOR equ 0x9     ;kernel.bin开始扇区号
KERNEL_BIN_BASE_ADDR equ 0x70000 ;读到的内存缓冲区地址
KERNEL_ENTRY_POINT equ 0xc0001500

; --------------------------------------
;               elf中程序头类型
; --------------------------------------
PT_NONE equ 0   ;No file type
PT_REL  equ 1   ;Relocatable file
PT_EXEC equ 2   ;Executable file 
