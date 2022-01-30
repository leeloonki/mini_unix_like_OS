// ---------------------------------- 2.创建中断描述符表，安装中断处理程序-----------------------------
#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"



// 8259端口
#define PIC_M_CTRL 0x20                 //主片控制端口
#define PIC_M_DATA 0x21                 //主片数据端口
#define PIC_S_CTRL 0xA0
#define PIC_S_DATA 0xA1


extern void set_cursor(uint16_t bx);

// 0x21 已支持中断数
#define IDT_DESC_CNT    0x23            // 我们在kernel中写了32+3个中断函数,并将中断入口地址数组地址intr_entry_table导出

// IF标志位
#define EFLAGS_IF 0x00000200            //eflags中IF标志位为1

// 获取eflags状态                       //输出约束=g 寄存器或内存
#define GET_EFLAGS(EFLAGS_VAR)  asm volatile("pushfl; popl %0" : "=g" (EFLAGS_VAR)) 

// 中断门描述符结构体
struct gate_desc {
    uint16_t func_offset_low_word;      //低2字节
    uint16_t selector;                  //
    uint8_t dcount;
    uint8_t attribute;                  
    uint16_t func_offset_high_word;
};

// 定义中断描述符表(数组)
static struct gate_desc idt[IDT_DESC_CNT];  
static void make_idt_desc(struct gate_desc *p_gate_desc,uint8_t attr,void * function);

// 中断处理程序入口地址数组
extern void* intr_entry_table[IDT_DESC_CNT];//kernel中定义

// 2.1 具体中断处理函数表
void* idt_table[IDT_DESC_CNT];          // 具体的中断处理函数表kernel.s中的intrxxentry只是中断程序入口,
                                        // 具体调用还是得调用这里idt_table[xx]的具体中断函数

// 2.1 中断或异常名字
char *intr_name[IDT_DESC_CNT];

// 2.2 具体的中断处理函数
static void general_intr_handler(uint8_t vec_nr){
    set_cursor(0);//将光标置为0
    int cursor_pos = 0;
    while (cursor_pos < 320 ){ //每行80个字符位置
        put_char(' '); //清空前四行，用于打印异常信息
        cursor_pos++;
    }
    set_cursor(0);
    put_str("!!!     excetion message begin     !!!");
    set_cursor(88); //从第二行8个字符开始打印 
    put_str(intr_name[vec_nr]);
    
    if(vec_nr == 14){
        int page_fault_vaddr = 0;
        asm("movl %%cr2,%0" : "=r"(page_fault_vaddr));
        put_str("\npage fault addr is ");
        put_int(page_fault_vaddr);
    }
    put_str("\n!!!     excetion message end       !!!");
    while(1);
}

// 2.3 idt_table中具体的中断函数赋值
static void exception_init(){
    int i;
    for(i =0;i<IDT_DESC_CNT;i++){
        // 默认每个中断或异常的具体中断处理函数为gener_intr_handler
        idt_table[i] = general_intr_handler;
        intr_name[i] = "unknown";   
    }
    intr_name[0] = "Divide Error";
    intr_name[1] = "Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "Breakpoint";
    intr_name[4] = "Overflow";
    intr_name[5] = "Bound Check";
    intr_name[6] = "Illegal Opcode";
    intr_name[7] = "Device Not available";
    intr_name[8] = "Double Fault";
    intr_name[9] = "Reserved";
    intr_name[10] = "Invalid TSS";
    intr_name[11] = "Segment Not Present";
    intr_name[12] = "Stack Exception";
    intr_name[13] = "General Protection Fault";
    intr_name[14] = "Page Fault";
    intr_name[15] = "Reserved";
    intr_name[16] = "Floating Point Error";
    intr_name[17] = "Alignment Check";
    intr_name[18] = "Machine Check";
    intr_name[19] = "Simd Floating Point Error";
}


// 创建中断门描述符 将kernel.s中的中断入口地址填充到idt，供lidt idt加载装配idt
// 参数 1 中断门描述符指针， 参数2 中断描述符属性， 参数3 中断处理函数
static void make_idt_desc(struct gate_desc *p_gate_desc,uint8_t attr,void * function){   // func:中断处理函数入口地址
    p_gate_desc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;     // 设置中断门描述符 低16位offset
    p_gate_desc->selector = SELECTOR_K_CODE;
    p_gate_desc->dcount =0;
    p_gate_desc->attribute =attr;
    p_gate_desc->func_offset_high_word =((uint32_t)function & 0xFFFF0000 )>>16 ;//右移16位,将偏移移到低16位赋值
}

// 初始化安装中断描述符表
static void idt_desc_init(){
    int i;
    for(i = 0;i<IDT_DESC_CNT;i++){
        make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
    }
    put_str("idt_desc_init done\n");
}

// ---------------------------------- 3.设置中断控制器pic 8259A -----------------------------
// Master 8259A的端口地址是0x20，0x21；Slave 8259A的端口地址是0xA0，0xA1。对两个8259A都进行初始化
static void pic_init(){
    // 初始化主片
    // ICW1     00010001 
    // ICW2     0010 0000       可屏蔽起始中断号为0x20 = 32，0x20-0x27 
    // ICW3     0000 0100       IR2连接从片
    // ICW4     0000 0001       8086模式 normal eoi(手动终端结束标记)
    outb(PIC_M_CTRL,0x11);      //icw1 0x20
    outb(PIC_M_DATA,0x20);      //icw2-icw4 0x21
    outb(PIC_M_DATA,0x04);
    outb(PIC_M_DATA,0x01);

    //初始化从片
    // icw2 起始中断向量号  0x28 
    // icw3 0x02
    // icw4 8086 normal eoi
    outb(PIC_S_CTRL,0x11);      //icw1 0x20
    outb(PIC_S_DATA,0x28);      //icw2-icw4 0x21
    outb(PIC_S_DATA,0x02);
    outb(PIC_S_DATA,0x01);      //

    // // 写控制字ocw 设置工作方式
    // // 我们定义的32号为第一个可屏蔽中断,其中断类型号0x20,主片ICW2 1^25 = 0x20
    // // ir0中断类型号为 0x20 + 0 = 0x20 ,对应32号可屏蔽时钟中断
    // // 这里屏蔽主片除ir0对应的时钟中断外的其余中断，从片全部中断
    // outb(PIC_M_DATA,0xfe);
    // outb(PIC_S_DATA,0xff);

    // 测试键盘中断，关闭其余所有中断
    outb(PIC_M_DATA,0xfd);
    outb(PIC_S_DATA,0xff);

    put_str("pic_init done\n");
}



// ---------------------------------- 实现开关中断函数 -----------------------------
// 关中断，返回关中断前的(开关中断)状态
enum intr_status intr_disable(){
    enum intr_status old_status;
    if(INTR_ON==intr_get_status()){
        old_status = INTR_ON;
        asm volatile("cli");            //关中断  
        return old_status;
    }else{
        old_status = INTR_OFF;
        return old_status;
    }
}

// 开中断，返回关中断前的(开关中断)状态
enum intr_status intr_enable(){
    enum intr_status old_status;
    if(INTR_ON==intr_get_status()){
        old_status = INTR_ON;
        return old_status;
    }else{
        old_status = INTR_OFF;
        asm volatile("sti");            //开中断
        return old_status;
    }
}

// 获取当前的中断状态
enum intr_status intr_get_status(){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);                 //eflags为获取到的EFLAGS各个位
    return (eflags&EFLAGS_IF) ? INTR_ON:INTR_OFF;
} 

// 将中断设置为status
enum intr_status intr_set_status(enum intr_status status){
    // status = 1 开中断，=0关
    return status & INTR_ON ? intr_enable():intr_disable();    //若status=1 则调用开中断
}

// 安装中断处理程序
void register_handler(uint8_t vector_no,void* function){
    idt_table[vector_no] = function;
}

// ---------------------------------- 3.1.完成所有中断初始化工作 -----------------------------
void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();                    //初始化中断描述符表
    exception_init();                   // 2.4注册具体中断函数
    pic_init();                         //初始化中断控制器
// ---------------------------------- 4.加载IDT中断描述符表,开中断 -----------------------------
    // lidt加载IDT表
    // lidt 48位寄存器 低16位limit 高32位base
    uint64_t idt_operand =  (sizeof(idt)-1) | ((uint64_t)(uint32_t)idt << 16);
    asm volatile("lidt %0": : "m" (idt_operand));   //64位idt_operand 高16位为0，加载idt时只加载低6字节
    put_str("idt_init_done\n");
}