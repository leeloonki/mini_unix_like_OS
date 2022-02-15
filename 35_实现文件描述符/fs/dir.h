#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "fs.h"
#define MAX_FILE_NAME_LEN   16  //最大文件名长度

// 硬盘上的目录是数据块，由目录项组成，不存在目录结构，只存在构成目录的目录项。
// 内存中的数据结构
struct dir{
    struct inode* inode;        //指向目录项的inode，便于对目录下的目录项操作
    uint32_t dir_pos;           //记录在目录中的偏移，偏移目录项的整数倍，通过该偏移指向其他目录项
    uint8_t dir_buf[512];       //目录的数据缓存，如读取硬盘目录时，存储获取的目录项信息
};

// 目录项结构
struct dir_entry{
    char filename[MAX_FILE_NAME_LEN];//文件名或目录名
    uint32_t i_no;              //目录项的inode号
    enum file_types f_type;     //文件类型,定义在fs.h
};

#endif