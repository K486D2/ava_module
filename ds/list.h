#ifndef LIST_H
#define LIST_H

#include "util/marcodef.h"
#include "util/typedef.h"

#define LIST_POISON1 NULL
#define LIST_POISON2 NULL

typedef struct list_head {
        struct list_head *prev;
        struct list_head *next;
} list_head_t;

HAPI void
list_init(list_head_t *list)
{
        list->next = list;
        list->prev = list;
}

HAPI void
__list_add(list_head_t *new_node, list_head_t *prev, list_head_t *next)
{
        next->prev     = new_node;
        new_node->next = next;
        new_node->prev = prev;
        prev->next     = new_node;
}

HAPI void
list_add(list_head_t *new_node, list_head_t *head)
{
        __list_add(new_node, head, head->next);
}

HAPI void
list_add_tail(list_head_t *new_node, list_head_t *head)
{
        __list_add(new_node, head->prev, head);
}

HAPI void
__list_del(list_head_t *prev, list_head_t *next)
{
        prev->next = next;
        next->prev = prev;
}

HAPI void
list_del(list_head_t *entry)
{
        __list_del(entry->prev, entry->next);
        entry->next = LIST_POISON1;
        entry->prev = LIST_POISON2;
}

HAPI void
__list_splice(list_head_t *node, list_head_t *head)
{
        list_head_t *first = node->next;
        list_head_t *last  = node->prev;
        list_head_t *at    = head->next;

        first->prev = head;
        head->next  = first;

        last->next = at;
        at->prev   = last;
}

HAPI bool
list_empty(const list_head_t *list)
{
        return list->next == list;
}

HAPI void
list_splice(list_head_t *node, list_head_t *head)
{
        if (!list_empty(node))
                __list_splice(node, head);
}

HAPI void
list_replace(list_head_t *old_node, list_head_t *new_node)
{
        new_node->next       = old_node->next;
        new_node->next->prev = new_node;
        new_node->prev       = old_node->prev;
        new_node->prev->next = new_node;
}

HAPI void
list_replace_init(list_head_t *old_node, list_head_t *new_node)
{
        list_replace(old_node, new_node);
        list_init(old_node);
}

HAPI void
list_move(list_head_t *node, list_head_t *head)
{
        __list_del(node->prev, node->next);
        list_add(node, head);
}

HAPI void
list_move_tail(list_head_t *node, list_head_t *head)
{
        __list_del(node->prev, node->next);
        list_add_tail(node, head);
}

#define LIST_FIRST_ENTRY(ptr, type, member) CONTAINER_OF((ptr)->next, type, member)

#define LIST_FOR_EACH(pos, head)            for (pos = (head)->next; pos != (head); pos = pos->next)

#define LIST_FOR_EACH_ENTRY(pos, head, member)                                               \
        for (pos = CONTAINER_OF((head)->next, typeof(*pos), member); &pos->member != (head); \
             pos = CONTAINER_OF(pos->member.next, typeof(*pos), member))

#define LIST_FOR_EACH_PREV(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)

#endif // !LIST_H
