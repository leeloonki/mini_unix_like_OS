#include "dir.h"
#include "inode.h"
#include "super_block.h"
#include "memory.h"
#include "global.h"             //bool类型定义
#include "stdio-kernel.h"
#include "string.h"
#include "debug.h"              //ASSERT宏定义
#include "file.h"
// 定义根目录,根目录
struct dir root_dir;

// 打开硬盘分区part上的根目录(加载根目录的inode到内存)
void open_root_dir(struct partition* part){
    root_dir.inode = inode_open(part,part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

// 打开硬盘分区part上i结点号为inode_no的目录,并返回目录指针
struct dir* dir_open(struct partition* part,uint32_t inode_no){
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part,inode_no);
    pdir->dir_pos =0;
    return pdir;
}

// 在分区part的pdir目录寻找名为name的文件或目录,找到后返回true并将该目录项存入dir_entry,否则返回false
bool search_dir_entry(struct partition*part,struct dir* pdir,const char* name,struct dir_entry* dir_entry){
    uint32_t block_cnt = 12+128;        //目录pdir的数据块(保存该目录下的目录项)
    // all_blocks_lba用于存放目录文件的所有140个lba地址
    uint32_t* all_blocks_lba = (uint32_t*)sys_malloc(block_cnt*4);//(目录)文件最大包括block_cnt个块,每个块lba地址4字节，
    if(all_blocks_lba==NULL){
        printk("search_dir_entry:sys_malloc for all_blocks_lba failed\n");
        return false;
    }
    uint32_t block_idx = 0;     //块下标  0<=block_idx<140

    while(block_cnt<12){        //将inode中的12直接块的lba地址先写入all_block_lba
        all_blocks_lba[block_idx] = pdir->inode->i_blocks[block_idx];
    }

    if(pdir->inode->i_blocks[12]!=0){   //如果pdir的inode中的间接块指针不为0(含间接块扇区(13-140项直接块指针)),则将间接块读入all_blocks_lba
        ide_read(part->my_disk,pdir->inode->i_blocks[12],all_blocks_lba+12,1);
    }
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);    //读入内存的硬盘块缓存
    struct dir_entry* p_de = (struct dir_entry*)buf;    //将该缓冲区转化为目录项数组
    uint32_t dir_entry_size = part->sb->dir_entry_size; //目录项大小位于分区超级块
    uint32_t dir_entry_cnt = BLOCK_SIZE/dir_entry_size; //每块可容纳的目录项
    block_idx =0;
    // 在所有块（140块）中查找目录项
    while(block_idx<block_cnt){
        if(all_blocks_lba[block_idx]==0){//i_blocks[block_idx] == 0即，该索引表项为空
            block_idx++;
            continue;
        }
        // 该索引表指向的块不为空，则读入该块,在该块下寻找是否存在名为name的目录或文件
        ide_read(part->my_disk,all_blocks_lba[block_idx],buf,1);
        // 遍历目录数组buf中的所有目录项
        uint32_t dir_entry_idx=0;
        while(dir_entry_idx<dir_entry_cnt){ //遍历该块(buf缓冲区即1块大小)下所有目录项
            if(!strcmp(p_de->filename,name)){//目录项的name相等待查找的name
                memcpy(dir_entry,p_de,dir_entry_size);//将该目录项存入dir_entry
                // 返回前释放缓冲区buf和all_blocks_lba 
                sys_free(buf);
                sys_free(all_blocks_lba);
                return true;
            }
            // 该目录项不匹配时
            dir_entry_idx++;   
            p_de++;
        }
        block_idx++;                        //遍历下一块
        memset(buf,0,SECTOR_SIZE);          //buf作为下一数据块缓存
        p_de = (struct dir_entry*)buf;      //p_de指向buf
    }
    // 140块均未找到
    sys_free(buf);
    sys_free(all_blocks_lba);
    return false;
}

// 关闭目录
void dir_close(struct dir* dir){
    if(dir==&root_dir){
        return;         //1.根目录不该关闭,后续当查找该分区上其他文件时，必须根据根目录进行路径解析
                        //2.根目录提前定义在编译内核时分配的空间而不是在堆区分配,因此struct dir root_dir内存空间无法通过sys_free释放
    }
    inode_close(dir->inode);
    sys_free(dir);
}   

// 在内存中初始化目录项p_de
// 形参：目录项名、inode号、文件类型、待初始化的内存中目录项结构体指针
void create_dfir_entry(char* filename,uint32_t inode_no,uint8_t file_types,struct dir_entry* p_de){
    ASSERT(strlen(filename)<=MAX_FILE_NAME_LEN);
    memcpy(p_de->filename,filename,strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_types;
}

// 将目录项p_de写入父目录parent_dir (io_buf主调函数提供的缓冲区)
bool sync_dir_entry(struct partition* part, struct dir* parent_dir,struct dir_entry* p_de,void* io_buf){
    struct inode* dir_inode = parent_dir->inode;
    struct partition* cur_part = part;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;     //目录项大小

    uint32_t dir_entrys_per_sec = (SECTOR_SIZE/dir_entry_size); //每扇区目录项个数

    uint32_t all_blocks_lba[140] = {0};
    uint32_t block_idx =0;
    while(block_idx<12){
        all_blocks_lba[block_idx] = dir_inode->i_blocks[block_idx];     //将inode中的12直接块的lba地址先写入all_block_lba
        block_idx++;
    }

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;                 //dir_e用于遍历io_buf所有目录项

    int32_t block_bitmap_idx = -1;          //申请的数据块在块位图的位
    block_idx=0;                            //文件的inode的数据块的块索引
    int32_t block_lba = -1;                 //通过block_bitmap_alloc分配的数据块的lba地址
    while(block_idx<140){                   //硬盘上每个文件最多12+128 = 140数据块
        if(all_blocks_lba[block_idx]==0){   //目录parent_dir的inode的索引表的block_idx项空,为此索引表项申请硬盘数据块,在此块写目录项p_de
            // 分配第一块数据块,该数据块可以作为直接或间接索引块、也可作为间接索引表
            block_lba = block_bitmap_alloc(cur_part);
            if(block_lba==-1){                  //硬盘数据块分配失败
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }

            // block_bitmap_alloc分配数据块成功后,同步块位图到硬盘的块位图
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;//分配的数据块在block_bitmap的位idx = 分配的数据块的lba地址减去数据块的起始lba地址
            ASSERT(block_bitmap_idx!=-1);
            bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

            if(block_idx<12){               //block_idx<12时block_lba直接作为待写入目录项的数据块
                dir_inode->i_blocks[block_idx] = all_blocks_lba[block_idx] = block_lba;//获取该直接块的lba地址
            }else if(block_idx == 12){      //如果是一级索引项(i_blocks[12]),则先为该索引项分配一硬盘数据块作为索引表,再分配一块数据块做索引表的第0项(文件的第12+1数据块)
                dir_inode->i_blocks[12] = block_lba;//如果block_idx = 12时，即到了一级索引表表项,那么分配的第一块数据块 block_lba作为间接索引表块
                // 即i_blocks[12]指向间接索引表,我们需要再分配一数据块,在该数据块写目录项,并将该数据块的lba地址填入到i_blocks[12]指向的索引表的0表项
                block_lba =-1;
                block_lba = block_bitmap_alloc(cur_part);   //分配第二块数据块
                if(block_lba == -1){        //如果该数据块分配失败,那么目录项将无法写入,因此我们需要回滚，将上面申请的数据块i_blocks[12](间接索引表所在块)释放,即其对应数据块位图置0
                    block_bitmap_idx = dir_inode->i_blocks[12] - cur_part->sb->data_start_lba;//block_bitmap_idx即 该简介索引表块在数据块位图的位索引
                    bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);//释放简介索引表块
                    dir_inode->i_blocks[12] = 0;//简介索引表项置0
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }

                // 如果第二块数据块(真正存放目录项的数据块)分配成功
                // 再次同步硬盘分区的数据块位图
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;//分配的数据块在block_bitmap的位idx = 分配的数据块的lba地址减去数据块的起始lba地址
                ASSERT(block_bitmap_idx!=-1);
                bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
                all_blocks_lba[12] = block_lba;                             // all_blocks_lba存放一个文件140项的lba地址,这里间接块(第13块，也即第一间接块)分配成功则将设置第12项
                ide_write(cur_part->my_disk,dir_inode->i_blocks[12],all_blocks_lba + 12,1); //all_blocks_lba + 12 即all_blocks_lba[12] = block_lba,将内存中i_blocks[12]同步到硬盘的i_blocks[12](同步间接索引表LBA地址)
            }else{                                                          // 如果block_idx>12时，则将分配的第一块数据块同步到硬盘上的间接索引表,
                // all_blocks_lba + 12 即偏移all_blocks_lba共12*4字节,之后的(12-140)*4 共512B一扇区，对应13-140项间接索引块LBA
                all_blocks_lba[block_idx] = block_lba;
                ide_write(cur_part->my_disk,dir_inode->i_blocks[12],all_blocks_lba + 12,1);//同步内存中索引表项到硬盘的inode索引表
            }
            memset(io_buf,0,512);
            memcpy(io_buf,p_de,dir_entry_size);
            ide_write(cur_part->my_disk,dir_inode->i_blocks[block_idx],io_buf,1);//写入目录项到硬盘
            dir_inode->i_size+=dir_entry_size;                                   //写入目录项后更新目录大小
            return true;
        }
        // 如果inode的数据块索引表的block_idx项已存在,读入该块,在该存在的目录数据中查找是否有空余空间供写p_de目录项
        ide_read(cur_part->my_disk,all_blocks_lba[block_idx],io_buf,1);         //从硬盘读入目录数据块到io_buf
        uint8_t dir_entry_idx =0;
        while(dir_entry_idx<dir_entrys_per_sec){
            if((dir_e + dir_entry_idx)->f_type==FT_UNKNOWN){                    //如果扇区内某目录项的f_type==0,说明该项为空,可以写入目录项p_de
                memcpy(dir_e+dir_entry_idx,p_de,dir_entry_size);                //将p_de拷贝到io_buf指定位置
                ide_write(cur_part->my_disk,all_blocks_lba[block_idx],io_buf,1);    //写回io_buf到硬盘
                dir_inode->i_size +=dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++; 
    }
    printk("directory is full!\n");
    return false;
}