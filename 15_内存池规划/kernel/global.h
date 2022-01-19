#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H

#define RPL0 0
#define RPL3 3

#define TI_GDT 0        
#define TI_LDT 1        

// INTERRUPT GATE中的selector字段，类似GDT段选择子
#define SELECTOR_K_CODE ((1<<3) + ( TI_GDT <<2 ) + RPL0 ) 

// INTERRUPT GATE属性(第4字节)
#define IDT_DESC_P  1
#define IDT_DESC_DPL0   0
#define IDT_DESC_DEP3   3
#define IDT_DESC_TYPE   0xE//1110

#define IDT_DESC_ATTR_DPL0 (( IDT_DESC_P << 7 ) + ( IDT_DESC_DPL0 << 5 )+ IDT_DESC_TYPE )
#define IDT_DESC_ATTR_DPL3 (( IDT_DESC_P << 7 ) + ( IDT_DESC_DPL3 << 5 )+ IDT_DESC_TYPE )

#define NULL ((void*)0)
#define bool int
#define true 1
#define false 0

#endif