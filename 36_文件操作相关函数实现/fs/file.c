#include "file.h"
#include "stdint.h"
#include "global.h" //NULL定义
#include "stdio-kernel.h"// printk
#include "thread.h"
#include "fs.h"
// 系统打开文件表SFT
struct file file_table[MAX_FILE_OPEN];

// 在文件表file_table[]中获取一个空闲元素(成员的iptr空)，成功返回其下标，失败返回-1
int32_t get_free_slot_in_global(){
    uint32_t fd_idx = 3;    //0 1 2预留
    while(fd_idx <MAX_FILE_OPEN){
        if(file_table[fd_idx].fd_iptr == NULL){
            break;
        }
        fd_idx++;
    }
    if(fd_idx == MAX_FILE_OPEN){
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

// 将全局描述符下标安装到进程或线程自己的文件描述符表中
// 形参global_fd_idx为在全局描述符表申请的表项下标
// 成功返回进程或线程文件表下标,失败返回-1
int32_t pcb_fd_install(int32_t global_fd_idx){
    struct task_struct* cur = running_thread();//获取当前任务
    uint8_t local_fd_idx = 3;   //进程的fd描述符0-2已预留，init_thread时已初始化0-2
    while(local_fd_idx<MAX_FILES_OPEN_PER_PROC){
        if(cur->fd_table[local_fd_idx]==-1){ //-1即表项可使用
            cur->fd_table[local_fd_idx]=global_fd_idx;
            break;
        }
        local_fd_idx++;
    }
    if(local_fd_idx == MAX_FILES_OPEN_PER_PROC){
        printk("exceed max open files_per_proc\n");
        return -1;
    }
    return local_fd_idx;
}

// 在已挂载分区part(内存中channels[].devices[].prim_parts[])的inode位图中分配一inode位,并返回i结点号
// 该函数操作的是内存中的inode位图,还需要将位图写回磁盘
int32_t inode_bitmap_alloc(struct partition* part){
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap,1);
    if(bit_idx == -1){
        return -1;      //
    }
    bitmap_set(&part->inode_bitmap,bit_idx,1);//首先将内存中读入的硬盘分区的inode位图的该位置1,之后再将内存inode位图写回硬盘分区即可
    return bit_idx;
}

// 分配分区part一个块,返回块在硬盘分区的 LBA地址
// 此函数还是操作的内存中的分区part的block位图
int32_t block_bitmap_alloc(struct partition* part){
    int32_t bit_idx = bitmap_scan(&part->block_bitmap,1);
    if(bit_idx == -1){
        return -1;      
    }
    bitmap_set(&part->block_bitmap,bit_idx,1);
    return (part->sb->data_start_lba + bit_idx);//我们系统每块表示1扇区,因此bit_idx + 数据块的起始lba地址即可
}

// 将内存的位图的第bit_idx(inode 或者 block的位)同步到硬盘分区中
// 形参bit_idx为内存中位图bitmap的第bit_idx位
// 形参btmp_type表示要写入的位图类型
void bitmap_sync(struct partition* part,uint32_t bit_idx,uint8_t btmp_type){
    uint32_t off_block = bit_idx/MAX_FILES_PER_PART;//off_block为bit_idx相对于位图的扇区(块)偏移量
    uint32_t off_size = off_block * BLOCK_SIZE;     //off_size为bit_idx相对于位图的字节偏移量

    uint32_t sec_lba;           //写入扇区的lba
    uint8_t* bitmap_off;        //内存中bit_idx所在的内存地址
    switch(btmp_type){
        case INODE_BITMAP:
            // part->inode_bitmap.bits为内存中分区的inode起始内存地址
            sec_lba = part->sb->inode_bitmap_lba + off_block;
            bitmap_off = part->inode_bitmap.bits + off_size;//位图起始内存地址+位图偏移的字节数
            break;
        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_block;
            bitmap_off = part->block_bitmap.bits + off_size;
            break;
    }
    ide_write(part->my_disk,sec_lba,bitmap_off,1);//将内存地址bitmap_off开始的1扇区写入硬盘扇区sec_lba
}
