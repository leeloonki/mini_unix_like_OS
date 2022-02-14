#include "fs.h"
#include "dir.h"
#include "inode.h"
#include "super_block.h"
#include "ide.h"
#include "global.h"     //向上取整宏定义
#include "stdio-kernel.h"
#include "memory.h"
#include "string.h"
#include "debug.h"
// 格式化分区：初始化分区元信息，创建文件系统
static void partition_format(struct partition* part){
    uint32_t boot_block_block =1;      //引导块1扇区
    uint32_t super_block_block =1;      //超级块1扇区
    uint32_t inode_bitmap_block = DIV_ROUND_UP(MAX_FILES_PER_PART,BITS_PER_BLOCK);//inode 4096项，每项占用1位，4096/块数 = 所占块数
    uint32_t inode_table_block = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)),BLOCK_SIZE);
    uint32_t used_blocks = boot_block_block + super_block_block + inode_bitmap_block + inode_table_block;
    uint32_t free_blocks = part->sec_cnt - used_blocks;
    
    // 计算块位图所占块数
    uint32_t block_bitmap_blocks;       //块 位图 占据块数    
    block_bitmap_blocks = DIV_ROUND_UP(free_blocks,BITS_PER_BLOCK);
    uint32_t block_bitmap_bit_len = free_blocks - block_bitmap_blocks;//去除块位图后，空闲块个数
    block_bitmap_blocks = DIV_ROUND_UP(block_bitmap_bit_len,BITS_PER_BLOCK);

    // 超级块初始化 
    struct super_block sb;
    sb.magic = 0x4c49;                  //自定魔数
    sb.block_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;//分区lba地址

    // boot_block | super_block | block_bitmap_block | inode_bitmap_block | data_block
    sb.block_bitmap_lba = sb.part_lba_base + 2;
    sb.block_bitmap_block = block_bitmap_blocks;

    // inode位图
    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_block;
    sb.inode_bitmap_block = inode_bitmap_block;

    // inode table
    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_block;
    sb.inode_table_block = inode_table_block;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_block;

    sb.root_inode_no = 0;               //根目录占据inode编号0
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n",part->name);
    printk("    magic:0x%x\n    part_lba_base:0x%x\n    all_sectors:0x%x\n    inode_cnt:0x%x\n    block_bitmap_lba:0x%x\n"
    "    block_bitmap_blocks:0x%x\n    inode_bitmap_lba:0x%x\n    inode_bitmap_blocks:0x%x\n    inode_table_lba:0x%x\n    inode_table_blocks:0x%x\n"
    "    data_start_lba:0x%x\n"
    "",sb.magic,sb.part_lba_base,sb.block_cnt,sb.inode_cnt,sb.block_bitmap_lba,sb.block_bitmap_block,sb.inode_bitmap_lba,sb.inode_bitmap_block,sb.inode_table_lba,sb.inode_table_block,sb.data_start_lba);

    //1.将超级块写入该分区的第一扇区
    struct disk* hd = part->my_disk;
    ide_write(hd,part->start_lba+1,&sb,1);  
    printk("    super_block_lba:0x%x\n",part->start_lba+1);
    // 定义写入硬盘的内存缓冲区，在该缓冲区定义数据结构，写入硬盘
    // 我们还需要写入块位图、inode位图、inode表,因此缓冲区必须能容纳该三类数据的最大字节
    uint32_t buf_size = (sb.block_bitmap_block >=sb.inode_bitmap_block? sb.block_bitmap_block:sb.inode_bitmap_block);//比较块位图和inode位图谁大
    buf_size = (buf_size >= sb.inode_table_block ? buf_size:inode_table_block)*BLOCK_SIZE;//缓冲区大小

    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);//sys_malloc申请的内存初始全为0
    
    // 2.将块位图初始化并写入硬盘分区的块位图所在块
    buf[0] |=0x01;          //第0个数据块给根目录(第0个inode也同时给根目录),位图1标识被占用，0标识空闲
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len /8;  //块位图最后一字节需要特殊处理
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len %8;
    uint32_t last_size = BLOCK_SIZE - (block_bitmap_last_byte % BLOCK_SIZE);//last_size是位图所在最后一 block中不足1block的剩余部分。
    // 2.1将块位图的最后一块(block)中 最后一个包含块位图的字节和其后的无实际数据块对应的位全部置1
    memset(&buf[block_bitmap_last_byte],0xff,last_size);        
    // 2.1再将最后一个包含块位图的字节的有效位bit重新置0
    uint8_t bit_idx = 0;
    while (bit_idx<=block_bitmap_last_bit){
        buf[block_bitmap_last_byte] &= ~(1<<bit_idx++);//与0
    }
    ide_write(hd,sb.block_bitmap_lba,(void*)buf,sb.block_bitmap_block);

    // 3.将inode位图写入硬盘对应扇区
    // 先清空buf
    memset(buf,0,buf_size);
    buf[0] |= 0x01;         //第0个inode给根目录"/"
    ide_write(hd,sb.inode_bitmap_lba,(void*)buf,sb.inode_bitmap_block);//inode刚好4096位即1个块大小

    // 4.将inode数组初始化，写入硬盘
    // 初始化inode的第0项即根目录的inode
    memset(buf,0,buf_size);
    struct inode* i = (struct inode*)buf;   //写硬盘时从buf开始写入，写到inode表开始的块，inode起始为第0个inode，我们这里初始化的i将写到inode表第0项
    i->i_size = sb.dir_entry_size *2;       // .   ..
    i->i_no = 0;                            //根目录占inode数组第0个
    i->i_blocks[0]=sb.data_start_lba;       //根目录中的目录项存储在可用数据块的第0个，预先给根目录分配一个数据块,上面在初始化块位图也已经buf[0] |=0x01; 
    ide_write(hd,sb.inode_table_lba,buf,sb.inode_table_block);

    // 5.将根目录初始化，并将目录项写入根目录所在的sb.data_start_lba
    memset(buf,0,buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 初始化当前目录  "."
    memcpy((void*)p_de->filename,".",1);
    p_de->i_no = 0;                     //  "."是当前目录，因此目录项的inode是0
    p_de->f_type = FT_DIRECTORY;
    p_de++;
    // 初始化上级目录  ".." 根目录的上级目录还是根目录本身
    memcpy((void*)p_de->filename,"..",2);
    p_de->i_no = 0;                     //  "."是当前目录，因此目录项的inode是0
    p_de->f_type = FT_DIRECTORY;
    ide_write(hd,sb.data_start_lba,buf,1);   //sb.data_start_lba开始的一扇区以分配给根目录

    printk("    root_dir_lba:0x%x\n",sb.data_start_lba);
    printk("%s  format done\n",part->name);
    sys_free(buf);   
}

// 在硬盘上搜索文件系统，若没有则格式化分区创建文件系统
void filesys_init(){
    // 通道号、主从设备盘、分区索引
    uint8_t channel_no =0,dev_no,part_idx;

    // 超级块缓存
    struct super_block* sb_buf = (struct super_block*)sys_malloc(BLOCK_SIZE);
    if(sb_buf == NULL){
        PANIC("malloc memory failed!");
    }
    printk("searching filesystem......\n");
    while(channel_no<channel_cnt){
        dev_no = 0;
        while(dev_no<2){
            if(dev_no==0) {  //主盘,跨过我们的裸内核盘
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            // 处理主盘或从盘的各个分区
            struct partition* part = hd->prim_parts;//part即分区表的数组表项
            while(part_idx<12){
                if(part_idx==4){
                    part = hd->logic_parts;//我们的hd80M.img第4个主分区规划为扩展分区
                }
                if(part->sec_cnt!=0){       //分区存在
                    memset(sb_buf,0,BLOCK_SIZE);
                    // 读出硬盘分区中的超级块
                    ide_read(hd,part->start_lba+1,sb_buf,1);

                    // 内核只支持我们自定的文件系统
                    if(sb_buf->magic == 0x4c49){
                        printk("%s has filesystem\n",part->name);
                    }else{
                        printk("%s has no filesystem\n",part->name);
                        printk("formatting %s's partition %s......\n",hd->name,part->name);
                        partition_format(part);
                    }
                }
                part_idx++; //下一分区
                part++;     //数组下标++
            }
            dev_no++;       //下一盘
        }
        channel_no++;       //下一通道
    }
    sys_free(sb_buf);       //释放超级块缓存
}