#ifndef SCHEDULER_H
#define SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "thread.h"

#include "../container/rbtree.h"
#include "../util/util.h"

#ifndef SCHED_TASK_MAX
#define SCHED_TASK_MAX 16
#endif

typedef enum {
  SCHED_TYPE_CFS,
} sched_type_e;

typedef struct {
  f32          exec_freq; // 时间戳更新频率
  u8           cpu_id;
  sched_type_e type;
} sched_cfg_t;

typedef void (*sched_cb_f)(void *arg);

typedef enum {
  SCHED_TASK_STATE_BLOCKED,
  SCHED_TASK_STATE_READY,
  SCHED_TASK_STATE_RUNNING,
  SCHED_TASK_STATE_SUSPENDED,
} sched_task_state_e;

typedef struct {
  u32        id;           // 任务ID
  u32        priority;     // 任务优先级, 数值越小优先级越高
  u32        exec_freq;    // 执行频率
  u32        exec_cnt_max; // 执行次数
  u32        delay;        // 初始延时
  sched_cb_f f_cb;         // 回调函数
  void      *arg;          // 回调参数
} sched_task_cfg_t;

typedef struct {
  sched_task_state_e state;
  u32                exec_cnt;
  u32                elapsed;
  u32                create_ts;
  u32                next_exec_ts;
} sched_task_status_t;

typedef struct {
  sched_task_cfg_t    cfg;
  sched_task_status_t status;
  rb_node_t           rb_node;
} sched_task_t;

typedef struct {
  u32          elapsed;
  f32          elapsed_us;
  sched_task_t tasks[SCHED_TASK_MAX];
  u32          tasks_num;
  u32          curr_ts;
  rb_root_t    ready_tree;
} sched_lo_t;

typedef u64 (*sched_get_ts_f)(void);

struct sched;
typedef sched_task_t *(*sched_get_task_f)(struct sched *sched);

typedef struct {
  sched_get_ts_f   f_get_ts;
  sched_get_task_f f_get_task;
} sched_ops_t;

typedef struct sched {
  sched_cfg_t cfg;
  sched_lo_t  lo;
  sched_ops_t ops;
} sched_t;

#define DECL_SCHED_PTRS(sched)                                                                     \
  sched_t     *p   = (sched);                                                                      \
  sched_cfg_t *cfg = &p->cfg;                                                                      \
  sched_lo_t  *lo  = &p->lo;                                                                       \
  sched_ops_t *ops = &p->ops;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(lo);                                                                                  \
  ARG_UNUSED(ops);

#define DECL_SCHED_PTRS_PREFIX(sched, prefix)                                                      \
  sched_t     *prefix##_p   = (sched);                                                             \
  sched_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                    \
  sched_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                     \
  sched_ops_t *prefix##_ops = &prefix##_p->ops;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_lo);                                                                         \
  ARG_UNUSED(prefix##_ops);

static inline i32 sched_task_cmp(const sched_task_t *a, const sched_task_t *b) {
  if (a->cfg.priority < b->cfg.priority)
    return -1;
  if (a->cfg.priority > b->cfg.priority)
    return 1;
  if (a->status.next_exec_ts < b->status.next_exec_ts)
    return -1;
  if (a->status.next_exec_ts > b->status.next_exec_ts)
    return 1;
  return 0;
}

static inline void sched_insert_ready(sched_lo_t *lo, sched_task_t *task) {
  rb_node_t **new_node = &lo->ready_tree.rb_node;
  rb_node_t  *parent   = NULL;
  while (*new_node) {
    sched_task_t *curr_task = rb_entry(*new_node, sched_task_t, rb_node);
    i32           cmp       = sched_task_cmp(task, curr_task);
    parent                  = *new_node;
    if (cmp < 0)
      new_node = &((*new_node)->rb_left);
    else
      new_node = &((*new_node)->rb_right);
  }
  rb_link_node(&task->rb_node, parent, new_node);
  rb_insert_color(&task->rb_node, &lo->ready_tree);
}

static inline void sched_remove_ready(sched_lo_t *lo, sched_task_t *task) {
  rb_erase(&task->rb_node, &lo->ready_tree);
}

static i32 sched_register_task(sched_t *sched, sched_task_cfg_t sched_task_cfg) {
  DECL_SCHED_PTRS(sched);

  lo->curr_ts                                  = ops->f_get_ts();
  lo->tasks[lo->tasks_num].cfg                 = sched_task_cfg;
  lo->tasks[lo->tasks_num].status.create_ts    = lo->curr_ts;
  lo->tasks[lo->tasks_num].status.next_exec_ts = lo->curr_ts + HZ_TO_US(cfg->exec_freq);

  if (lo->tasks[lo->tasks_num].status.state == SCHED_TASK_STATE_READY)
    sched_insert_ready(lo, &lo->tasks[lo->tasks_num]);

  lo->tasks_num++;

  return 0;
}

static i32 sched_set_task_state(sched_t *sched, u32 task_id, sched_task_state_e state) {
  DECL_SCHED_PTRS(sched);

  if (task_id >= lo->tasks_num)
    return -MEINVAL;

  sched_task_t *task = &lo->tasks[task_id];
  task->status.state = state;

  if (state == SCHED_TASK_STATE_READY)
    sched_insert_ready(lo, task);
  else if (state == SCHED_TASK_STATE_SUSPENDED)
    sched_remove_ready(lo, task);

  return 0;
}

static sched_task_t *sched_get_task(sched_t *sched) {
  DECL_SCHED_PTRS(sched);

  rb_node_t *node = rb_first(&lo->ready_tree);
  if (!node)
    return NULL;

  return rb_entry(node, sched_task_t, rb_node);
}

static i32 sched_init(sched_t *sched, sched_cfg_t sched_cfg) {
  DECL_SCHED_PTRS(sched);

  *cfg = sched_cfg;

  switch (cfg->type) {
  case SCHED_TYPE_CFS:
    ops->f_get_task = sched_get_task;
    break;
  default:
    return -MEINVAL;
  }

  // only run on Linux/Windows
  thread_init(sched, cfg->cpu_id);
  return 0;
}

static i32 sched_exec(sched_t *sched) {
  DECL_SCHED_PTRS(sched);

  lo->curr_ts = ops->f_get_ts();

  sched_task_t *task = sched_get_task(sched);
  if (!task->cfg.f_cb) {
    sched_remove_ready(lo, task);
    task->status.state = SCHED_TASK_STATE_SUSPENDED;
    return -MEFAULT;
  }

  if (lo->curr_ts < task->status.next_exec_ts)
    return 0;

  if (lo->curr_ts - task->status.create_ts < task->cfg.delay * HZ_TO_US(cfg->exec_freq))
    return 0;

  u64 begin_ts       = lo->curr_ts;
  task->status.state = SCHED_TASK_STATE_RUNNING;
  sched_remove_ready(lo, task);
  task->cfg.f_cb(task->cfg.arg);
  task->status.exec_cnt++;
  task->status.state   = SCHED_TASK_STATE_READY;
  u64 end_ts           = ops->f_get_ts();
  task->status.elapsed = end_ts - begin_ts;

  if (task->cfg.exec_cnt_max != 0 && task->status.exec_cnt >= task->cfg.exec_cnt_max) {
    task->status.state = SCHED_TASK_STATE_SUSPENDED;
    return 0;
  }

  task->status.next_exec_ts = end_ts + HZ_TO_US(task->cfg.exec_freq);
  sched_insert_ready(lo, task);
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif // !SCHEDULER_H
