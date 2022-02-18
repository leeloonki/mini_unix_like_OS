#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "fs.h"
#include "ide.h"

#define MAX_FILE_NAME_LEN   16  //最大文件名长度

// 目录也是文件，在硬盘分区上由数据块构成，这些数据块由目录项组成，
// 硬盘上不存在目录这种结构，只存在构成目录的目录项。
// 目录 数据结构只在内存中存。
struct dir{
    struct inode* inode;        //指向目录的inode(打开目录时,目录文件的inode被载入内存,由分区链表part->open_inodes维护)
    uint32_t dir_pos;           //记录在目录中的偏移，偏移目录项的整数倍，通过该偏移指向其他目录项
    uint8_t dir_buf[512];       //目录的数据缓存，如读取硬盘目录时，存储获取的目录项信息
};

// 目录项结构
struct dir_entry{
    char filename[MAX_FILE_NAME_LEN];//文件名或目录名
    uint32_t i_no;              //目录项的inode号
    enum file_types f_type;     //文件类型,定义在fs.h
};

void open_root_dir(struct partition* part);
struct dir* dir_open(struct partition* part,uint32_t inode_no);
bool search_dir_entry(struct partition*part,struct dir* pdir,const char* name,struct dir_entry* dir_entry);
void dir_close(struct dir* dir);
void create_dfir_entry(char* filename,uint32_t inode_no,uint8_t file_types,struct dir_entry* p_de);
bool sync_dir_entry(struct partition* part, struct dir* parent_dir,struct dir_entry* p_de,void* io_buf);
#endif