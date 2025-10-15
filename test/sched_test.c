#include <stdint.h>
#include <stdio.h>

#include "sched/sched.h"
#include "util/typedef.h"

sched_t cfs;

u64 fake_time_us = 0;

u64 get_ts_us(void) { return fake_time_us; }

void task_cb(void *arg) {
        const char *name = (const char *)arg;
        printf("[%.6llu] Task %s executed\n", fake_time_us, name);
}

int main(void) {
        sched_cfg_t cfg_cfg = {.cpu_id = 19, .type = SCHED_TYPE_CFS};
        sched_init(&cfs, cfg_cfg);

        sched_task_cfg_t tasks[] = {
            {
                .id           = 0,
                .priority     = 5,
                .exec_freq    = 1000,
                .exec_cnt_max = 3,
                .delay        = 0,
                .f_cb         = task_cb,
                .arg          = "A",
            },
            {
                .id           = 1,
                .priority     = 1,
                .exec_freq    = 500,
                .exec_cnt_max = 5,
                .delay        = 0,
                .f_cb         = task_cb,
                .arg          = "B",
            },
            {
                .id           = 2,
                .priority     = 3,
                .exec_freq    = 800,
                .exec_cnt_max = 2,
                .delay        = 0,
                .f_cb         = task_cb,
                .arg          = "C",
            },
        };

        cfs.lo.tasks[0].cfg = tasks[0];
        cfs.lo.tasks[1].cfg = tasks[1];
        cfs.lo.tasks[2].cfg = tasks[2];
        cfs.ops.f_get_ts    = get_ts_us;

        for (int i = 0; i < 3; i++) {
                sched_task_t *t        = &cfs.lo.tasks[cfs.lo.ntasks++];
                t->cfg                 = tasks[i];
                t->status.e_state      = SCHED_TASK_STATE_RUNNING;
                t->status.create_ts    = get_ts_us();
                t->status.next_exec_ts = get_ts_us();

                // 插入 CFS 红黑树
                sched_cfs_insert_task(&cfs, t);
        }

        for (int step = 0; step < 10; step++) {
                fake_time_us += 500; // 每次增加 0.15 ms

                printf("\n=== CFS step %d ===\n", step);
                sched_exec(&cfs);
        }

        return 0;
}
