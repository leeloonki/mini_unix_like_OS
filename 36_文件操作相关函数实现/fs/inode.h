#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "global.h"         //bool类型定义
#include "list.h"
#include "ide.h"
#include "stdint.h"
struct inode{
    uint32_t i_no;          //inode编号
    uint32_t i_size;        //当inode描述的文件是普通文件，i_size指文件大小；当inode描述的文件为目录，i_size指该目录下所有目录项大小之和。
    uint32_t i_open_cnts;   //记录本文件被打开的次数
    bool write_deny;        //写文件时只能串行写入，写文件前，进程检查该标识
    uint32_t i_blocks[13];  //组成文件的块的索引表，i_blocks[0-11]是直接块 i_blocks[12] 是一级间接块指针
    struct list_elem inode_tag;//我们使用文件时需要在内存记录打开了哪些文件，通过定义链表和inode的链表元素，将进程打开的文件链接起来
};

void inode_sync(struct partition* part,struct inode* inode,void* io_buf);
struct inode* inode_open(struct partition* part,uint32_t inode_no);
void inode_close(struct inode* inode);
void inode_init(uint32_t inode_no,struct inode* new_inode);
#endif