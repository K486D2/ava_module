#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "container/rbtree.h"
#include "util/util.h"

#include "sched_thread.h"

#ifndef SCHED_TASK_MAX
#define SCHED_TASK_MAX 3
#endif

typedef void (*sched_cb_f)(void *arg);

typedef union {
        struct {
                usz prev_idx;
        } fcfs;
        struct {
                rb_root_t rbtree;
                rb_node_t nodes[SCHED_TASK_MAX];
        } cfs;
} sched_algo_ctx;

/*
 *             +-----------+
 *             |  SLEEPING |
 *             +-----------+
 *                   ^
 *                   |
 *                   | wakeup
 *                   |
 *     +----------+  |  +-----------+
 *     |  STOPPED |<--->| RUNNING   |
 *     +----------+     +-----------+
 *             \              |
 *              \             |
 *               \            v
 *                \---->  +--------+
 *                        |  DEAD  |
 *                        +--------+
 *
 */
typedef enum {
        SCHED_TASK_STATE_RUNNING,  // 就绪或运行
        SCHED_TASK_STATE_SLEEPING, // 阻塞
        SCHED_TASK_STATE_STOPPED,  // 手动挂起
        SCHED_TASK_STATE_DEAD,     // 已结束
} sched_task_state_e;

typedef struct {
        usz        id;           // 任务 ID
        u32        priority;     // 任务优先级, 数值越小优先级越高
        usz        exec_freq;    // 执行频率
        usz        exec_cnt_max; // 最多执行次数
        usz        delay;        // 初始延时
        sched_cb_f f_cb;         // 回调函数
        void      *arg;          // 回调参数
} sched_task_cfg_t;

typedef struct {
        sched_task_state_e e_state;
        usz                exec_cnt;
        f32                elapsed_us;
        usz                create_ts;
        usz                next_exec_ts;
} sched_task_status_t;

typedef struct {
        sched_task_cfg_t    cfg;
        sched_task_status_t status;
        void               *node; // 调度算法节点
} sched_task_t;

typedef u64 (*sched_get_ts_f)(void);
typedef sched_task_t *(*sched_get_task_f)(struct sched *sched);
typedef void (*sched_insert_task_f)(struct sched *sched, sched_task_t *task);
typedef void (*sched_remove_task_f)(struct sched *sched, sched_task_t *task);

typedef enum {
        SCHED_TYPE_FCFS,
        SCHED_TYPE_CFS,
} sched_type_e;

typedef struct {
        u8           cpu_id;
        sched_type_e type;
} sched_cfg_t;

typedef struct {
        f32            elapsed_us;
        usz            curr_ts;
        usz            tasks_num;
        sched_task_t   tasks[SCHED_TASK_MAX];
        sched_algo_ctx algo_ctx;
} sched_lo_t;

typedef struct {
        sched_get_ts_f      f_get_ts;
        sched_get_task_f    f_get_task;
        sched_insert_task_f f_insert_task;
        sched_remove_task_f f_remove_task;
} sched_ops_t;

typedef struct sched {
        sched_cfg_t cfg;
        sched_lo_t  lo;
        sched_ops_t ops;
} sched_t;

static inline int sched_cfs_task_cmp(const sched_task_t *a, const sched_task_t *b) {
        if (a->status.next_exec_ts < b->status.next_exec_ts)
                return -1;
        if (a->status.next_exec_ts > b->status.next_exec_ts)
                return 1;
        if (a->cfg.priority < b->cfg.priority)
                return -1;
        if (a->cfg.priority > b->cfg.priority)
                return 1;
        return 0;
}

static inline void sched_cfs_insert_task(sched_t *sched, sched_task_t *task) {
        DECL_PTRS_1(sched, lo);

        rb_root_t  *root     = &lo->algo_ctx.cfs.rbtree;
        rb_node_t **new_node = &root->rb_node;
        rb_node_t  *parent   = NULL;

        // 将任务的红黑树节点与任务关联
        rb_node_t *node = &lo->algo_ctx.cfs.nodes[task->cfg.id];
        task->node      = node;

        while (*new_node) {
                sched_task_t *curr = rb_entry(*new_node, sched_task_t, node);
                int           cmp  = sched_cfs_task_cmp(task, curr);
                parent             = *new_node;
                new_node           = (cmp < 0) ? &(*new_node)->rb_left : &(*new_node)->rb_right;
        }
        rb_link_node(node, parent, new_node);
        rb_insert_color(node, root);
}

static inline void sched_cfs_remove_task(sched_t *sched, sched_task_t *task) {
        DECL_PTRS_1(sched, lo);

        rb_root_t *root = &lo->algo_ctx.cfs.rbtree;
        if (task->node) {
                rb_erase((rb_node_t *)task->node, root);
                task->node = NULL;
        }
}

static inline sched_task_t *sched_cfs_get_task(sched_t *sched) {
        DECL_PTRS_1(sched, lo);

        rb_root_t *root = &lo->algo_ctx.cfs.rbtree;
        rb_node_t *node = rb_first(root);
        if (!node)
                return NULL;
        return rb_entry(node, sched_task_t, node);
}

static inline sched_task_t *sched_fcfs_get_task(sched_t *sched) {
        DECL_PTRS_1(sched, lo);

        usz prev_idx = lo->algo_ctx.fcfs.prev_idx;

        for (usz i = 0; i < lo->tasks_num; ++i) {
                usz           idx = (prev_idx + i) % lo->tasks_num;
                sched_task_t *t   = &lo->tasks[idx];
                if (t->status.e_state == SCHED_TASK_STATE_RUNNING) {
                        lo->algo_ctx.fcfs.prev_idx = idx + 1;
                        return t;
                }
        }
        return NULL;
}

static inline int sched_init(sched_t *sched, sched_cfg_t sched_cfg) {
        DECL_PTRS_2(sched, cfg, ops);

        *cfg = sched_cfg;

        switch (cfg->type) {
        case SCHED_TYPE_FCFS: {
                ops->f_get_task    = sched_fcfs_get_task;
                ops->f_insert_task = NULL;
                ops->f_remove_task = NULL;
                break;
        }
        case SCHED_TYPE_CFS: {
                ops->f_get_task    = sched_cfs_get_task;
                ops->f_insert_task = sched_cfs_insert_task;
                ops->f_remove_task = sched_cfs_remove_task;
                break;
        }
        default:
                return -MEINVAL;
        }

        // only run on Linux/Windows
        // sched_thread_init(sched, cfg->cpu_id);
        return 0;
}

static inline int sched_exec(sched_t *sched) {
        DECL_PTRS_2(sched, lo, ops);

        lo->curr_ts = ops->f_get_ts();

        sched_task_t *task = ops->f_get_task(sched);
        if (!task || !task->cfg.f_cb)
                return -MEINVAL;

        if (lo->curr_ts - task->status.create_ts < task->cfg.delay)
                return 0;

        if (lo->curr_ts < task->status.next_exec_ts)
                return 0;

        if (sched->cfg.type == SCHED_TYPE_CFS && task->node)
                sched_cfs_remove_task(sched, task);

        task->status.e_state = SCHED_TASK_STATE_RUNNING;

        u64 begin_ts = ops->f_get_ts();
        task->cfg.f_cb(task->cfg.arg);
        u64 end_ts = ops->f_get_ts();

        task->status.exec_cnt++;
        task->status.elapsed_us = (f32)(end_ts - begin_ts);

        if (task->cfg.exec_cnt_max == 0 || task->status.exec_cnt < task->cfg.exec_cnt_max) {
                task->status.next_exec_ts = end_ts + HZ_TO_US(task->cfg.exec_freq);
                task->status.e_state      = SCHED_TASK_STATE_RUNNING;
                if (sched->cfg.type == SCHED_TYPE_CFS)
                        sched_cfs_insert_task(sched, task);
        } else
                task->status.e_state = SCHED_TASK_STATE_DEAD;

        return 0;
}

#endif // !SCHEDULER_H
