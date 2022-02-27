#ifndef __FS_FS_H
#define __FS_FS_H
#include "stdint.h"

#define MAX_FILES_PER_PART  4096    //每个分区支持的最大文件数，即inode个数
#define BITS_PER_BLOCK      4096    //每块的位数 512*8
#define SECTOR_SIZE         512     
#define BLOCK_SIZE  SECTOR_SIZE     //块大小1扇区

// 最大路径长度
#define MAX_PATH_LEN 256


enum file_types{
    FT_UNKNOWN,         //不支持文件类型    0,删除文件后,也会将目录中的目录项的f_type置0
    FT_REGULAR,         //普通文件          1
    FT_DIRECTORY        //目录文件          2
};


struct path_search_record{
    char searched_path[MAX_PATH_LEN];   //查找过程中存储父路径
    struct dir* parent_dir;             //文件或目录的上级父目录
    enum file_types file_type;          //检索得到的文件类型
};

extern struct partition* cur_part;//操作分区
int32_t path_depth_cnt(char* pathname);
void filesys_init();

#endif