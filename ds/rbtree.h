/**
 * @file rbtree.h
 * @brief Linux 风格红黑树实现
 *
 * 红黑树是一种自平衡二叉搜索树，满足:
 *   1. 每个节点是红色或黑色;
 *   2. 根节点是黑色;
 *   3. 所有叶子节点 (NULL) 视为黑色;
 *   4. 红色节点的子节点必须是黑色;
 *   5. 任意节点到其所有叶子的路径中, 黑色节点数相等.
 *
 * 本实现采用 Linux 内核风格:
 *   - 用最低 2 bit 存颜色 (节省结构体空间);
 *   - 提供插入/删除平衡逻辑;
 *   - 用户负责维护键值比较;
 */

#ifndef RBTREE_H
#define RBTREE_H

#include "../util/macrodef.h"
#include "../util/typedef.h"

typedef enum {
        RB_RED   = 0,
        RB_BLACK = 1,
} rb_color_e;

/* 红黑树节点结构 */
typedef struct rb_node {
        usz             rb_parent_color; // 低 2 位存颜色, 高位存父节点指针
        struct rb_node *rb_right;        // 右子节点
        struct rb_node *rb_left;         // 左子节点
} rb_node_t;

/* 红黑树根节点 */
typedef struct {
        rb_node_t *rb_node;
} rb_root_t;

/* 空树定义 */
#define RB_ROOT      \
        (rb_root_t)  \
        {            \
                NULL \
        }

HAPI rb_node_t *rb_get_parent(const rb_node_t *rb);
HAPI void       rb_set_parent(rb_node_t *rb, rb_node_t *parent);

HAPI rb_color_e rb_get_color(const rb_node_t *rb);
HAPI void       rb_set_color(rb_node_t *rb, rb_color_e color);

HAPI bool rb_is_red(const rb_node_t *rb);
HAPI bool rb_is_black(const rb_node_t *rb);

HAPI void rb_set_red(rb_node_t *rb);
HAPI void rb_set_black(rb_node_t *rb);

HAPI bool rb_is_unlinked(const rb_node_t *rb);
HAPI void rb_link_node(rb_node_t *node, rb_node_t *parent, rb_node_t **rb_link);

HAPI void rb_rotate_left(rb_node_t *node, rb_root_t *root);
HAPI void rb_rotate_right(rb_node_t *node, rb_root_t *root);

HAPI void rb_insert_color(rb_node_t *node, rb_root_t *root);
HAPI void rb_erase_color(rb_node_t *node, rb_node_t *parent, rb_root_t *root);
HAPI void rb_erase(rb_node_t *node, rb_root_t *root);

HAPI rb_node_t *rb_first(const rb_root_t *root);
HAPI rb_node_t *rb_last(const rb_root_t *root);
HAPI rb_node_t *rb_next(const rb_node_t *node);
HAPI rb_node_t *rb_prev(const rb_node_t *node);

HAPI rb_node_t *
rb_get_parent(const rb_node_t *rb)
{
        return (rb_node_t *)(rb->rb_parent_color & ~3);
}

HAPI void
rb_set_parent(rb_node_t *rb, rb_node_t *parent)
{
        rb->rb_parent_color = (usz)parent | (rb->rb_parent_color & 3);
}

HAPI rb_color_e
rb_get_color(const rb_node_t *rb)
{
        return (rb_color_e)(rb->rb_parent_color & 1);
}

HAPI void
rb_set_color(rb_node_t *rb, const rb_color_e color)
{
        rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

HAPI bool
rb_is_red(const rb_node_t *rb)
{
        return rb_get_color(rb) == RB_RED;
}

HAPI bool
rb_is_black(const rb_node_t *rb)
{
        return rb_get_color(rb) == RB_BLACK;
}

HAPI void
rb_set_red(rb_node_t *rb)
{
        rb_set_color(rb, RB_RED);
}

HAPI void
rb_set_black(rb_node_t *rb)
{
        rb_set_color(rb, RB_BLACK);
}

HAPI bool
rb_is_unlinked(const rb_node_t *rb)
{
        return rb->rb_parent_color == (usz)rb;
}

HAPI void
rb_link_node(rb_node_t *node, rb_node_t *parent, rb_node_t **rb_link)
{
        node->rb_parent_color = (usz)parent;
        node->rb_left = node->rb_right = NULL;
        *rb_link                       = node;
}

HAPI void
rb_rotate_left(rb_node_t *node, rb_root_t *root)
{
        rb_node_t *right  = node->rb_right;
        rb_node_t *parent = rb_get_parent(node);

        if ((node->rb_right = right->rb_left))
                rb_set_parent(right->rb_left, node);

        right->rb_left = node;
        rb_set_parent(right, parent);

        if (parent) {
                if (node == parent->rb_left)
                        parent->rb_left = right;
                else
                        parent->rb_right = right;
        } else
                root->rb_node = right;

        rb_set_parent(node, right);
}

HAPI void
rb_rotate_right(rb_node_t *node, rb_root_t *root)
{
        rb_node_t *left   = node->rb_left;
        rb_node_t *parent = rb_get_parent(node);

        if ((node->rb_left = left->rb_right))
                rb_set_parent(left->rb_right, node);

        left->rb_right = node;
        rb_set_parent(left, parent);

        if (parent) {
                if (node == parent->rb_right)
                        parent->rb_right = left;
                else
                        parent->rb_left = left;
        } else
                root->rb_node = left;

        rb_set_parent(node, left);
}

HAPI void
rb_insert_color(rb_node_t *node, rb_root_t *root)
{
        rb_node_t *parent, *uncle;

        while (((parent = rb_get_parent(node))) && rb_is_red(parent)) {
                rb_node_t *gparent = rb_get_parent(parent);

                if (parent == gparent->rb_left) {
                        uncle = gparent->rb_right;

                        if (uncle && rb_is_red(uncle)) {
                                rb_set_black(uncle);
                                rb_set_black(parent);
                                rb_set_red(gparent);
                                node = gparent;
                                continue;
                        }

                        if (parent->rb_right == node) {
                                rb_rotate_left(parent, root);
                                rb_node_t *tmp = parent;
                                parent         = node;
                                node           = tmp;
                        }

                        rb_set_black(parent);
                        rb_set_red(gparent);
                        rb_rotate_right(gparent, root);
                } else {
                        uncle = gparent->rb_left;

                        if (uncle && rb_is_red(uncle)) {
                                rb_set_black(uncle);
                                rb_set_black(parent);
                                rb_set_red(gparent);
                                node = gparent;
                                continue;
                        }

                        if (parent->rb_left == node) {
                                rb_rotate_right(parent, root);
                                rb_node_t *tmp = parent;
                                parent         = node;
                                node           = tmp;
                        }

                        rb_set_black(parent);
                        rb_set_red(gparent);
                        rb_rotate_left(gparent, root);
                }
        }

        rb_set_black(root->rb_node);
}

HAPI void
rb_erase_color(rb_node_t *node, rb_node_t *parent, rb_root_t *root)
{
        rb_node_t *sibling;

        while ((!node || rb_is_black(node)) && node != root->rb_node) {
                if (parent->rb_left == node) {
                        sibling = parent->rb_right;
                        if (rb_is_red(sibling)) {
                                rb_set_black(sibling);
                                rb_set_red(parent);
                                rb_rotate_left(parent, root);
                                sibling = parent->rb_right;
                        }

                        if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                            (!sibling->rb_right || rb_is_black(sibling->rb_right))) {
                                rb_set_red(sibling);
                                node   = parent;
                                parent = rb_get_parent(node);
                        } else {
                                if (!sibling->rb_right || rb_is_black(sibling->rb_right)) {
                                        rb_set_black(sibling->rb_left);
                                        rb_set_red(sibling);
                                        rb_rotate_right(sibling, root);
                                        sibling = parent->rb_right;
                                }

                                rb_set_color(sibling, rb_get_color(parent));
                                rb_set_black(parent);
                                rb_set_black(sibling->rb_right);
                                rb_rotate_left(parent, root);
                                node = root->rb_node;
                                break;
                        }
                } else {
                        sibling = parent->rb_left;
                        if (rb_is_red(sibling)) {
                                rb_set_black(sibling);
                                rb_set_red(parent);
                                rb_rotate_right(parent, root);
                                sibling = parent->rb_left;
                        }

                        if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                            (!sibling->rb_right || rb_is_black(sibling->rb_right))) {
                                rb_set_red(sibling);
                                node   = parent;
                                parent = rb_get_parent(node);
                        } else {
                                if (!sibling->rb_left || rb_is_black(sibling->rb_left)) {
                                        rb_set_black(sibling->rb_right);
                                        rb_set_red(sibling);
                                        rb_rotate_left(sibling, root);
                                        sibling = parent->rb_left;
                                }

                                rb_set_color(sibling, rb_get_color(parent));
                                rb_set_black(parent);
                                rb_set_black(sibling->rb_left);
                                rb_rotate_right(parent, root);
                                node = root->rb_node;
                                break;
                        }
                }
        }

        if (node)
                rb_set_black(node);
}

HAPI void
rb_erase(rb_node_t *node, rb_root_t *root)
{
        rb_node_t *child, *parent;
        rb_color_e color;

        if (!node->rb_left) {
                child = node->rb_right;
        } else if (!node->rb_right)
                child = node->rb_left;
        else {
                const rb_node_t *old = node;
                rb_node_t       *left;

                node = node->rb_right;
                while ((left = node->rb_left))
                        node = left;

                child  = node->rb_right;
                parent = rb_get_parent(node);
                color  = rb_get_color(node);

                if (child)
                        rb_set_parent(child, parent);

                if (parent == old) {
                        parent->rb_right = child;
                        parent           = node;
                } else
                        parent->rb_left = child;

                node->rb_parent_color = old->rb_parent_color;
                node->rb_left         = old->rb_left;
                node->rb_right        = old->rb_right;

                if (rb_get_parent(old)) {
                        if (rb_get_parent(old)->rb_left == old)
                                rb_get_parent(old)->rb_left = node;
                        else
                                rb_get_parent(old)->rb_right = node;
                } else
                        root->rb_node = node;

                rb_set_parent(old->rb_left, node);
                if (old->rb_right)
                        rb_set_parent(old->rb_right, node);

                goto color;
        }

        parent = rb_get_parent(node);
        color  = rb_get_color(node);

        if (child)
                rb_set_parent(child, parent);

        if (parent) {
                if (parent->rb_left == node)
                        parent->rb_left = child;
                else
                        parent->rb_right = child;
        } else
                root->rb_node = child;

color:
        if (color == RB_BLACK)
                rb_erase_color(child, parent, root);
}

HAPI rb_node_t *
rb_first(const rb_root_t *root)
{
        rb_node_t *n = root->rb_node;
        if (!n)
                return NULL;
        while (n->rb_left)
                n = n->rb_left;
        return n;
}

HAPI rb_node_t *
rb_last(const rb_root_t *root)
{
        rb_node_t *n = root->rb_node;
        if (!n)
                return NULL;
        while (n->rb_right)
                n = n->rb_right;
        return n;
}

HAPI rb_node_t *
rb_next(const rb_node_t *node)
{
        if (node->rb_right) {
                node = node->rb_right;
                while (node->rb_left)
                        node = node->rb_left;
                return (rb_node_t *)node;
        }

        rb_node_t *parent = rb_get_parent(node);
        while (parent && node == parent->rb_right) {
                node   = parent;
                parent = rb_get_parent(parent);
        }
        return parent;
}

HAPI rb_node_t *
rb_prev(const rb_node_t *node)
{
        if (node->rb_left) {
                node = node->rb_left;
                while (node->rb_right)
                        node = node->rb_right;
                return (rb_node_t *)node;
        }

        rb_node_t *parent = rb_get_parent(node);
        while (parent && node == parent->rb_left) {
                node   = parent;
                parent = rb_get_parent(parent);
        }
        return parent;
}

#endif // !RBTREE_H
