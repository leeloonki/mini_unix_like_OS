#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILES_PER_PART  4096    //每个分区支持的最大文件数，即inode个数
#define BITS_PER_BLOCK      4096    //每块的位数 512*8
#define SECTOR_SIZE         512     
#define BLOCK_SIZE  SECTOR_SIZE     //块大小1扇区

enum file_types{
    FT_UNKNOWN,         //不支持文件类型
    FT_REGULAR,         //普通文件
    FT_DIRECTORY        //目录文件
};

void filesys_init();

#endif