#include "bitmap.h"
#include "string.h"
#include "debug.h"
// 位图初始化(根据btmp中位图起始地址和位图字节数将这片区域内存置为0)
void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits,0,btmp->btmp_bytes_len);
}

// 判断位图中bit_idx位是否为1
bool bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx){
    uint32_t byte_idx = bit_idx / 8;                        // 将位索引转化为字节索引
    uint8_t bit_odd = bit_idx %8;                          // 字节内位置
    return (btmp->bits[byte_idx] & (BITMAP_MASK <<bit_odd));
}

// 在位图中连续申请cnt个位，成功：返回起始位的下标，失败：返回-1
int bitmap_scan(struct bitmap* btmp,uint32_t cnt){
    uint32_t idx_byte = 0;          //记录空闲位所在字节

    while ((0xff==btmp->bits[idx_byte]) &&(idx_byte<btmp->btmp_bytes_len)){
        idx_byte++;
    }
    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if(idx_byte==btmp->btmp_bytes_len)                      //内存池无可用空间
    {
        return -1;
    }

    int idx_bit = 0;
    while((uint8_t)(BITMAP_MASK<<idx_bit)&btmp->bits[idx_byte]){
        idx_bit++;          //找到第一个可用0位
    }

    int bit_idx_start = idx_byte * 8 + idx_bit;
    if(cnt == 1){return bit_idx_start;}
    
    uint32_t bit_left = (btmp->btmp_bytes_len *8 - bit_idx_start);
    uint32_t next_bit = bit_idx_start +1;
    uint32_t count = 1;

    bit_idx_start = -1;
    while(bit_left-- >0){
        if(!bitmap_scan_test(btmp,next_bit)){
            count++;
        }else{
            count=0;
        }
        if(count==cnt){
            bit_idx_start = next_bit -cnt +1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

// 将位图中bit_idx位置为value(0 || 1)
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value){
    ASSERT((value ==0 || value==1));
    uint32_t byte_idx = bit_idx / 8;
    uint8_t bit_odd = bit_idx % 8;
    if(value){
        btmp->bits[bit_idx] |= (BITMAP_MASK<<bit_odd);  //或1
    }else{
        btmp->bits[bit_idx] &= (BITMAP_MASK<<bit_odd);  //与0
    }
}