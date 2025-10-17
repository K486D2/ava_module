#ifndef SCHED_H
#define SCHED_H

#include "ds/rbtree.h"
#include "util/util.h"

#include "thread.h"

#ifndef SCHED_TASK_MAX
#define SCHED_TASK_MAX 8
#endif

typedef void (*sched_cb_f)(void *arg);

typedef union {
        struct {
                usz prev_idx;
        } fcfs;
        struct {
                rb_root_t rb_root;
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
 *     +----------+  |  +----------+
 *     |  STOPPED |<--->| RUNNING  |
 *     +----------+     +----------+
 *           \              |
 *            \             |
 *             \            v
 *              \---->  +--------+
 *                      |  DEAD  |
 *                      +--------+
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
        usz        delay_tick;   // 初始延时
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
        rb_node_t           rb_node;
} sched_task_t;

typedef u64 (*sched_get_ts_f)(void);
typedef sched_task_t *(*sched_get_task_f)(struct sched *sched);
typedef void (*sched_insert_task_f)(struct sched *sched, sched_task_t *task);
typedef void (*sched_remove_task_f)(struct sched *sched, sched_task_t *task);

typedef enum {
        SCHED_TYPE_FCFS,
        SCHED_TYPE_CFS,
} sched_type_e;

typedef enum {
        SCHED_TICK_US,
        SCHED_TICK_MS,
} sched_tick_e;

typedef struct {
        u8           cpu_id;
        sched_type_e e_type;
        sched_tick_e e_tick;
} sched_cfg_t;

typedef struct {
        f32            elapsed_us;
        usz            curr_ts;
        usz            ntasks;
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

HAPI u64
sched_hz2tick(sched_t *sched, f32 hz)
{
        DECL_PTRS(sched, cfg);

        switch (cfg->e_tick) {
                case SCHED_TICK_US:
                        return HZ_TO_US(hz);
                case SCHED_TICK_MS:
                        return HZ_TO_MS(hz);
                default:
                        return 0;
        }
}

HAPI int
sched_cfs_task_cmp(const sched_task_t *a, const sched_task_t *b)
{
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

HAPI void
sched_cfs_insert_task(sched_t *sched, sched_task_t *task)
{
        DECL_PTRS(sched, lo);

        rb_root_t  *rb_root     = &lo->algo_ctx.cfs.rb_root;
        rb_node_t **new_rb_node = &rb_root->rb_node;
        rb_node_t  *rb_parent   = NULL;

        if (task->cfg.id >= SCHED_TASK_MAX)
                return;

        while (*new_rb_node) {
                sched_task_t *curr = CONTAINER_OF(*new_rb_node, sched_task_t, rb_node);
                int           cmp  = sched_cfs_task_cmp(task, curr);
                rb_parent          = *new_rb_node;
                new_rb_node        = (cmp < 0) ? &(*new_rb_node)->rb_left : &(*new_rb_node)->rb_right;
        }
        rb_link_node(&task->rb_node, rb_parent, new_rb_node);
        rb_insert_color(&task->rb_node, rb_root);
}

HAPI void
sched_cfs_remove_task(sched_t *sched, sched_task_t *task)
{
        DECL_PTRS(sched, lo);

        rb_root_t *rb_root = &lo->algo_ctx.cfs.rb_root;
        if (task->rb_node.__rb_parent_color) {
                rb_erase(&task->rb_node, rb_root);
                memset(&task->rb_node, 0, sizeof(rb_node_t));
        }
}

HAPI sched_task_t *
sched_cfs_get_task(sched_t *sched)
{
        DECL_PTRS(sched, lo);

        rb_root_t *rb_root = &lo->algo_ctx.cfs.rb_root;
        rb_node_t *rb_node = rb_first(rb_root);
        if (!rb_node)
                return NULL;
        return CONTAINER_OF(rb_node, sched_task_t, rb_node);
}

HAPI sched_task_t *
sched_fcfs_get_task(sched_t *sched)
{
        DECL_PTRS(sched, lo);

        usz prev_idx = lo->algo_ctx.fcfs.prev_idx;
        for (usz i = 0; i < lo->ntasks; ++i) {
                usz           idx = (prev_idx + i) % lo->ntasks;
                sched_task_t *t   = &lo->tasks[idx];
                if (t->status.e_state == SCHED_TASK_STATE_RUNNING) {
                        lo->algo_ctx.fcfs.prev_idx = idx + 1;
                        return t;
                }
        }
        return NULL;
}

HAPI int
sched_add_task(sched_t *sched, sched_task_cfg_t task_cfg)
{
        DECL_PTRS(sched, lo, ops);

        sched_task_t *task        = &lo->tasks[lo->ntasks];
        task->cfg                 = task_cfg;
        task->status.e_state      = SCHED_TASK_STATE_RUNNING;
        task->status.create_ts    = ops->f_get_ts();
        task->status.next_exec_ts = task->status.create_ts + task->cfg.delay_tick;

        lo->ntasks++;
        if (sched->cfg.e_type == SCHED_TYPE_CFS)
                ops->f_insert_task(sched, task);

        return 0;
}

HAPI int
sched_init(sched_t *sched, sched_cfg_t sched_cfg)
{
        DECL_PTRS(sched, cfg, ops);

        *cfg = sched_cfg;

        switch (cfg->e_type) {
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
        sched_thread_init(sched, cfg->cpu_id);
        return 0;
}

HAPI int
sched_exec(sched_t *sched)
{
        DECL_PTRS(sched, ops, lo);

        lo->curr_ts = ops->f_get_ts();

        sched_task_t *task = ops->f_get_task(sched);
        if (!task || !task->cfg.f_cb)
                return -MEINVAL;

        if (lo->curr_ts - task->status.create_ts < task->cfg.delay_tick)
                return 0;

        if (lo->curr_ts < task->status.next_exec_ts)
                return 0;

        if (sched->cfg.e_type == SCHED_TYPE_CFS)
                sched_cfs_remove_task(sched, task);

        u64 begin_ts = lo->curr_ts;
        task->cfg.f_cb(task->cfg.arg);
        u64 end_ts = ops->f_get_ts();

        task->status.exec_cnt++;
        task->status.elapsed_us = (f32)(end_ts - begin_ts);

        if (task->cfg.exec_cnt_max == 0 || task->status.exec_cnt < task->cfg.exec_cnt_max) {
                task->status.next_exec_ts = end_ts + sched_hz2tick(sched, task->cfg.exec_freq);
                if (sched->cfg.e_type == SCHED_TYPE_CFS)
                        sched_cfs_insert_task(sched, task);
        } else
                task->status.e_state = SCHED_TASK_STATE_DEAD;

        return 0;
}

#endif // !SCHED_H
