#include "memory.h"
#include "print.h"
#define PG_SIZE 4096                // 每页4k

#define MEM_BITMAP_BASE 0xc009a000  // 存放位图的(虚拟)地址
// 位图存放地址zai0xc009a000 - 0xc009e000 对应物理内存0x9a000 - 0x9e000
// 供4页物理内存，4*4K*8*4K = 4*128Mb 系统最大支持4个页框的位图

#define K_HEAP_BASE     0xc0100000  // 内核堆起始地址，内核动态申请空间的起始地址

struct virtual_addr kernel_vaddr;   // 用来管理内核虚拟地址分配
struct pool kernel_pool,user_pool;  // 管理内核内存池和用户内存池

// 初始化物理、虚拟内存池
static void mem_pool_init(uint32_t all_phy_men){
    // 形参 all_phy_mem为loader运行时获取的物理内存大小,其物理地址为0xb00
    put_str("mem_pool_init start\n");

    // 页目录表起始地址在0x100000处,PAGE_DIR_TABLE_POS  equ 0x100000 
    // 我们在loader中的页目录表项 为3-4G的内核虚拟空间额外分配了256-1个页表，其中第1023项页目录表
    // 指向了页目录表本身，因此页表加页目录表共 255 +1 = 256页
    
    // 物理内存池管理的物理内存大小
    uint32_t page_table_size = PG_SIZE *256;    //
    uint32_t used_phy_mem = page_table_size + 0x100000; //物理内存最低1M + 页表（从1M起始开始）使用的内存
    uint32_t free_phy_mem = all_phy_men - used_phy_mem; //给内存池管理的内存大小
    uint16_t all_free_phy_pages = free_phy_mem / PG_SIZE;//所有空闲的物理页个数，对不足1页的内存不在考虑
    uint16_t kernel_free_phy_pages = all_free_phy_pages / 2;//我们将物理内存池和用户内存池分别分配一半
    uint16_t user_free_phy_pages = all_free_phy_pages - kernel_free_phy_pages;

    // 物理内存池位图
    uint32_t kbm_length = kernel_free_phy_pages / 8;    //内核物理位图长度
    uint32_t ubm_length = user_free_phy_pages / 8;       //用户物理位图长度
    uint32_t kp_start = used_phy_mem;                   //内核管理的物理内存的起始地址
    uint32_t up_start = kp_start + kernel_free_phy_pages * PG_SIZE;

    // 内存池结构体初始化
    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;
    kernel_pool.pool_size = kernel_free_phy_pages * PG_SIZE;
    user_pool.pool_size = user_free_phy_pages * PG_SIZE;
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE; 
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);//用户物理内存位图在内核物理内存位图之后

    put_str("kernel_pool_bitmap_start:");put_int((uint32_t)(kernel_pool.pool_bitmap.bits));
    put_str("\n");
    put_str("kernel_pool_phy_addr_start:");put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("user_pool_bitmap_start:");put_int((uint32_t)(user_pool.pool_bitmap.bits));
    put_str("\n");
    put_str("user_pool_phy_addr_start:");put_int(user_pool.phy_addr_start);
    put_str("\n");
    
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    // 对内核虚拟地址池初始化
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;  //内核虚拟地址池和内核物理内存池大小一致
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);    //内核虚拟位图在内核用户物理位图之后
    kernel_vaddr.vaddr_start = K_HEAP_BASE;                 //内核虚拟池起始地址在3G开始1M后
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("mem_poop_init done\n");    
}


void mem_init(){
    put_str("mem_init start\n");
    uint32_t phy_mem_bytes_total = *(uint32_t*)(0xb00);
    mem_pool_init(phy_mem_bytes_total);
    put_str("mem_init done\n"); 
}
