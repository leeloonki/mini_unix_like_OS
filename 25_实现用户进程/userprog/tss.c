#include "stdint.h"
#include "thread.h"
#include "print.h"
#include "string.h"
#include "global.h"

#define PG_SIZE 4096
// tss结构体
struct tss{
    uint32_t link;
    uint32_t* esp0;
    uint32_t ss0;
    uint32_t* esp1;
    uint32_t ss1;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint32_t IOPB;
};

static struct tss tss;

// 任务切换时切换esp0属性为当前任务(线程或进程)的0级栈
void update_tss_esp(struct task_struct* pthread){
    tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE); 
}

// 创建GDT描述符
// 参数段基址、段偏移、属性低8、属性高8
static struct gdt_desc make_gdt_desc(uint32_t* desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high){
    uint32_t desc_base = (uint32_t)desc_addr;
    struct gdt_desc desc;
    desc.limit_low_word = limit & 0x0000ffff;
    desc.base_low_word = desc_base & 0x0000ffff;
    desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16 );
    desc.attr_low_byte = (uint8_t)(attr_low);
    desc.limit_high_attr_high = (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
    desc.base_high_byte = desc_base >>24;
    return desc; 
}

// 在GDT中创建tss并重新加载GDT
void tss_init(){
    put_str("tss_init start\n");
    uint32_t tss_size = sizeof(tss);
    memset(&tss,0,tss_size);    //初始化tss内存结构
    // 实现任务切换时只使用tss中的0特权级栈
    tss.ss0 = SELECTOR_K_STACK; //指定内核栈所在段 平坦模式下段基址0，偏移4G
    
    // 在gdt添加dpl为0的tss描述符(loader.s中已添加1+3个)
    // tss位于gdt表中第4项
    // GDT表在loader中在内存地址0x900开始处，TSS为GDT表第4项(第0项为空) 每项8字节，TSS为第5项偏移0x900 共32 = 0x20字节，在创建页表时，物理内存低1M对应虚拟地址c0000000 
    *((struct gdt_desc*)0xc0000920) = make_gdt_desc( (uint32_t*)(&tss), tss_size-1, TSS_ATTR_LOW, TSS_ATTR_HIGH);
    // 在GDT表添加dpl为3的数据段和代码段
    *((struct gdt_desc*)0xc0000928) = make_gdt_desc( (uint32_t*)(0), 0xfffff,GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
    *((struct gdt_desc*)0xc0000930) = make_gdt_desc( (uint32_t*)(0), 0xfffff,GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);

    // 描述符大小
    uint32_t gdt_limit = (8* (1 + 3 + 1 + 2 ));
    uint64_t gdt_base = (uint64_t)(uint32_t)(0xc0000900) << 16;
    // lgdt操作共48位对应gdtr低16位偏移高32位基址
    uint64_t gdt_operand = gdt_limit | gdt_base;
    asm volatile("lgdt %0": : "m" (gdt_operand)); 
    // 加载GDT第四项的tss段选择子
    asm volatile("ltr %w0": : "r" (SELECTOR_TSS)); 
    put_str("tss_init and ltr done\n");
}