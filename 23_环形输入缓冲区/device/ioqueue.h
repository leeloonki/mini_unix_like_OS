#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "sync.h"
#include "thread.h"
#include "stdint.h"
#include "global.h"
#define bufsize 64                  //定义缓冲区大小 最大存储bufsize-1   环形队列实现缓冲区时为了区分队列是满还是空，需占用一个存储空间 

// 环形队列
struct ioqueue{
    struct lock lock;               //该缓冲区对应的锁
    struct task_struct* producer;   //阻塞在该缓冲区上的生产者
    struct task_struct* consumer;   //阻塞在该缓冲区上的消费者
    char buf[bufsize];              //缓冲区内存
    int32_t head;                   //队头指针
    int32_t tail;                   //队尾指针
    int16_t nowlength               //缓冲区已输入字符数
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq,char byte);
#endif