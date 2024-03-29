#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "print.h"
#include "debug.h"
#include "string.h"
#include "sync.h"
#include "interrupt.h"
#include "list.h"

#define MEM_BITMAP_BASE 0xc009a000  // 存放位图的(虚拟)地址
// 位图存放地址zai0xc009a000 - 0xc009e000 对应物理内存0x9a000 - 0x9e000
// 供4页物理内存，4*4K*8*4K = 4*128Mb 系统最大支持4个页框的位图

#define K_HEAP_BASE     0xc0100000  // 虚拟内核堆起始地址，内核动态申请空间的起始地址

#define PDE_IDX(addr)   ((addr&0xffc00000) >> 22)   //获取addr虚拟页的页表项
#define PTE_IDX(addr)   ((addr&0x003ff000) >> 12)   //获取addr虚拟页的页目录项

// 物理内存池
struct pool{
    struct bitmap pool_bitmap;  
    uint32_t phy_addr_start;    //本内存池管理的物理内存的起始地址
    uint32_t pool_size;         //本内存池字节容量
    struct lock lock;           //在多进程申请物理内存池的内存时，应做到互斥申请。我们对物理内存池加锁
};

struct virtual_addr kernel_vaddr;   // 用来管理内核虚拟地址分配
struct pool kernel_pool,user_pool;  // 管理内核内存池和用户内存池

// arena:位于页内存首部，属于元信息
struct arena{
    struct mem_block_desc* desc;            //此arena关联的描述符
    uint32_t cnt;                           //当large为true，cnt表示待分配内存块占的页数，否则表示空闲内存块mem_block数量
    bool large;                             //large为true表示分配的是>1024B内存块
};

struct mem_block_desc k_block_descs[DESC_CNT];   //内核的内存块描述符数组


// 初始化物理、虚拟内存池
static void mem_pool_init(uint32_t all_phy_men){
    // 形参 all_phy_mem为loader运行时获取的物理内存大小,其物理地址为0xb00
    put_str("mem_pool_init start\n");

    // 页目录表起始地址在0x100000处,PAGE_DIR_TABLE_POS  equ 0x100000 
    // 我们在loader中的页目录表项 为3-4G的内核虚拟空间额外分配了256-1个页表，其中第1023项页目录表
    // 指向了页目录表本身，因此页表加页目录表共 255 +1 = 256页
    
    // 物理内存池管理的物理内存大小
    uint32_t page_table_size = PG_SIZE *256;            //页目录表和页表所占物理内存 共1M 
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

    // 物理内存池结构体初始化
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

    // 初始化内存池对应的锁
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

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
        // 申请内核虚拟内存池
        // 申请pg_cnt页虚拟页
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);   //获取可用页的位图索引
        if(bit_idx_start == -1){
            return NULL;
        }
        while(cnt<pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;//分配的虚拟页起始：虚拟页起始 + 页*位图索引 
    }else{
        // 用户虚拟内存池
        struct task_struct*cur = running_thread();
        // 在当前进程的虚拟地址池申请pg_cnt页虚拟页
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
        if(bit_idx_start ==-1){
            return NULL;
        }
        while(cnt<pg_cnt){
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;//分配的虚拟页起始：虚拟页起始 + 页*位图索引 
        // 我们仿照Linux将进程虚拟地址空间4G的3-G给内核，用户最后一页内存3G-4K 到 3G 预留给用户的3级栈
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
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
    uint32_t* pde =(uint32_t*)( 0xfffff000 + PDE_IDX(vaddr) *4);
    return pde;
}

// 在m_pool管理的物理内存池中分配一个物理页
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

// 实现虚拟地址 _vaddr与物理地址 _page_phyaddr 映射,每次调用该函数在页表完成一个物理页 虚拟页映射
static void page_table_add(void* _vaddr,void* _page_phyaddr){
   uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
   uint32_t* pde = pde_ptr(vaddr);
   uint32_t* pte = pte_ptr(vaddr);

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

// 分配g_cnt个物理页空间，成功返回起始虚拟地址，失败返回NULL
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

// 在内核空间申请pg_cnt页内存(返回的地址可直接使用,已完成虚拟地址到页表的映射)
void* get_kernel_pages(uint32_t pg_cnt){
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr!=NULL){
        memset(vaddr,0,pg_cnt*PG_SIZE);
    }
    return vaddr;
}

// 在用户空间申请pg_cnt页内存,并返回虚拟地址
void* get_user_pages(uint32_t pg_cnt){
    // 多个进程共用一个物理内存池，内该内存池属于临界资源因此需要互斥访问
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER,pg_cnt);
    memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

// 指定一个虚拟页地址，申请一物理页并将虚拟页和物理页完成页表映射、
// 该函数与get_user_pages get_kernel_pages区别为该函数可以指定虚拟页地址vaddr
void* get_a_page(enum pool_flags pf,uint32_t vaddr){
    struct pool* mem_pool = pf&PF_KERNEL ? &kernel_pool:&user_pool;//获取是在内核还是用户物理池申请
    lock_acquire(&mem_pool->lock);
    // 将虚拟地址vaddr对应的位图置1 
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    if(cur->pgdir!=NULL && pf==PF_USER){
        // 进程的pg_dir不为NULL
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;//在位图中每页为1位，本行获取vaddr在位图中第多少位
        ASSERT(bit_idx>0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
    }else if(cur->pgdir==NULL && pf==PF_KERNEL){
        // 内核线程申请
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx>0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);

    }else{
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }
    void* page_phyaddr = palloc(mem_pool);//分配物理内存页
    if(page_phyaddr == NULL){
        return NULL;
    }
    // 使vaddr与物理内存页映射
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

// 获取虚拟地址映射到的物理地址
uint32_t addr_v2p(uint32_t vaddr){
    uint32_t* pte = pte_ptr(vaddr);//pte指向一个页表项地址
    // uint32_t high20 =  *pte & 0xfffff000;//页表项的高20位，去除页表项属性
    // uint32_t offset = vaddr & 0x00000fff;//获取虚拟地址的页内偏移
    // return high20 + offset;
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// 初始化内存块描述符数组 粒度 16 32 64 128 256 512 1024
void block_desc_init(struct mem_block_desc* desc_array){
    uint16_t desc_idx,block_size=16;
    for(desc_idx=0;desc_idx<DESC_CNT;desc_idx++){
        desc_array[desc_idx].block_size = block_size;
        desc_array[desc_idx].blocks_per_arena =(PG_SIZE - sizeof(struct arena)) / block_size;
        list_init(&desc_array[desc_idx].free_list);
        block_size*=2;                  //更新为下一规格内存块
    }
}


// 返回arena中第idx个内存块的地址
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

// 返回mem_block所在arena页
static struct arena* block2arena(struct mem_block* b){
    return (struct arena*)((uint32_t)b & 0xfffff000);
}


// 在堆中申请size字节内存
void* sys_malloc(uint32_t size){
    enum pool_flags PF;
    uint32_t pool_size;             //内存池的字节容量
    struct pool* mem_pool;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    if(cur_thread->pgdir==NULL){    //内核线程pd_dir空
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;//内核物理内存池字节
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }else{
        PF = PF_USER;
        pool_size = user_pool.pool_size;//内核物理内存池字节
        mem_pool = &user_pool;
        descs = cur_thread->u_block_descs;
    }

    // 如果申请的size大于内存池的size，或者小于0则返回
    if(!(size>0&&size<pool_size)){
        return NULL;
    }
    struct arena* a;
    struct mem_block* b;

    lock_acquire(&mem_pool->lock);
    if(size>1024){ //分配大内存
        // 计算该size占用几页 对size向上取整
        uint32_t page_cnt = DIV_ROUND_UP(size +sizeof(struct arena),PG_SIZE);
        a = malloc_page(PF,page_cnt);
        if(a!=NULL){
            memset(a,0,page_cnt*PG_SIZE);
            a->desc = NULL;//大内存块无desc
            a->cnt = page_cnt;
            a->large = true;        
            lock_release(&mem_pool->lock);
            return (void*)(a+1);            //返回给进程或线程使用的去除元信息arena头部后的可用内存地址  (a+1)跨过sizeof(arena)字节
        }else{
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }else{ //分配小内存
        uint8_t desc_idx; //size对于何种粒度内存块
        for(desc_idx=0;desc_idx< DESC_CNT;desc_idx++){
            if(size <=descs[desc_idx].block_size){
                break;
            }
        }
        // 此时从内存块描述符desc_idx项分配内存
        
        //首先判断是否descs[desc_idx]. free_list是否为空
        if(list_empty(&descs[desc_idx].free_list)){
            // 如果该粒度内存块为空，则再申请一页内存并初始化该页的arena
            a = malloc_page(PF,1);
            if(a==NULL){
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a,0,PG_SIZE);
            // 初始化该页的arena
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;

            // 将该页内存划分为内存块，并插入到desc的 free_list中
            // 内核线程共用一个descs，因此需要关闭中断
            uint32_t block_idx;
            enum intr_status old_status = intr_disable();
            
            for(block_idx=0;block_idx<descs[desc_idx].blocks_per_arena;block_idx++){
                b = arena2block(a,block_idx);
                ASSERT(!elem_find(&a->desc->free_list,&b->free_elem));
                list_append(&a->desc->free_list,&b->free_elem);
            }
            intr_set_status(old_status);
        }

        // 开始分配块内存,根据struct mem_block结构体地址和成员struct list_elem地址，
        // b既是分配出去的block首地址
        b = elem2entry(struct mem_block,free_elem,list_pop(&(descs[desc_idx].free_list)));    
        memset(b,0,descs[desc_idx].block_size);
        a = block2arena(b);
        a->cnt--;
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

// 将物理页回收到物理内存池
void pfree(uint32_t pg_phy_addr) {
   struct pool* mem_pool;
   uint32_t bit_idx = 0;
   if (pg_phy_addr >= user_pool.phy_addr_start) {     // 用户物理内存池
      mem_pool = &user_pool;
      bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
   } else {	  // 内核物理内存池
      mem_pool = &kernel_pool;
      bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
   }
   bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);	 // 将位图中该位清0
}
// 去除虚拟页vaddr的映射，将vaddr对应的pte  p置0
static void page_table_pte_remove(uint32_t vaddr){
    uint32_t* pte = pte_ptr(vaddr);     //获取该vaddr对应的pte指针
    *pte &= ~PG_P_1;                    //将p位置0
    asm volatile ("invlpg %0"::"m"(vaddr):"memory");                         //刷新TLB vaddr对应的缓存
}


// 清空虚拟内存池 vaddr起始对应的位图的 pg_cnt个位
static void vaddr_remove(enum pool_flags pf,void* _vaddr, uint32_t pg_cnt){
    uint32_t bit_idx_start =0,vaddr = (uint32_t)_vaddr,cnt=0;
    if(pf==PF_KERNEL){
        bit_idx_start = (vaddr-kernel_vaddr.vaddr_start)/PG_SIZE;
        while(cnt<pg_cnt){
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start+ cnt,0);
            cnt ++;
        }
    }else{
        struct task_struct* cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start)/PG_SIZE;
        while(cnt<pg_cnt){
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,bit_idx_start+ cnt,0);
            cnt ++;
        }
    }
}

// 释放以虚拟地址vaddr 起始的cnt个物理页框
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt){
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
    ASSERT(pg_cnt >=1 && vaddr % PG_SIZE == 0); 
    pg_phy_addr = addr_v2p(vaddr);  // 获取虚拟地址vaddr对应的物理地址

    //回收的物理内存必须大于1M +4K +4K = 0x100000 +0x1000 +0x1000 = 0x102000;
    ASSERT((pg_phy_addr % PG_SIZE)==0 && pg_phy_addr >= 0x102000);
    // 判断pg_phy_addr在用户还是内核物理内存池
    if( pg_phy_addr >= user_pool.phy_addr_start){//位于用户物理内存池
        // 
        vaddr-=PG_SIZE;
        while(page_cnt<pg_cnt){                 //回收vaddr开始的pg_cnt个用户物理页
            vaddr+=PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PG_SIZE)==0 && pg_phy_addr >=user_pool.phy_addr_start);
            pfree(pg_phy_addr);                 //1.将物理页框归还到内存池
            //2.从页表中清除该页虚拟地址对应的pte表项的p位为0
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        // 3.清空虚拟地址位图中的cnt个相应位
        vaddr_remove(pf,_vaddr,pg_cnt);
    }else{
        vaddr-=PG_SIZE;
        while(page_cnt<pg_cnt){                 //回收vaddr开始的pg_cnt个用户物理页
            vaddr+=PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PG_SIZE)==0 && pg_phy_addr >=kernel_pool.phy_addr_start && \
            pg_phy_addr < user_pool.phy_addr_start);
            pfree(pg_phy_addr);                 //1.将物理页框归还到内存池
            //2.从页表中清除该页虚拟地址对应的pte表项的p位为0
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        // 3.清空虚拟地址位图中的cnt个相应位
        vaddr_remove(pf,_vaddr,pg_cnt);
    }
}

// 回收ptr指向的内存
void sys_free(void* ptr){
    ASSERT(ptr!=NULL);
    if(ptr!=NULL){
        enum pool_flags PF;
        struct pool* mem_pool;
        // 判断是进程还是线程
        if(running_thread()->pgdir==NULL){//线程
            ASSERT((uint32_t)ptr >= K_HEAP_BASE);
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        }else{
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);
        struct mem_block* b = ptr;
        struct arena*a = block2arena(b);
        ASSERT(a->large ==0 || a->large==1);
        if(a->desc == NULL && a->large==true){  //大内存
            mfree_page(PF,a,a->cnt);
        }else{
            // 1.先将小内存块回收到free_list
            list_append(&a->desc->free_list,&b->free_elem);
            // 2.判断此arena(1页物理内存)的内存块是否都未使用,若是，则释放此arena
            a->cnt++;//更新目前此arena的内存块数
            if(a->cnt == a->desc->blocks_per_arena){//如果arena中内存块都空闲，
            // 则在该粒度的内存块描述符desc链表去除属于该arena的元素，并释放该arena
                uint32_t block_idx;
                for(block_idx=0;block_idx<a->desc->blocks_per_arena;block_idx++){//去除链表所有属于该arena的mem_block元素
                    struct mem_block* b = arena2block(a,block_idx);
                    ASSERT(elem_find(&a->desc->free_list,&b->free_elem));
                    list_remove(&b->free_elem);
                }
                mfree_page(PF,a,1); //回收该arena
            }
        }
        lock_release(&mem_pool->lock);
    }
}

void mem_init(){
    put_str("mem_init start\n");
    uint32_t phy_mem_bytes_total = *(uint32_t*)(0xb00);
    mem_pool_init(phy_mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done\n"); 
}
