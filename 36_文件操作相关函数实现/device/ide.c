#include "ide.h"
#include "stdio-kernel.h"
#include "debug.h"
#include "global.h"         // DIV_ROUND_UP
#include "stdio.h"          //sprintf
#include "io.h"
#include "timer.h"
#include "interrupt.h"
#include "list.h"
#include "string.h"
#include "memory.h"
// 定义通道硬盘控制器的端口号
// 通道ATA0 ATA1 BASE分别为0x1F0 0x170 ，IO寄存器相对于BASE偏移0-7
// IO寄存器
#define reg_data(channel)       (channel->port_base + 0)
#define reg_error(channel)      (channel->port_base + 1)
#define reg_sect_cnt(channel)   (channel->port_base + 2)
#define reg_lba_l(channel)      (channel->port_base + 3)
#define reg_lba_m(channel)      (channel->port_base + 4)
#define reg_lba_h(channel)      (channel->port_base + 5)
#define reg_dev(channel)        (channel->port_base + 6)
#define reg_status(channel)     (channel->port_base + 7)        //状态寄存器
#define reg_cmd(channel)        (reg_status(channel))           //命令寄存器向设备发送ATA命令
// 命令寄存器
#define reg_alt_status(channel) (channel->port_base + 0x206)    //硬盘状态寄存器
#define reg_ctl(channel)        reg_alt_status(channel)         //控制寄存器


// reg_alt_status关键位
#define BIT_STAT_BSY    0x80    // Busy
#define BIT_STAT_DRDY   0x40    // Drive ready
#define BIT_STAT_DRQ    0x08    // Data request ready

// reg_device关键位
#define BIT_DEV_MBS     0xa0    // 7、5位固定 1
#define BIT_DEV_LBA     0x40    // 6位置1  lba模式
#define BIT_DEV_DEV     0x10    // 4位 指定默认设备 0表示主盘,1表示从盘

// 硬盘操作命令指令
#define CMD_IDENTIFY    0xEC    //identify指令
#define CMD_READ_SECTOR 0x20    //读扇区
#define CMD_WRITE_SECTOR 0x30   //写扇区

// 定义最大读写的扇区 lba从0开始
#define max_lba ((80*1024*1024/512)-1)

uint8_t channel_cnt;            //实际获取的通道数，根据物理地址0x475获得的硬盘个数/2反推通道数
struct ide_channel channels[2]; //支持两个ide通道

// 总扩展分区的起始lba
int32_t ext_lba_base =0;        

// 主分区和逻辑分区下表
uint8_t p_no=0,l_no=0;

// 分区链表
struct list partition_list;

// 分区表项
struct partition_table_entry{
    uint8_t bootable;       //是否可引导
    uint8_t start_head;     //起始磁头号
    uint8_t start_sec;      //起始扇区号
    uint8_t start_chs;      //起始柱面号
    uint8_t fs_type;        //分区文件系统类型
    uint8_t end_head;       //结束磁头号
    uint8_t end_sec;        //结束扇区号
    uint8_t end_chs;        //结束柱面号
    uint32_t start_lba;     //偏移lba
    uint32_t sec_cnt;       //扇区数
}__attribute__((packed));   //保证此结构体16字节大小

// 引导扇区 mbr或者ebr
struct boot_sector{
    uint8_t other[446];     //引导字节
    struct partition_table_entry partition_table[4];//分区表
    uint16_t signature;     //结束标识 0x55,0xaa
}__attribute__((packed));   //保证此结构体512字节大小



// 选择读写的硬盘
static void select_disk(struct disk* hd){
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA; //组装device寄存器值
    if(hd->dev_no ==1 ){                            //若是从盘
        reg_device |= BIT_DEV_DEV;                  //
    }
    outb(reg_dev(hd->my_channel),reg_device);       //向该磁盘所在的通道的设备寄存器写入设备值
}

// 写入读写的起始扇区地址和扇区数
static void select_sector(struct disk* hd,uint32_t lba,uint8_t sec_cnt){
    ASSERT(lba<=max_lba);
    struct ide_channel* channel = hd->my_channel;
    // 写入要读写的扇区数
    outb(reg_sect_cnt(channel),sec_cnt);
    // 写入28位LBA,lba的高4位需要写入Drive / Head Register即device寄存器,因此需要再此写入device寄存器
    outb(reg_lba_l(channel),lba);       //outb每次写1byte  将写入lba最低8位
    outb(reg_lba_m(channel),lba>>8);       
    outb(reg_lba_h(channel),lba>>16);
    outb(reg_dev(channel),BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no==1 ?BIT_DEV_DEV:0) | lba>>24);
}

// 向通道channel发送命令
static void cmd_out(struct ide_channel* channel,uint8_t cmd){
    channel->expecting_intr =true;  //每次发送命令时，将此标记标记为true，供硬盘中断处理程序判断中断程序接下来的操作
    outb(reg_cmd(channel),cmd);
}

// 等待30秒，硬盘为低速设备，处理请求时间较长，
static bool busy_wait(struct disk* hd){
    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;//30 * 1000ms
    while(time_limit-=10>=0){
        if(!(inb(reg_status(channel)) & BIT_STAT_BSY)){ //设备不忙
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        }else{
            // 休眠10ms，在等待过程中，通过调用mtime_sleep(10)，使驱动程序休眠，让出CPU;
            mtime_sleep(10);
        }
    }
    return false;//30s未ready 则返回false

}

static void read_from_sector(struct disk*hd,void* buf,uint8_t sec_cnt){
    uint32_t size_in_byte;  //将扇区转换为字节
    if(sec_cnt ==0){    //256个扇区
        size_in_byte = 256*512;
    }else{
        size_in_byte =sec_cnt*512;
    }
    insw(reg_data(hd->my_channel),buf,size_in_byte/2);  //按字读取
}


static void write_to_sector(struct disk* hd,void* buf,uint8_t sec_cnt){
    uint32_t size_in_byte;
    if(sec_cnt ==0){    //256个扇区
        size_in_byte = 256*512;
    }else{
        size_in_byte =sec_cnt*512;
    }
    outsw(reg_data(hd->my_channel),buf,size_in_byte/2);  //按字
}

// 从硬盘hd读取lba开始的sec_cnt个扇区到缓冲区buf
void ide_read(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt){
    ASSERT(lba<=max_lba);
    ASSERT(sec_cnt>0);
    lock_acquire(&hd->my_channel->lock);//驱动程序独占通道
    // 1.选择要操作的硬盘
    select_disk(hd);
    uint32_t secs_op;       //每次操作的扇区数
    uint32_t secs_done=0;   //已完成的扇区数
    // Sector Number Register 读写扇区数寄存器在LBA28下为8bit sec_cnt为0时读取256个扇区
    while(secs_done<sec_cnt){
        if((secs_done+256) <= sec_cnt){  //未读取完成
            secs_op = 256;              //每次读取256个扇区
        }else{
            secs_op = sec_cnt - secs_done;//最后一次小于256扇区
        }

        // 2.写入待读取的扇区数
        select_sector(hd,lba+secs_done,secs_op);

        // 3.将读写命令写入通道command寄存器
        cmd_out(hd->my_channel,CMD_READ_SECTOR);
        // 向通道写入读取命令后，驱动程序将自己阻塞，
        // 等待硬盘读取完成后产生中断，唤醒驱动程序，读取当前磁盘请求
        sema_down(&hd->my_channel->disk_done);//阻塞自身

        // 4.驱动程序被硬盘中断程序唤醒后检测是否可读
        if(!(busy_wait(hd))){    //如果失败(不可读取)
            char error[64];
            sprintf(error,"%s read sector %d failed!\n",hd->name,lba);
            PANIC(error);
        }
        // 5.将数据从硬盘的读出到缓冲区
        // WHILE循环每次buf地址根据secs_done变化,每次buf地址为buf + secs_done*512
        read_from_sector(hd, (void*)((uint32_t)buf + secs_done*512),secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}


// 将内存地址buf起始的sec_cnt个扇区写入到硬盘lba扇区
void ide_write(struct disk*hd,uint32_t lba,void* buf,uint32_t sec_cnt){
    ASSERT(lba<=max_lba);
    ASSERT(sec_cnt>0);
    lock_acquire(&hd->my_channel->lock); //独占通道

    // 1.选择操作的硬盘
    select_disk(hd);
    uint32_t secs_op;   //每次操作的扇区数
    uint32_t secs_done = 0; //已完成扇区数
    while(secs_done<sec_cnt){
        if((secs_done+256) <= sec_cnt){  //未读取完成
            secs_op = 256;              //每次读取256个扇区
        }else{
            secs_op = sec_cnt - secs_done;//最后一次小于256扇区
        }

        // 2.写入待写入的扇区数和起始扇区
        select_sector(hd,lba+secs_done,secs_op);

        // 3.执行命令写入控制器命令寄存器
        cmd_out(hd->my_channel,CMD_WRITE_SECTOR);

        // 4.检测硬盘状态是否可写
        if(!busy_wait(hd)){
            char error[64];
            sprintf(error,"%s write sector %d failed!\n",hd->name,lba);
            PANIC(error);
        }
        // 5.将数据写入硬盘
        write_to_sector(hd, (void*)((uint32_t)buf + secs_done*512),secs_op);
        sema_down(&hd->my_channel->disk_done);  //写入secs_op个字节命令后，休眠
        secs_done +=secs_op;
    }
    lock_release(&hd->my_channel->lock);
}


// 将dst中len个相邻两字节的位置交换后存入buf
static void swap_pairs_bytes(const char* dst,char* buf,uint32_t len){
    uint8_t idx;
    for(idx=0;idx<len;idx+=2){
        buf[idx+1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}


static void identify_disk(struct disk* hd){
    char id_info[512];      //读取的硬盘信息
    select_disk(hd);
    cmd_out(hd->my_channel,CMD_IDENTIFY);
    // 发出命令后阻塞，等待中断处理程序唤醒
    sema_down(&hd->my_channel->disk_done);
    
    // 唤醒后
    if(!busy_wait(hd)){
        char error[64];
        sprintf(error,"%s identify failed !\n",hd->name);
        PANIC(error);
    }
    read_from_sector(hd,id_info,1);     //将mbr或ebr读入到id_info
    char buf[64];                       //存放硬盘的序列号和硬盘型号
    uint8_t sn_start = 10*2,sn_len = 20;
    uint8_t md_start = 27*2,md_len =40;
    // 由于identify范围结果按字为单位，因此需要对sn\md_start开始的sn\md_len个字节的相邻两字节交换后写入buf[64]
    swap_pairs_bytes(&id_info[sn_start],buf,sn_len);
    printk("    disk %s info:\n    SN: %s\n",hd->name,buf);
    memset(buf,0,sizeof(buf));
    swap_pairs_bytes(&id_info[md_start],buf,md_len);
    printk("    MODULE: %s\n",buf);     //硬盘型号
    uint32_t sectors = *(uint32_t*)&id_info[60*2];
    printk("    SECTORS: %d\n",sectors);
    printk("    CAPACITY: %dMD\n",sectors*512/1024/1024);

}   

// 扫描硬盘hd中地址为ext_lba的扇区的所有分区
static void partition_scan(struct disk* hd,uint32_t ext_lba){
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));//mbr  ebr
    ide_read(hd,ext_lba,bs,1);      //读取ext_lba开始的一个扇区到bs指向的512字节mbr\ebr缓冲区
    uint8_t part_idx = 0;   //分区表项下标
    struct partition_table_entry* p = bs->partition_table;//p指向第一个表项
    // 遍历分区表四个表项
    while(part_idx++<4){
        if(p->fs_type ==0x5){//扩展分区
            if(ext_lba_base==0){//第一次读取引导块
                ext_lba_base = p->start_lba;//总扩展分区的偏移为相对于整个物理硬盘起始偏移lba扇区，即绝对lba扇区
                partition_scan(hd,p->start_lba);//读取第一个子扩展分区
            }else{              //读取其他子扩展分区
                partition_scan(hd,p->start_lba + ext_lba_base);
            }
        }else if(p->fs_type!=0){//有效分区类型
            if(ext_lba==0){//ext_lba=0即mbr所在扇区，此时全是主分区
                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;    //0 + 偏移
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list,&hd->prim_parts[p_no].part_tag);
                sprintf(hd->prim_parts[p_no].name, "%s%d",hd->name,p_no+1);
                p_no++;
                ASSERT(p_no<4);
            }else{
                hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;    // 扩展总分区lba + 偏移lba
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list,&hd->logic_parts[l_no].part_tag);
                sprintf(hd->logic_parts[l_no].name, "%s%d",hd->name,l_no+5); //逻辑分区从5开始
                l_no++;
                if(l_no>=8){return;}    //支持8个逻辑分区
            }
        }
        p++;        //更新分区表表项
    }
    sys_free(bs);
}

// 打印分区信息
static bool partition_info(struct list_elem* pelem,int arg){
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("    %s start_lba:0x%x, sec_cnt:0x%x\n",part->name,part->start_lba,part->sec_cnt);
    return false;//return false是为让主调函数list_traversal继续遍历链表元素
}


// 硬盘中断处理函数
void intr_hd_handler(uint8_t irq_no){
    ASSERT(irq_no == 0x2e || irq_no ==0x2f);
    uint8_t ch_no = irq_no -0x2e;
    struct ide_channel* channel = &channels[ch_no];//中断所在通道
    ASSERT(channel->irq_no == irq_no);
    if(channel->expecting_intr){
        channel->expecting_intr =false;
        sema_up(&channel->disk_done);   //唤醒驱动程序
        inb(reg_status(channel));       //读取状态寄存器，使硬盘控制认为此次中断已被处理，使硬盘可以执行新的读写
    }
}

// 硬盘数据结构初始化
void ide_init(){
    printk("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*)(0x475));  //内存地址0x475这个字节存放系统获取到的硬盘数
    ASSERT(hd_cnt>0);
    list_init(&partition_list);
    channel_cnt =  DIV_ROUND_UP(hd_cnt,2);  //每个通道最多2硬盘，根据硬盘数反推通道数
    struct ide_channel* channel;            //通道指针
    uint8_t channel_no = 0,dev_no=0;

    // 处理每个通道的硬盘
    while (channel_no < channel_cnt){
    
        channel = &channels[channel_no];
        sprintf(channel->name,"IDE(ATA)%d",channel_no); //设置通道名

        // 为每个通道初始化端口基址和中断向量
        switch (channel_no){
        case 0:
            channel->port_base = 0x1F0;
            channel->irq_no = 0x20 + 0xe;
            break;
        case 1:
            channel->port_base = 0x170;
            channel->irq_no = 0x20 + 0xf;
            break;
        }
        channel->expecting_intr = false;
        lock_init(&channel->lock);
        //驱动程序进程发出对磁盘读写命令后就阻塞，磁盘开始工作，因此将信号量初始化为0，对磁盘操作时执行p操作，阻塞当前进程或线程
        sema_init(&channel->disk_done,0);
        register_handler(channel->irq_no,intr_hd_handler);

        // 获取两硬盘的参数和分区信息
        while(dev_no<2){
            struct disk*hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;    
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            identify_disk(hd);      //获取安装的硬盘参数
            if(dev_no!=0){          //系统主盘
                partition_scan(hd,0);//扫描所有分区
            }
            p_no=0,l_no=0;          //每个盘重新初始化0
            dev_no++;
        }
        dev_no=0;                   //下一个channel
        channel_no++;
    }
    printk("\n    all partition info:\n");
    list_traversal(&partition_list,partition_info,(int)NULL);
    printk("ide_init done\n");
}

