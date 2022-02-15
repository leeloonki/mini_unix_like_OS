#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "stdint.h"
#include "list.h"
#include "thread.h"
// 信号量结构
struct semaphore{
    uint8_t value;          //信号量的初值
    struct list waiters;    //该信号量上的阻塞线程队列
};

// 锁结构
struct lock{
    struct task_struct* holder;     //持有该锁的线程指针
    struct semaphore semaphore;     //二元信号量实现锁
    uint32_t holder_repeat_nr;      //锁的持有者重复申请锁的 次数
};
void sema_init(struct semaphore* psema,uint8_t value);
void lock_init(struct lock* plock);
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);
#endif