#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
      
#include "stdint.h"
/************************************ GDT描述符属性 *******************************/
#define DESC_G_4K   1
#define DESC_D_32   1
#define DESC_L      0
#define DESC_AVL    0
#define DESC_P      1
#define DESC_DPL_0  0
#define DESC_DPL_3  3  //00 01 10 11
#define DESC_S_CODE 1               //EXECUTABLE SEGMENT DESCRIPTOR  S位为1
#define DESC_S_DATA DESC_S_CODE     //DATA SEGMENT DESCRIPTOR   S位为1
#define DESC_S_SYS  0               //SYSTEM SEGMENT DESCRIPTOR S位为0
// ; 0 E W A
// ; E = 0 向上扩展
// ; W = 1 数据段可写
// ; A = 0 未访问过
#define DESC_TYPE_DATA  2            //高4字节中7到11位type属性
// ; 1 C R A
// ; C = 0 非一致性
// ; R = 0 只执行，不可读
// ; A = 0 未访问过
#define DESC_TYPE_CODE  8

// TSS描述符中的B位是“忙”位（Busy）。在任务刚刚创建的时候，它应该为二进制的1001，即，B位是“0”，表明任务不忙。
// 1001b = 9
#define DESC_TYPE_TSS   9

#define RPL0 0
#define RPL3 3

#define TI_GDT 0        
#define TI_LDT 1  

// 创建GDT选择子
#define SELECTOR_K_CODE ((1<<3) + ( TI_GDT <<2 ) + RPL0 ) //内核代码段 GDT 第1项
#define SELECTOR_K_DATA ((2<<3) + ( TI_GDT <<2 ) + RPL0 ) //内核数据段 GDT 第2项
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_VIDEO ((3<<3) + ( TI_GDT <<2 ) + RPL0 )//内核显存段 GDT 第3项
#define SELECTOR_U_CODE ((5<<3) + ( TI_GDT <<2 ) + RPL0 ) //内核显存段 GDT 第5项
#define SELECTOR_U_DATA ((6<<3) + ( TI_GDT <<2 ) + RPL0 ) //内核数据段 GDT 第6项
#define SELECTOR_U_STACK SELECTOR_U_DATA                    

// GDT属性位：高四字节的20-23位  代码段数据段该些位属性相同
#define GDT_ATTR_HIGH ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5 ) + (DESC_AVL << 4 ))
// GDT属性位：高四字节的8-15位  特权级为3的代码段数据段的部分属性位
#define GDT_CODE_ATTR_LOW_DPL3 ( (DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE ) 
#define GDT_DATA_ATTR_LOW_DPL3 ( (DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA )

/************************************ TSS描述符属性 *******************************/
#define TSS_DESC_D 0
#define SELECTOR_TSS ((4<<3) + ( TI_GDT <<2 ) + RPL0 )    //内核TSS段 GDT 第4项
// TSS段描述符属性
#define TSS_ATTR_HIGH ((DESC_G_4K <<7 ) + (TSS_DESC_D <<6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0) 
#define TSS_ATTR_LOW ((DESC_G_4K <<7 ) + (DESC_DPL_0 <<5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS ) 

// GDT描述符结构体
struct gdt_desc{
    uint16_t limit_low_word;            //低四字节：0-15位段界限
    uint16_t base_low_word;             //低四字节：16-31位段基址
    uint8_t base_mid_byte;              //高四字节：0-7位段基址
    uint8_t attr_low_byte;              //高四字节：8-15位属性低位
    uint8_t limit_high_attr_high;       //高四字节：16-23位 偏移高位属性高位
    uint8_t base_high_byte;             //高四字节：24-31位段基址高位
};


/************************************ IDT描述符属性 *******************************/
#define IDT_DESC_P  1
#define IDT_DESC_DPL0   0
#define IDT_DESC_DEP3   3
#define IDT_DESC_TYPE   0xE//1110

#define IDT_DESC_ATTR_DPL0 (( IDT_DESC_P << 7 ) + ( IDT_DESC_DPL0 << 5 )+ IDT_DESC_TYPE )
#define IDT_DESC_ATTR_DPL3 (( IDT_DESC_P << 7 ) + ( IDT_DESC_DPL3 << 5 )+ IDT_DESC_TYPE )



/*数据类型*/
#define NULL ((void*)0)
#define bool int
#define true 1
#define false 0

#endif