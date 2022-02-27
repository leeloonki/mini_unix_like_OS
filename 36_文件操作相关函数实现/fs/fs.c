#include "fs.h"
#include "dir.h"
#include "inode.h"
#include "super_block.h"
#include "ide.h"
#include "global.h"     //向上取整宏定义   NULL定义
#include "stdio-kernel.h"
#include "memory.h"
#include "string.h"
#include "debug.h"
#include "list.h"

struct partition* cur_part;//操作分区


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



// 在分区链表(ide_init初始化时读取硬盘分区表并在在内存更新struct list partition_list)中找到名为arg的硬盘分区挂载之(再次读取分区获取元信息)
// 形参pelem 分区指针；arg分区名
static bool mount_partition(struct list_elem* pelem,int arg){
    char* part_name = (char*)arg;
    // part为partition_list某链表项指针
    struct partition* part = elem2entry(struct partition,part_tag,pelem);//根据pelem地址和结构体struct partition成员part_tag距结构体本身偏移地址得出结构体地址
    if(!strcmp(part->name,part_name)){

        // 1. 读取硬盘超级块到内存
        cur_part = part;                        //cur_part为partition_list某链表项指针
        struct disk* hd  = cur_part->my_disk;   //分区所属硬盘，做读写硬盘的参数使用
        struct super_block* sb_buf = (struct super_block*)sys_malloc(sizeof(struct super_block));//缓冲区，缓存硬盘读入的超级块
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if(cur_part->sb == NULL){
            PANIC("malloc memory failed!");
        }
        memset(sb_buf,0,BLOCK_SIZE);
        // 将硬盘分区超级块读入内存sb_buf缓冲区
        ide_read(hd,cur_part->start_lba+1,sb_buf,1);//一扇区 = 1block，超级块位于分区lba+1所在块(扇区)

        // 2. 读取硬盘块位图

        // 内存中 块位图起始地址
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_block * BLOCK_SIZE);//块位图每位代表1块
        if(cur_part->block_bitmap.bits == NULL){
            PANIC("malloc memory failed!");
        }
        ide_read(hd,sb_buf->block_bitmap_lba,cur_part->block_bitmap.bits,sb_buf->block_bitmap_block);
        // 块位图字节数
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_block * BLOCK_SIZE;

        // 3. 读取硬盘inode位图

        // 内存inode位图起始地址
        cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_block * BLOCK_SIZE);
        if(cur_part->inode_bitmap.bits == NULL){
            PANIC("malloc memory failed!");
        }
        ide_read(hd,sb_buf->inode_bitmap_lba,cur_part->inode_bitmap.bits,sb_buf->inode_bitmap_block);
        // inode位图字节数
        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_block*BLOCK_SIZE;
        
        list_init(&cur_part->open_inodes);//挂载分区时，分区打开的文件(inode)为空
        printk("mount %s done!\n",part->name);
        return true;//停止list_travesal遍历
    }
    return false;//分区名不匹配则继续遍历下一分区，需返回false
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

    // 挂载分区
    // ide_init时，struct list partition_list变量已将扫描的所有分区链接，我们对该链表中所有分区进行遍历，挂在所有分区

    // 遍历的第一个分区为分区链表partition_list第一个元素
    
    char default_part[16] = "sdb1";
    //默认挂载的分区，我们默认挂载"sdb1"，即./hd80M.img1
    list_traversal(&partition_list,mount_partition,(int)default_part);//mount_partition()做回调函数,返回值必须为bool类型
}

// 路径解析(每次解析上层路径名)
// pathname：初始时为待解析文件绝对路径名如/a/b/c,name_store：解析获得的上层路径名如a或b或c
static char* path_parse(char* pathname, char* name_store){
    if(pathname[0]=='/'){//根目录
        // glibc 库支持的系统中 路径中多个连续的'/'等同于一个/,如//a///b/c = /a/b/c,我们内核也按此处理
        while(*(++pathname)=='/');
        // 开始解析下级目录
        while(*pathname!='/'&&*pathname!=0){
            *name_store++ = *pathname++;
        }
        if(pathname[0]==0){
            return NULL;
        }
        
    }
    return pathname;//pathname指向去除上层目录后的路径名 如/b/c
}

// 返回路径深度  /a/b/c  深度=3
int32_t path_depth_cnt(char* pathname){
    ASSERT(pathname!=NULL);
    char* p = pathname;
    uint32_t depth = 0;
    char name[MAX_FILE_NAME_LEN];       //存放上层目录或文件
    p = path_parse(p,name);             //去除上层目录后的路径名
    while(name[0]){                     //目录或文件字符串不为空
        depth++;
        memset(name,0,MAX_FILE_NAME_LEN);//清空,存放下次经path_parse调用获取的上层目录字符串
        if(p){
            p = path_parse(p,name); 
        }
    }
    return depth;
}

// 搜索文件pathname,若找到返回其inode 否则返回-1
static int search_file(const char* pathname,struct path_search_record* searched_record){
    // 对于根路径
    if(!strcmp(pathname,"/")||!strcmp(pathname,"/.")||!strcmp(pathname,"/..")){
        //如果路径名为 / /. /..均为根目录
        searched_record->file_type = FT_DIRECTORY;
        searched_record->parent_dir = &root_dir;
        searched_record->searched_path[0] =0;//搜索路径置空
        return 0;
    }

    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0]=='/'&&path_len>1 &&path_len<MAX_PATH_LEN);

    struct dir* parent_dir = &root_dir;
    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;    //初始化为known即0
    
    // 解析子路径
    char* sub_path = (char*)pathname;
    char name[MAX_FILE_NAME_LEN];
    sub_path = path_parse(sub_path,name);
    uint32_t parent_inode_no =0;                //父目录的inode号
    struct dir_entry dir_e;
    while(name[0]){
        // 记录已存在的父目录
        strcat(searched_record->searched_path,"/");
        strcat(searched_record->searched_path,name);
        // 例：/a/b/c
        // 在目录/下查找名为name文件或目录
        if(search_dir_entry(cur_part,parent_dir,name,&dir_e)){//第一轮：在 "/"下找名为a的文件或目录,将a的目录项存入dir_e
            memset(name,0,MAX_FILE_NAME_LEN);
            if(sub_path){                                   //如果子路径存在
                sub_path = path_parse(sub_path,name);
            }
            if(dir_e.f_type == FT_DIRECTORY){               //被打开的是目录
                parent_inode_no = parent_dir->inode->i_no;
                // 更新父目录
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part,dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;//继续遍历下层目录
            }else if(dir_e.f_type == FT_REGULAR){           //当前name对应的为文件非目录
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        }else{
            // 找不到目录下的目录项
            return -1;
        }
    }
    // 只存在查找的文件的同名目录   
    dir_close(searched_record->parent_dir);                //遍历到此，路径pathname已被完整解析,且pathname的最后一层路径为目录（如果是文件则返回文件的inode）
    // 更新被查找到的目录的父目录
    searched_record->parent_dir = dir_open(cur_part,parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}


// 例：查找文件c,在pathname=/a/b/c/查找
// 第一轮：
//     parent_dir = /,name = a,sub_path = b/c/,dir_e = a->inode
//     parent_dir = a,name = b,sub_path = c/

// 第二轮：
//     parent_dir = a,name = b,sub_path = 0,dir_e = b->inode
//     parent_dir = b,name = c,sub_path = 0
    
// 第三轮：
//     parent_dir = b,name = c,dir_e = c->inode
//     parent_dir = c,name = 0,sub_path = 0