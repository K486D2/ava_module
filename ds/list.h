#ifndef LIST_H
#define LIST_H

#include "../util/macrodef.h"
#include "../util/typedef.h"

/* 双向循环链表节点 */
typedef struct list_head {
        struct list_head *prev;
        struct list_head *next;
} list_head_t;

/* 获取链表首节点所在的宿主结构体指针 */
#define LIST_FIRST_ENTRY(entry, type, member) CONTAINER_OF((entry)->next, type, member)

/* 正向遍历链表节点指针 */
#define LIST_FOR_EACH(entry, head)            for (entry = (head)->next; entry != (head); entry = entry->next)

/* 正向遍历链表，并将节点转换为宿主结构体 */
#define LIST_FOR_EACH_ENTRY(entry, head, member)                                                   \
        for (entry = CONTAINER_OF((head)->next, typeof(*entry), member); &entry->member != (head); \
             entry = CONTAINER_OF(entry->member.next, typeof(*entry), member))

/* 反向遍历链表节点指针 */
#define LIST_FOR_EACH_PREV(entry, head) for (entry = (head)->prev; entry != (head); entry = entry->prev)

/* 反向遍历链表，并将节点转换为宿主结构体 */
#define LIST_FOR_EACH_ENTRY_REVERSE(entry, head, member)                                           \
        for (entry = CONTAINER_OF((head)->prev, typeof(*entry), member); &entry->member != (head); \
             entry = CONTAINER_OF(entry->member.prev, typeof(*entry), member))

/* -------------------------------------------------------------------------- */
/*                                   声明                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief 初始化链表头节点，使其自指
 *
 * @param head
 */
HAPI void list_init(list_head_t *head);

/**
 * @brief 在 prev_node 与 next_node 之间插入 entry
 *
 * @param entry
 * @param prev_node
 * @param next_node
 */
HAPI void __list_add(list_head_t *entry, list_head_t *prev_node, list_head_t *next_node);

/**
 * @brief 将 entry 插入到 head 之后 (头插)
 *
 * @param entry
 * @param head
 */
HAPI void list_add(list_head_t *entry, list_head_t *head);

/**
 * @brief 将 entry 插入到 head 之后 (尾插)
 *
 * @param entry
 * @param head
 */
HAPI void list_add_tail(list_head_t *entry, list_head_t *head);

/**
 * @brief 删除 prev_node 与 next_node 间的节点连接
 *
 * @param prev_node
 * @param next_node
 */
HAPI void __list_del(list_head_t *prev_node, list_head_t *next_node);

/**
 * @brief 将 entry 从链表中删除, 并将指针置空便于调试
 *
 * @param entry
 */
HAPI void list_del(list_head_t *entry);

/**
 * @brief 判断链表是否为空
 *
 * @param head
 */
HAPI bool list_empty(const list_head_t *head);

/**
 * @brief 将 list 整体拼接到 head 之后
 *
 * @param list
 * @param head
 */
HAPI void __list_splice(const list_head_t *list, list_head_t *head);

/**
 * @brief 若 list 非空，则拼接到 head
 *
 * @param list
 * @param head
 */
HAPI void list_splice(list_head_t *list, list_head_t *head);

/**
 * @brief 用 new_entry 替换 old_entry
 *
 * @param old_entry
 * @param new_entry
 */
HAPI void list_replace(const list_head_t *old_entry, list_head_t *new_entry);

/**
 * @brief 替换后重置 old_entry 为独立链表
 *
 * @param old_entry
 * @param new_entry
 */
HAPI void list_replace_init(list_head_t *old_entry, list_head_t *new_entry);

/**
 * @brief 将 entry 移动到 head 之后
 *
 * @param entry
 * @param head
 */
HAPI void list_move(list_head_t *entry, list_head_t *head);

/**
 * @brief 将 entry 移动到 head 之前
 *
 * @param entry
 * @param head
 */
HAPI void list_move_tail(list_head_t *entry, list_head_t *head);

/* -------------------------------------------------------------------------- */
/*                                   定义                                     */
/* -------------------------------------------------------------------------- */

HAPI void
list_init(list_head_t *head)
{
        head->next = head;
        head->prev = head;
}

HAPI void
__list_add(list_head_t *entry, list_head_t *prev_node, list_head_t *next_node)
{
        next_node->prev = entry;
        entry->next     = next_node;
        entry->prev     = prev_node;
        prev_node->next = entry;
}

HAPI void
list_add(list_head_t *entry, list_head_t *head)
{
        __list_add(entry, head, head->next);
}

HAPI void
list_add_tail(list_head_t *entry, list_head_t *head)
{
        __list_add(entry, head->prev, head);
}

HAPI void
__list_del(list_head_t *prev_node, list_head_t *next_node)
{
        prev_node->next = next_node;
        next_node->prev = prev_node;
}

HAPI void
list_del(list_head_t *entry)
{
        __list_del(entry->prev, entry->next);
        entry->next = NULL;
        entry->prev = NULL;
}

HAPI bool
list_empty(const list_head_t *head)
{
        return head->next == head;
}

HAPI void
__list_splice(const list_head_t *list, list_head_t *head)
{
        list_head_t *first = list->next;
        list_head_t *last  = list->prev;
        list_head_t *at    = head->next;

        first->prev = head;
        head->next  = first;

        last->next = at;
        at->prev   = last;
}

HAPI void
list_splice(list_head_t *list, list_head_t *head)
{
        if (!list_empty(list))
                __list_splice(list, head);
}

HAPI void
list_replace(const list_head_t *old_entry, list_head_t *new_entry)
{
        new_entry->next       = old_entry->next;
        new_entry->next->prev = new_entry;
        new_entry->prev       = old_entry->prev;
        new_entry->prev->next = new_entry;
}

HAPI void
list_replace_init(list_head_t *old_entry, list_head_t *new_entry)
{
        list_replace(old_entry, new_entry);
        list_init(old_entry);
}

HAPI void
list_move(list_head_t *entry, list_head_t *head)
{
        __list_del(entry->prev, entry->next);
        list_add(entry, head);
}

HAPI void
list_move_tail(list_head_t *entry, list_head_t *head)
{
        __list_del(entry->prev, entry->next);
        list_add_tail(entry, head);
}

#endif // !LIST_H
