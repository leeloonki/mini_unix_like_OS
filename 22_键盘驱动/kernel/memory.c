#include "memory.h"
#include "print.h"
#include "debug.h"
#include "string.h"
#define PG_SIZE 4096                // 每页4k

#define MEM_BITMAP_BASE 0xc009a000  // 存放位图的(虚拟)地址
// 位图存放地址zai0xc009a000 - 0xc009e000 对应物理内存0x9a000 - 0x9e000
// 供4页物理内存，4*4K*8*4K = 4*128Mb 系统最大支持4个页框的位图

#define K_HEAP_BASE     0xc0100000  // 内核堆起始地址，内核动态申请空间的起始地址

#define PDE_IDX(addr)   ((addr&0xffc00000) >> 22)   //获取addr虚拟页的页表项
#define PTE_IDX(addr)   ((addr&0x003ff000) >> 12)   //获取addr虚拟页的页目录项
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
    put_str("mem_pool_init done\n");    
}

// 在pf表示的虚拟内存池中分配pg_cnt个虚拟页
// 成功返回虚拟页起始地址  失败返回NULL
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
    int vaddr_start = 0;
    int bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf ==PF_KERNEL){
        bit_idx_start = bitmap_scan(&kernel_pool.pool_bitmap,pg_cnt);   //获取可用页的位图索引
        if(bit_idx_start == -1){
            return NULL;
        }
        while(cnt<pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;//分配的虚拟页起始：虚拟页起始 + 页*位图索引 
    }else{
        // 用户虚拟内存池
    }
    return (void*)vaddr_start;
}

// 获取虚拟地址vaddr对应的pte(页表项)指针
uint32_t* pte_ptr(uint32_t vaddr){
    // 首先：页目录表的最后一项(1023)页目录项指向的是页目录表本身
    // pte作为一个32位地址
    //    高10位 1111 1111 11    即dir = 1023 最后一个页目录项 指向页目录本身
    //    中10位 vaddr高10位     作为实际页目录项索引
    //    低12位 使用vaddr中间10位 手动* 4 作为实际页表项索引   
    uint32_t* pte =(uint32_t*)( 0xffc00000 + ((vaddr & 0xffc00000) >>10 ) + PTE_IDX(vaddr) *4);
    return pte;
}

// 获取虚拟地址vaddr对应的pde(页目录表项)指针
uint32_t* pde_ptr(uint32_t vaddr){
    // 首先：页目录表的最后一项(1023)页目录项指向的是页目录表本身
    // pde作为一个32位地址
    //    高10位 1111 1111 11    即dir = 1023 最后一个页目录项 指向页目录本身
    //    中10位 1111 1111 11    即dir = 1023 最后一个页目录项 指向页目录本身（套娃）
    //    低十位 PDE_IDX(vaddr) *4
    uint32_t* pte =(uint32_t*)( 0xfffff000 + PDE_IDX(vaddr) *4);
    return pte;
}

// 在m_pool管理的内存池中分配一个物理页
// 成功返回页框物理地址，失败返回NULL
static void* palloc(struct pool* m_pool){
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);   //找到一个物理页
    if(bit_idx == -1){
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);         //找到一页
    uint32_t page_phyaddr = m_pool->phy_addr_start + bit_idx * PG_SIZE;
    return (void*)page_phyaddr;
}

// 页表中添加虚拟地址 _vaddr与物理地址 _page_phyaddr 映射,每次调用该函数完成一个物理页 虚拟页映射
static void page_table_add(void* _vaddr,void* _page_phyaddr){
    uint32_t vaddr =(uint32_t)_vaddr;
    uint32_t page_phyaddr =(uint32_t)_page_phyaddr;
    uint32_t*pde = pde_ptr(vaddr);
    uint32_t*pte = pte_ptr(vaddr);

    if(*pde & 0x00000001){//1 页表项或页目录表项存在位
        ASSERT(!(*pte & 0x00000001));
        if(!(*pte& 0x00000001)) {//页表项不存在，开始添加物理页映射
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }else{
            PANIC("pte repeat");

        }
    }else{//页目录项不存在，则一律从内核物理内存池分配
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  //设置新分配的页目录项属性

        memset((void*) ((uint32_t) pte & 0xfffff000 ),0,PG_SIZE);//初始化该物理页

        ASSERT(!(*pte& 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

// 分配g_cnt个页空间，成功返回起始虚拟地址，失败返回NULL
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt){
    // 1.通过vaddr_get在虚拟内存池申请虚拟页
    // 2.通过palloc 在物理内存池申请物理页
    // 3.通过page_table_add 将1 2得到的页在页表中完成映射
    void* vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start == NULL){
        return NULL;
    }
    uint32_t vaddr = (uint32_t)vaddr_start;
    uint32_t cnt = pg_cnt;
    struct pool* mem_pool = pf&PF_KERNEL ? &kernel_pool:&user_pool;
    while(cnt-->0){
        void* page_phyaddr = palloc(mem_pool);
        if(page_phyaddr == NULL){
            return NULL;    //申请失败时返回NULL，要么cnt个要么0个  
        }
        page_table_add((void*)vaddr,page_phyaddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

// 
void* get_kernel_pages(uint32_t pg_cnt){
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PG_SIZE);
    }
    return vaddr;
}

void mem_init(){
    put_str("mem_init start\n");
    uint32_t phy_mem_bytes_total = *(uint32_t*)(0xb00);
    mem_pool_init(phy_mem_bytes_total);
    put_str("mem_init done\n"); 
}
