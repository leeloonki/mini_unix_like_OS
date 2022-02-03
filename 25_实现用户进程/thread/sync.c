#include "sync.h"
#include "global.h" //定义了NULL
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
// 初始化信号量
void sema_init(struct semaphore* psema,uint8_t value){
    psema->value = value;
    list_init(&psema->waiters); //阻塞队列初始为空
}

void lock_init(struct lock* plock){
    plock->holder = NULL;
    plock->holder_repeat_nr =0;
    sema_init(&plock->semaphore,1); //信号量初始 1
}

// 信号量P操作
void sema_down(struct semaphore* psema){
    // 关中断，保证原子操作
    enum intr_status old_status = intr_disable();
    while(psema->value==0){ //锁被持有
        ASSERT(!elem_find(&psema->waiters,&running_thread()->general_tag));
        if(elem_find(&psema->waiters,&running_thread()->general_tag)){
            PANIC("sema_down: thread blocked has been in waiters_list\n"); 
        }
        // 将当前线程加到阻塞队列
        list_append(&psema->waiters,&running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }

    // 线程被唤醒后 或value=1时 继续执行以下代码
    psema->value--;
    ASSERT(psema->value==0);
    intr_set_status(old_status);
}

// 信号量V操作
void sema_up(struct semaphore* psema){
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value==0);
    // 如果阻塞队列不为空，则唤醒一个阻塞进程
    if(!list_empty(&psema->waiters)){
        struct task_struct* thread_blocked = elem2entry(struct task_struct,general_tag,list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    // 线程被唤醒后 或value=0时 继续执行以下代码
    psema->value++;
    ASSERT(psema->value==1);
    intr_set_status(old_status);
}

// 获取锁
void lock_acquire(struct lock* plock){
    // if else 排除自己已经持有锁但未将其释放情况
    if(plock->holder != running_thread()){
        sema_down(&plock->semaphore);   //对信号量P操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr==0);
        plock->holder_repeat_nr=1;
    }else{
        plock->holder_repeat_nr++;
    }
}

// 释放锁
void lock_release(struct lock* plock){
    ASSERT(plock->holder == running_thread());
    if(plock->holder_repeat_nr > 1){
        plock->holder_repeat_nr --;
        return;
    }
    ASSERT(plock->holder_repeat_nr ==1);
    plock->holder = NULL;
    plock->holder_repeat_nr =0;
    sema_up(&plock->semaphore);
}          