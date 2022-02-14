#include "list.h"
#include "interrupt.h"
// 初始化双向链表
void list_init(struct list* list){
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

// 将链表元素elem插入到结点元素before前面
void list_inster_before(struct list_elem* before,struct list_elem* elem){
    // 对链表的操作在内核进行，为防止中断对链表数据结构的破坏，在因插入时需要关闭中断
    enum intr_status old_status = intr_disable();
    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;
    intr_set_status(old_status);
}

// 将元素添加到队首(head后)
void list_push(struct list* plist, struct list_elem* elem){
    list_inster_before(plist->head.next,elem);
}

// 将元素添加到队尾(tail前)
void list_append(struct list* plist, struct list_elem* elem){
    list_inster_before(&plist->tail,elem);
}

// 使元素脱离链表
void list_remove(struct list_elem* elem){
    enum intr_status old_status = intr_disable();
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    intr_set_status(old_status);
}

// 弹出链表第一个元素 类似pop
struct list_elem* list_pop(struct list* plist){
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;    
}

// 从链表中查找元素
bool elem_find(struct list* plist,struct list_elem* elem){
    struct list_elem* pelem = plist->head.next;
    while (pelem!=&plist->tail) //链表节点list_elem没有数据域，这里比较结点地址
    {
        if(pelem==elem){
            return true;
        }
        pelem = pelem->next;
    }
    return false;
}

// 判断链表是否为空
bool list_empty(struct list* plist){
    return (plist->head.next==&plist->tail ? true : false);
}

// 返回链表长度
uint32_t list_len(struct list* plist){
    struct list_elem* pelem = plist->head.next;
    uint32_t length =0;
    while(pelem != &plist->tail){
        length ++;
        pelem = pelem->next;
    }
    return length;
}

// 遍历链表中所有元素，判断是否有符合条件的元素
// 回调函数func进行判断，符合条件返回元素指针
struct list_elem* list_traversal(struct list* plist,function func,int arg){
    struct list_elem* pelem = plist->head.next;
    if(list_empty(plist)){
        return NULL;
    }
    while(pelem!=&plist->tail){
        if(func(pelem,arg)){
            return pelem;
        }
        pelem = pelem->next;
    }
    return NULL;
}
