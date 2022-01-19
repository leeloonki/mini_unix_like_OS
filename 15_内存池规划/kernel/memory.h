#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include"bitmap.h"
#include"stdint.h"
#include"global.h"
// 用于虚拟地址管理
struct virtual_addr{
    struct bitmap vaddr_bitmap; //管理虚拟地址用到的位图
    uint32_t vaddr_start;       //虚拟地址起始地址
};

// 用于物理内存管理
struct pool{
    struct bitmap pool_bitmap;  
    uint32_t phy_addr_start;    //本内存池管理的物理内存的起始地址
    uint32_t pool_size;         //本内存池字节容量
};

void mem_init();
#endif