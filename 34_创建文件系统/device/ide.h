#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "sync.h"
#include "global.h"
#include "list.h"
// 分区结构:描述一个分区
struct partition{
    uint32_t start_lba;         //起始扇区
    uint32_t sec_cnt;           //扇区数
    struct disk* my_disk;       //分区所属的硬盘
    struct list_elem part_tag;  //队列元素标记，通过队列将系统所有分区链接
    char name[8];               //分区名
    struct super_block* sb;     //描述分区超级块
    struct bitmap inode_bitmap; //i结点位图，每个i结点对应一个文件属性
    struct bitmap block_bitmap; //逻辑块位图
    struct list open_inodes;    //本分区(文件系统)打开的i节点队列
};

struct disk{
    char name[8];                   //硬盘名
    struct ide_channel* my_channel; //硬盘所在ATA(IDE)通道
    uint8_t dev_no;                 //本disk是通道的主盘还是从盘 
    struct partition prim_parts[4]; //磁盘最多4个主分区
    struct partition logic_parts[8];//逻辑分区无上限，我们这里支持8个逻辑分区
};


struct ide_channel{
    char name[16];               //本ATA通道名
    uint16_t port_base;         //本通道的起始端口号
    uint8_t irq_no;             //本通道使用的中断号
    struct lock lock;           //每个通道支持两块硬盘，而通道控制器只有一个，对某个硬盘访问时，通过锁实现该硬盘驱动程序独占通道
    bool expecting_intr;        //表示等待硬盘中断，驱动程序向硬盘发送命令后阻塞，等待中断处理程序中断，
    // 中断处理程序根据此属性判断这次的中断是否是上次的驱动程序命令引起的，如果是，则进行下一步操作，如读取获取到的缓冲区数据。
    struct semaphore disk_done; //用于阻塞唤醒在该通道的磁盘驱动程序
    struct disk devices[2];     //每个通道可以安装两个硬盘 主盘、从盘
};
extern uint8_t channel_cnt;
extern struct ide_channel channels[2];
extern struct list partition_list;
void ide_read(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt);
void ide_write(struct disk*hd,uint32_t lba,void* buf,uint32_t sec_cnt);
void intr_hd_handler(uint8_t irq_no);
void ide_init();

#endif