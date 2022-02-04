#include "ioqueue.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
// 缓冲区初始化
void ioqueue_init(struct ioqueue* ioq){
    lock_init(&ioq->lock);      //初始化队列的锁
    ioq->producer = ioq->consumer = NULL;
    ioq->head = ioq->tail = 0;
}

// 返回缓冲区内存当前位置pos的下一位置
static int32_t next_pos(int32_t pos){
    return (pos+1)%bufsize;
}

// 判断当前队列是否已满
bool ioq_full(struct ioqueue* ioq){
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) ==ioq->tail;
}

// 判断队列是否已空
static bool ioq_empty(struct ioqueue* ioq){
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head ==ioq->tail;
}

// 让当前生产者或消费者在此缓冲区阻塞
static void ioq_wait(struct task_struct** waiter){
    ASSERT(*waiter == NULL && waiter !=NULL); //此时缓冲区无阻塞线程，传入了阻塞线程
    *waiter = running_thread(); //设置缓冲区waiter进程为当前线程
    thread_block(TASK_BLOCKED); //阻塞当前进程
}

// 唤醒在此缓冲区阻塞生产者或消费者
static void ioq_wakeup(struct task_struct** waiter){
    ASSERT(*waiter!=NULL);      //
    thread_unblock(*waiter);
    *waiter = NULL;
}

// 消费者在此缓冲区获取一个字符
char ioq_getchar(struct ioqueue* ioq){
    ASSERT(intr_get_status() == INTR_OFF);
    while(ioq_empty(ioq)){
        lock_acquire(&ioq->lock);       //获取锁
        ioq_wait(&ioq->consumer);       //若缓冲区空，则消费者阻塞
        lock_release(&ioq->lock);
    }

    // 若缓冲区不空
    char byte = ioq->buf[ioq->tail];    //从缓冲区取出一字符
    ioq->tail = next_pos(ioq->tail);

    if(ioq->producer!=NULL){
        ioq_wakeup(&ioq->producer);
    }
    return byte;
}

// 生产者往缓冲区写一个字符
void ioq_putchar(struct ioqueue* ioq,char byte){
    ASSERT(intr_get_status() == INTR_OFF);
    while(ioq_full(ioq)){
        lock_acquire(&ioq->lock);       //获取锁
        ioq_wait(&ioq->producer);       //若缓冲区空，则消费者阻塞
        lock_release(&ioq->lock);
    }

    ioq->buf[ioq->head] = byte;
    ioq->head = next_pos(ioq->head);

    if(ioq->consumer!=NULL){
        ioq_wakeup(&ioq->consumer);
    }
}
