#ifndef RBTREE_H
#define RBTREE_H

#include <stddef.h>

#include "../util/typedef.h"

typedef enum {
  RB_RED   = 0,
  RB_BLACK = 1,
} rb_color_t;

typedef struct rb_node {
  u64             __rb_parent_color;
  struct rb_node *rb_right;
  struct rb_node *rb_left;
} rb_node_t;

typedef struct {
  rb_node_t *rb_node;
} rb_root_t;

// 初始化根节点
#define RB_ROOT                                                                                    \
  (rb_root_t) {                                                                                    \
    NULL                                                                                           \
  }

// 获取父节点
static inline rb_node_t *rb_parent(const rb_node_t *rb) {
  return (rb_node_t *)(rb->__rb_parent_color & ~3);
}

// 设置父节点
static inline void rb_set_parent(rb_node_t *rb, rb_node_t *p) {
  rb->__rb_parent_color = (u64)p | (rb->__rb_parent_color & 3);
}

static inline rb_color_t rb_color(const rb_node_t *rb) {
  return (rb_color_t)(rb->__rb_parent_color & 1);
}

static inline void rb_set_color(rb_node_t *rb, rb_color_t color) {
  rb->__rb_parent_color = (rb->__rb_parent_color & ~1) | color;
}

static inline bool rb_is_red(const rb_node_t *rb) {
  return rb_color(rb) == RB_RED;
}

static inline bool rb_is_black(const rb_node_t *rb) {
  return rb_color(rb) == RB_BLACK;
}

static inline void rb_set_red(rb_node_t *rb) {
  rb_set_color(rb, RB_RED);
}

static inline void rb_set_black(rb_node_t *rb) {
  rb_set_color(rb, RB_BLACK);
}

static inline bool rb_is_unlinked(const rb_node_t *rb) {
  return rb->__rb_parent_color == (u64)rb;
}

static inline void rb_link_node(rb_node_t *node, rb_node_t *parent, rb_node_t **rb_link) {
  node->__rb_parent_color = (u64)parent;
  node->rb_left = node->rb_right = NULL;
  *rb_link                       = node;
}

// 容器宏(用于从节点获取包含结构)
#define rb_entry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

static inline void __rb_rotate_left(rb_node_t *node, rb_root_t *root) {
  rb_node_t *right  = node->rb_right;
  rb_node_t *parent = rb_parent(node);

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

static inline void __rb_rotate_right(rb_node_t *node, rb_root_t *root) {
  rb_node_t *left   = node->rb_left;
  rb_node_t *parent = rb_parent(node);

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

static inline void rb_insert_color(rb_node_t *node, rb_root_t *root) {
  rb_node_t *parent, *gparent, *uncle;

  while ((parent = rb_parent(node)) && rb_is_red(parent)) {
    gparent = rb_parent(parent);

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
        rb_node_t *tmp;
        __rb_rotate_left(parent, root);
        tmp    = parent;
        parent = node;
        node   = tmp;
      }

      rb_set_black(parent);
      rb_set_red(gparent);
      __rb_rotate_right(gparent, root);
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
        rb_node_t *tmp;
        __rb_rotate_right(parent, root);
        tmp    = parent;
        parent = node;
        node   = tmp;
      }

      rb_set_black(parent);
      rb_set_red(gparent);
      __rb_rotate_left(gparent, root);
    }
  }

  rb_set_black(root->rb_node);
}

static inline void __rb_erase_color(rb_node_t *node, rb_node_t *parent, rb_root_t *root) {
  rb_node_t *sibling;

  while ((!node || rb_is_black(node)) && node != root->rb_node) {
    if (parent->rb_left == node) {
      sibling = parent->rb_right;
      if (rb_is_red(sibling)) {
        rb_set_black(sibling);
        rb_set_red(parent);
        __rb_rotate_left(parent, root);
        sibling = parent->rb_right;
      }

      if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
          (!sibling->rb_right || rb_is_black(sibling->rb_right))) {
        rb_set_red(sibling);
        node   = parent;
        parent = rb_parent(node);
      } else {
        if (!sibling->rb_right || rb_is_black(sibling->rb_right)) {
          rb_set_black(sibling->rb_left);
          rb_set_red(sibling);
          __rb_rotate_right(sibling, root);
          sibling = parent->rb_right;
        }

        rb_set_color(sibling, rb_color(parent));
        rb_set_black(parent);
        rb_set_black(sibling->rb_right);
        __rb_rotate_left(parent, root);
        node = root->rb_node;
        break;
      }
    } else {
      sibling = parent->rb_left;
      if (rb_is_red(sibling)) {
        rb_set_black(sibling);
        rb_set_red(parent);
        __rb_rotate_right(parent, root);
        sibling = parent->rb_left;
      }

      if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
          (!sibling->rb_right || rb_is_black(sibling->rb_right))) {
        rb_set_red(sibling);
        node   = parent;
        parent = rb_parent(node);
      } else {
        if (!sibling->rb_left || rb_is_black(sibling->rb_left)) {
          rb_set_black(sibling->rb_right);
          rb_set_red(sibling);
          __rb_rotate_left(sibling, root);
          sibling = parent->rb_left;
        }

        rb_set_color(sibling, rb_color(parent));
        rb_set_black(parent);
        rb_set_black(sibling->rb_left);
        __rb_rotate_right(parent, root);
        node = root->rb_node;
        break;
      }
    }
  }

  if (node)
    rb_set_black(node);
}

static inline void rb_erase(rb_node_t *node, rb_root_t *root) {
  rb_node_t *child, *parent;
  rb_color_t color;

  if (!node->rb_left) {
    child = node->rb_right;
  } else if (!node->rb_right)
    child = node->rb_left;
  else {
    rb_node_t *old = node, *left;

    node = node->rb_right;
    while ((left = node->rb_left))
      node = left;

    child  = node->rb_right;
    parent = rb_parent(node);
    color  = rb_color(node);

    if (child)
      rb_set_parent(child, parent);

    if (parent == old) {
      parent->rb_right = child;
      parent           = node;
    } else
      parent->rb_left = child;

    node->__rb_parent_color = old->__rb_parent_color;
    node->rb_left           = old->rb_left;
    node->rb_right          = old->rb_right;

    if (rb_parent(old)) {
      if (rb_parent(old)->rb_left == old)
        rb_parent(old)->rb_left = node;
      else
        rb_parent(old)->rb_right = node;
    } else
      root->rb_node = node;

    rb_set_parent(old->rb_left, node);
    if (old->rb_right)
      rb_set_parent(old->rb_right, node);

    goto color;
  }

  parent = rb_parent(node);
  color  = rb_color(node);

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
    __rb_erase_color(child, parent, root);
}

static inline rb_node_t *rb_first(const rb_root_t *root) {
  rb_node_t *n = root->rb_node;
  if (!n)
    return NULL;
  while (n->rb_left)
    n = n->rb_left;
  return n;
}

static inline rb_node_t *rb_last(const rb_root_t *root) {
  rb_node_t *n = root->rb_node;
  if (!n)
    return NULL;
  while (n->rb_right)
    n = n->rb_right;
  return n;
}

static inline rb_node_t *rb_next(const rb_node_t *node) {
  if (node->rb_right) {
    node = node->rb_right;
    while (node->rb_left)
      node = node->rb_left;
    return (rb_node_t *)node;
  }

  rb_node_t *parent = rb_parent(node);
  while (parent && node == parent->rb_right) {
    node   = parent;
    parent = rb_parent(parent);
  }
  return parent;
}

static inline rb_node_t *rb_prev(const rb_node_t *node) {
  if (node->rb_left) {
    node = node->rb_left;
    while (node->rb_right)
      node = node->rb_right;
    return (rb_node_t *)node;
  }

  rb_node_t *parent = rb_parent(node);
  while (parent && node == parent->rb_left) {
    node   = parent;
    parent = rb_parent(parent);
  }
  return parent;
}

#endif // !RBTREE_H
