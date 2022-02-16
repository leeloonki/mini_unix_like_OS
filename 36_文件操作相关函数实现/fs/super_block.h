#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

//我们定义一个块即一个扇区
#include "stdint.h"
// 超级块，占用一个块(一个扇区)

struct super_block{
    uint32_t magic;             //魔数，标识我们的文件系统类型
    uint32_t block_cnt;         //分区中总的块数
    uint32_t inode_cnt;         //本分区中inode项数
    uint32_t part_lba_base;     //本分区起始lba地址
    // 块位图
    uint32_t block_bitmap_lba;  //块位图的起始lba地址
    uint32_t block_bitmap_block;//块位图占据的块数
    // inode位图
    uint32_t inode_bitmap_lba;  //inode位图的起始lba地址
    uint32_t inode_bitmap_block;//inode位图占据的块数
    // inode表
    uint32_t inode_table_lba;   //inode表起始扇区地址
    uint32_t inode_table_block; //inode表占据的块数
    // 数据区块
    uint32_t data_start_lba;    //数据区(data region)开始的第一个块的lba地址
    uint32_t root_inode_no;     // 根目录"/"的inode号，位置固定，初始化时指定
    uint32_t dir_entry_size;    //目录项的大小

    // 以上数据成员共13*4字节，为了使该数据结构占据一个块512字节大小，我们填充剩下的字节
    uint8_t pad[460];               //512 - 52 = 460B
}__attribute__((packed));       //保证编译后，该数据结构变量占用512字节

#endif