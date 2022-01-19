#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "stdint.h"

#define BITMAP_MASK 1

struct bitmap{
    uint32_t btmp_bytes_len;    //位图总字节数(反映表示资源数目的多少)
    uint8_t* bits;              // bits 位图起始地址指针，指针指向的数据类型为uint8类型，
                                // 位图以8位一组便于组内位的操作    
};

// 位图初始化(根据btmp中位图起始地址和位图字节数将这片区域内存置为0)
void bitmap_init(struct bitmap* btmp);

// 判断位图中bit_idx位是否为1
bool bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx);

// 在位图中连续申请cnt个位，成功：返回起始位的下标，失败：返回-1
int bitmap_scan(struct bitmap* btmp,uint32_t cnt);

// 将位图中bit_idx位置为value(0 || 1)
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value);
#endif