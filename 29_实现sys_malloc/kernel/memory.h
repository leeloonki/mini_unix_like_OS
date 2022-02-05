#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include"bitmap.h"
#include"stdint.h"
#include"list.h"

// 用于虚拟地址管理
struct virtual_addr{
    struct bitmap vaddr_bitmap; //管理虚拟地址用到的位图
    uint32_t vaddr_start;       //虚拟地址起始地址
};

enum pool_flags{
    PF_KERNEL = 1,              //内核内存池标记
    PF_USER = 2                 //用户内存池标记
};


// 小内存块
struct mem_block{
    struct list_elem free_elem; //通过list_elem结点组织内存块
};

// 内存块描述符  同种粒度的多个arena共用一个该描述符。
struct mem_block_desc{
    uint32_t block_size;        //内存块粒度
    uint32_t blocks_per_arena;  //每一个arena含有的可分配的该力粒度的物理块
    struct list free_list;      //组织某种粒度的所有内存块
};
// 小内存块粒度共16 32 64 128 256 512 1024 7种，对于每种粒度都需要描述符进行管理
#define DESC_CNT 7

// PDE PTE 属性位
#define PG_P_1  1   //页表项或页目录表项存在位
#define PG_P_0  0
#define PG_RW_R 0   //RW属性 读、执行  00
#define PG_RW_W 2   //RW属性 读、写、执行  10
#define PG_US_S  0   //US属性 系统级 000
#define PG_US_U  4   //US属性 用户级 100


uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);
void mem_init();


#endif