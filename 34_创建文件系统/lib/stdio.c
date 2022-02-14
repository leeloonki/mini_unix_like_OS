#include "stdio.h"
#include "global.h"
#include "stdint.h"
#include "string.h"
#include "syscall.h"


#define va_start(ap,last) ap=(va_list)&last     //format为char*类型，是栈中指向一个字符串的指针变量，获取format参数地址，将其转化为为va_list
#define va_arg(ap,type) *((type*)(ap += 4))     //ap指向栈中下一个参数，并返回栈中该参数的值
#define va_end(ap) ap=NULL


// printf("The raw value: i=%d\n", i)

// 将整型转换为字符型
// int -> asci
// 参数1：待转换整型，参数2：转换后保存ASCII的缓冲区地址的地址，参数3：进制
static void itoa(uint32_t value,char** buf_ptr_addr,uint8_t base){
    uint32_t m = value % base;  //求模
    uint32_t i = value / base;  //取整  30%8=6 30/8=3 3%8=3 3/8=0  36
    if(i){ //如果取整后不为0
        itoa(i,buf_ptr_addr,base);
    }
    if(m<10){ //如果本次余数为0-9，最先执行本语句的为递归调用的最后一轮，得到的为数字value的最高位
        *((*buf_ptr_addr)++) = m + '0';     //将数转化为字符，即buf_ptr_addr开始从高位写入
    }else{ //余数A-F
        *((*buf_ptr_addr)++) = m - 10 + 'A'; //-10后相对字符A的值
    }  
}

// 将参数指针ap指向的参数按照格式format输出到字符串str,并返回str长度
uint32_t vsprintf(char *str, const char *format, va_list ap){
    char* buf_ptr = str;                //buf_str指向存放格式化后字符串的缓冲区
    const char* index_ptr = format;     
    char index_char = *index_ptr;
    int32_t arg_int;
    char* arg_str;                      //字符串指针                    
    while(index_char){
        if(index_char!='%'){
            *(buf_ptr++) = index_char;      //将format字符串中非%类型字符的字符拷贝到str
            index_char = *(++index_ptr);
            continue;
        }
        // 如果本次为'%'
        index_char = *(++index_ptr);        //获取%后的类型字符如%d %f %x 的d f x
        switch(index_char){
            case 's':
                arg_str = va_arg(ap,char*); //%s对应的栈中下一个参数为char*类型指针，指向一个字符串;va_arg返回的是栈中该变量的值，即字符串的地址，同时ap指向了栈中该变量的地址
                strcpy(buf_ptr,arg_str);    //将字符串拷贝到buf_ptr指向的缓冲区
                buf_ptr += strlen(arg_str); //更新buf_ptr字符指针位置
                index_char = *(++index_ptr);//跨过类型字符 s
                break;
            case 'c':
                *(buf_ptr++) = va_arg(ap,char); //获取到栈中char类型变量的值，填充到buf_ptr位置
                index_char = *(++index_ptr);    //跨过类型字符 c
                break;
            case 'd':
                arg_int = va_arg(ap,int);       //获取栈中int类型变量值
                if(arg_int < 0){                //如果是负数
                    arg_int = 0-arg_int;        //如果是负数，0-之转化为正数，并在buf_ptr中转换该数字时前加 负号。
                    *buf_ptr++ = '-';
                }
                itoa(arg_int,&buf_ptr,10);      //10进制
                index_char = *(++index_ptr);    //跨过类型字符 d
                break;   
            case 'x':                           //x十六进制
                arg_int = va_arg(ap,int);       //va_arg使用前必须调用va_start初始化ap指向第一个参数format的地址
                itoa(arg_int,&buf_ptr,16);      //调用完成后buf_ptr的"值"会++,即指向缓冲区的下一个地址
                index_char = *(++index_ptr);    //跨过类型字符 x
                break;   
        }
    }
    return strlen(str);
}

// 格式化输出字符串到console
uint32_t printf(const char* format,...){
    va_list args;               //args 即ap
    va_start(args,format);      //args指向format
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    return write(buf);
}

// 格式化输出字符串到缓冲区buf
uint32_t sprintf(char* buf,const char* format,...){
    va_list args;               //args 即ap
    uint32_t ret;               //返回值
    va_start(args,format);      //args指向format
    ret = vsprintf(buf,format,args);
    va_end(args);
    return ret;
}

