#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"
#include "inode.h"
// 系统已打开文件表表项file结构体
struct file{
    uint32_t fd_offset;     //记录当前打开硬盘文件的操作的偏移地址
    uint32_t fd_mode;       //文件表示操作如只读、读写...
    struct inode* fd_iptr;  //inode指针,指向内存中维护的已打开inode链表中的inode。
};


// 标准输入输出描述符
enum std_fd{
    stdin_no,       //0标准输入
    stdout_no,      //1标准输出
    stderr_no       //2标准错误
};

// 位图类型
enum bitmap_type{
    INODE_BITMAP,       //inode位图
    BLOCK_BITMAP        //块位图
};


#define MAX_FILE_OPEN 32    //系统支持最大打开文件个数32，系统文件表SFT表项
int32_t get_free_slot_in_global();
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
void bitmap_sync(struct partition* part,uint32_t bit_idx,uint8_t btmp_type);
#endif