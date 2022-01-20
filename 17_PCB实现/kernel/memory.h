#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include"bitmap.h"
#include"global.h"
#include "stdint.h"
#include"memory.h"

enum pool_flags{
    PF_KERNEL = 1,              //内核内存池标记
    PF_USER = 2                 //用户内存池标记
};

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


// PDE PTE 属性位
#define PG_P_1  1   //页表项或页目录表项存在位
#define PG_P_0  0
#define PG_RW_R 0   //RW属性 读、执行  00
#define PG_RW_W 2   //RW属性 读、写、执行  10
#define PG_US_S  0   //US属性 系统级 000
#define PG_US_U  4   //US属性 用户级 100

void mem_init();
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
#endif