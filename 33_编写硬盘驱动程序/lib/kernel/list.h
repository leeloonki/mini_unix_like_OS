#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"
#include "stdint.h"
// 定义链表节点 结点不需要数据域
struct list_elem{
    struct list_elem* prev;
    struct list_elem* next;
};

// 定义链表结构 实现内核数据结构队列
struct list{
    // head 队首
    struct list_elem head;
    // tail 队尾
    struct list_elem tail;  //队首队尾元素位置固定
};

#define offset(struct_type,member)  (int)(&((struct_type*)0)->member)    //获取结构体成员距离结构体偏移
#define elem2entry(struct_type,struct_member_name,elem_ptr) \
    (struct_type*)((int)elem_ptr - offset(struct_type,struct_member_name)) //将elem_ptr转换为struct_type类型指针


// list_traversal回调函数
typedef bool (function)(struct list_elem*,int arg); 

void list_init(struct list* list);
void list_inster_before(struct list_elem* before,struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* elem);
struct list_elem* list_pop(struct list* plist);
bool elem_find(struct list* plist,struct list_elem* elem);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist,function func,int arg);

#endif