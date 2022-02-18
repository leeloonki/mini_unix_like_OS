#include "inode.h"
#include "ide.h"
#include "stdint.h"
#include "global.h"      //bool类型定义
#include "debug.h"      //ASSERT宏
#include "fs.h"
#include "string.h"
#include "list.h"
#include "thread.h"
#include "memory.h"
#include "interrupt.h"
#include "super_block.h"
// 内存中存储inode_locate获取的inode位置的结构体
struct inode_position{
    bool two_sec;       //硬盘分区上的inode是否跨扇区,即在两扇区连接处
    uint32_t sec_lba;   //inode所在起始扇区号
    uint32_t off_size;  //inode在所在的扇区内的偏移字节数
};


// 获取硬盘分区中inode所在扇区和扇区内偏移
// 形参(分区指针,inode号,保存inode位置的内存结构体指针)
static void inode_locate(struct partition* part,uint32_t inode_no,struct inode_position* inode_pos){
    ASSERT(inode_no<MAX_FILES_PER_PART);//每个分区最多支持该宏4096个inode
    // 挂载分区时mount_partition已将分区part的超级块读入内存进行管理。
    uint32_t inode_table_lba = part->sb->inode_table_lba;
    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no*inode_size;//此off_size为第inode_no号inode据inode_table_lba扇区的字节偏移
    uint32_t off_sec = off_size/SECTOR_SIZE;//此off_sec为第inode_no号inode据inode_table_lba扇区的扇区偏移
    uint32_t off_size_in_sec = off_size%SECTOR_SIZE;//inode_no对应的inode在所在扇区的偏移,在扇区的起始字节

    // 判断inode结构体是否跨过2个扇区
    if(off_size_in_sec + inode_size>SECTOR_SIZE){//起始字节+其大小字节>一个扇区
        inode_pos->two_sec = true;
    }else{
        inode_pos->two_sec = false;
    }
    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}


// 将内存的inode同步(写)到硬盘分区part
void inode_sync(struct partition* part,struct inode* inode,void* io_buf){
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    // 根据形参inode指针指向的inode号，定位该inode在硬盘分区上的位置
    inode_locate(part,inode_no,&inode_pos);//获取的硬盘上的inode位置信息将存入inode_pos
    ASSERT(inode_pos.sec_lba <= (part->start_lba+part->sec_cnt));

    // inode的如下三个属性只在内存使用，写入硬盘分区时,需清除内存中的inode缓存的这三项
    // uint32_t i_open_cnts;   //记录本文件被打开的次数
    // bool write_deny;
    // struct list_elem inode_tag;
    struct inode pure_inode;   //构造写入硬盘的inode
    memcpy(&pure_inode,inode,sizeof(struct inode));//形参inode为内存中的inode缓存
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny = false;
    pure_inode.inode_tag.next=pure_inode.inode_tag.prev = NULL;

    // 将pure.inode写入硬盘分区
    // 写硬盘时我们只需写pure_inode，但对硬盘只能以扇区为读写单位,因此我们需将inode所在的硬盘扇区
    // 读入内存缓冲区,将pure_inode写入硬盘缓冲区,再将内存缓冲区写入硬盘分区,缓冲区io_buf主调函数提供
    char* inode_buf = (char*)io_buf;
    if(inode_pos.two_sec){//inode跨了两扇区
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,2);//读两扇区
        memcpy((inode_buf+inode_pos.off_size),&pure_inode,sizeof(struct inode));//将pure_inode拼到这两个扇区中对应的inode位置
        ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,2);//写回
    }else{
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,1);//读1扇区
        memcpy((inode_buf+inode_pos.off_size),&pure_inode,sizeof(struct inode));//将pure_inode拼到这两个扇区中对应的inode位置
        ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,1);//写回
    }
}


// 根据inode结点编号,返回内存inode结点指针,若inode结点不在内存则读入硬盘分区part的对应inode到内存
struct inode* inode_open(struct partition* part,uint32_t inode_no){
    
    // 1. 首先在内存中分区的已打开inode链表中查找是否内存已有该inode
    struct list_elem* elem = part->open_inodes.head.next;       //遍历分区已打开(在内存中有inode的缓存)inode链表
    struct inode* inode_found;                                  //inode_no对应的硬盘分区的inode在内存的缓存
    while(elem != &part->open_inodes.tail){                     //遍历
        inode_found = elem2entry(struct inode,inode_tag,elem);  //将inode_tag链表元素转换为其所在结构体的指针inode_found
        if(inode_found->i_no == inode_no){
            inode_found->i_open_cnts++;                           //内存中该inode对应的文件打开的次数++
            return inode_found;
        }
        elem = elem->next;
    }

    // 2. 链表没有,从硬盘分区读取inode，同时加入分区已打开链表
    
    // 2.1 获取该inode_no对应的inode在硬盘分区的地址信息
    struct inode_position inode_pos;
    inode_locate(part,inode_no,&inode_pos);

    // 2.2 将inode加入到分区链表中(在内存中为inode开辟堆空间)
    // list_init(&cur_part->open_inodes);//挂载分区时，分区打开的文件(inode)为空,为实现
    // part->open_inodes分区打开的inodes链表被所有进程共享,我们需要在内核的物理内存池使用sys_malloc为inode分配内存空间
    // 在我们的memory.c中sys_malloc通过调用malloc_page分配物理页,sys_malloc根据cur_thread->pgdir==NULL即当前运行的进程或线程是否含有页表
    // 判断是内核线程还是用户进程并置标志PF,malloc_page根据形参enum pool_flags pf,调用palloc从对应的物理内存池分配物理页。
    // 因此我们只要将当前正在运行的进程或线程的 cur_thread->pgdir==NULL 即可在内核物理内存池分配内存

    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;         //备份当前进程线程的pgdir值，待分配完物理内存后再恢复
    cur->pgdir=NULL;                               //保证了在物理内核内存池分配内存
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    // 恢复pgdir
    cur->pgdir = cur_pagedir_bak;
    // 2.3读取硬盘inode
    char* inode_buf;
    if(inode_pos.two_sec){//inode占两扇区
        inode_buf = sys_malloc(SECTOR_SIZE*2);
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,2);
    }else{
        inode_buf = sys_malloc(SECTOR_SIZE*1);
        ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,1);
    }
    memcpy(inode_found,inode_buf+inode_pos.off_size,sizeof(struct inode));
    // 2.4将inode_found加入分区打开的inode链表首部
    list_push(&part->open_inodes,&inode_found->inode_tag);

    sys_free(inode_buf);
    return inode_found;
}


// 关闭inode或减少内存中inode打开的次数,每次执行该函数,将内存中该文件inode属性i_open_cnts-1,减为0时，释放内存中该inode所在堆空间
void inode_close(struct inode* inode){
    // 关中断防止进程调度对临界资源inode->i_open_cnts修改不一致
    enum intr_status old_status = intr_disable();
    inode->i_open_cnts--;
    if(inode->i_open_cnts==0){
        // 1. 分区打开的inode链表去除该inode
        list_remove(&inode->inode_tag);
        // 2. 释放inode所在内存内核物理堆空间
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pgdir;         //备份当前进程线程的pgdir值，待分配完物理内存后再恢复
        cur->pgdir=NULL;                               //保证了在物理内核内存池释放内存
        sys_free(inode);                                //释放inode所占内核堆空间
        cur->pgdir = cur_pagedir_bak;
    }
    intr_set_status(old_status);
}

// 初始化内存中的inode
void inode_init(uint32_t inode_no,struct inode* new_inode){
    new_inode->i_no = inode_no;
    new_inode->i_size =0;
    new_inode->i_open_cnts =0;
    new_inode->write_deny = false;

    // 初始化索引数组
    uint8_t sec_idx = 0;
    while(sec_idx<13){
        new_inode->i_blocks[sec_idx]=0;
        sec_idx++;
    }
}