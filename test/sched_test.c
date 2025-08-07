#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>

#include "../scheduler/scheduler.h"
#include "../util/util.h"

sched_t sched;

typedef struct {
  u32  task_id;
  u32  cnt;
  char name[32];
} task_data_t;

// 任务1：高频任务，每100ms执行一次
static void task1_callback(void *arg) {
  task_data_t *data = (task_data_t *)arg;
  data->cnt++;
  u64 ts = get_mono_ts_ms();
  cprintf(COLOR_GREEN, "[Task1] %s: EXEC_CNT=%u, TIMESTAMP=%llu ms\n", data->name, data->cnt, ts);
}

// 任务2：中频任务，每500ms执行一次
static void task2_callback(void *arg) {
  task_data_t *data = (task_data_t *)arg;
  data->cnt++;
  u64 ts = get_mono_ts_ms();
  cprintf(COLOR_BLUE, "[Task2] %s: EXEC_CNT=%u, TIMESTAMP=%llu ms\n", data->name, data->cnt, ts);
}

// 任务3：低频任务，每1000ms执行一次
static void task3_callback(void *arg) {
  task_data_t *data = (task_data_t *)arg;
  data->cnt++;
  u64 ts = get_mono_ts_ms();
  cprintf(COLOR_YELLOW, "[Task3] %s: EXEC_CNT=%u, TIMESTAMP=%llu ms\n", data->name, data->cnt, ts);
}

// 任务4：有限次数任务，每200ms执行一次，最多执行10次
static void task4_callback(void *arg) {
  task_data_t *data = (task_data_t *)arg;
  data->cnt++;
  u64 ts = get_mono_ts_ms();
  cprintf(COLOR_RED, "[Task4] %s: EXEC_CNT=%u, TIMESTAMP=%llu ms\n", data->name, data->cnt, ts);
}

int main() {
  sched_cfg_t sched_cfg = {.exec_freq = 1000, .cpu_id = 10};

  sched.ops.f_get_ts = get_mono_ts_us;
  sched_init(&sched, sched_cfg);

  printf("=== SCHED TEST START ===\n");
  printf("SCHED FREQ: %f Hz\n", sched_cfg.exec_freq);
  printf("BIND CPU: %u\n", sched_cfg.cpu_id);
  printf("TIMESTAMP FUNC: get_mono_ts_us\n\n");

  task_data_t task1_data = {1, 0, "HIGH FREQ TASK"};
  task_data_t task2_data = {2, 0, "MEDIUM FREQ TASK"};
  task_data_t task3_data = {3, 0, "LOW FREQ TASK"};
  task_data_t task4_data = {4, 0, "FINITE TASK"};

  // 创建任务1：高频任务，PRIORITY1
  sched_task_cfg_t task1 = {
      .id           = 1,
      .priority     = 1,
      .exec_freq    = 10,
      .exec_cnt_max = 0,
      .delay        = 0,
      .f_cb         = task1_callback,
      .arg          = &task1_data,
  };

  // 创建任务2：中频任务，PRIORITY2
  sched_task_cfg_t task2 = {
      .id           = 2,
      .priority     = 2,
      .exec_freq    = 2,
      .exec_cnt_max = 0,
      .delay        = 100,
      .f_cb         = task2_callback,
      .arg          = &task2_data,
  };

  // 创建任务3：低频任务，PRIORITY3
  sched_task_cfg_t task3 = {
      .id           = 3,
      .priority     = 3,
      .exec_freq    = 1,
      .exec_cnt_max = 0,
      .delay        = 200,
      .f_cb         = task3_callback,
      .arg          = &task3_data,
  };

  // 创建任务4：有限次数任务，PRIORITY1
  sched_task_cfg_t task4 = {
      .id           = 4,
      .priority     = 1,
      .exec_freq    = 5,
      .exec_cnt_max = 10,
      .delay        = 300,
      .f_cb         = task4_callback,
      .arg          = &task4_data,
  };

  printf("REGISTER TASK...\n");
  sched_register_task(&sched, task1);
  sched_register_task(&sched, task2);
  sched_register_task(&sched, task3);
  sched_register_task(&sched, task4);
  printf("REGISTER TASK FINISH, TOTAL %u TASK\n\n", sched.lo.tasks_num);

  printf("TASK INFO:\n");
  printf("Task1: ID=%u, FREQ=%llu Hz, PRIORITY=%u, DELAY=%llu ms\n",
         task1.id,
         task1.exec_freq,
         task1.priority,
         task1.delay);
  printf("Task2: ID=%u, FREQ=%llu Hz, PRIORITY=%u, DELAY=%llu ms\n",
         task2.id,
         task2.exec_freq,
         task2.priority,
         task2.delay);
  printf("Task3: ID=%u, FREQ=%llu Hz, PRIORITY=%u, DELAY=%llu ms\n",
         task3.id,
         task3.exec_freq,
         task3.priority,
         task3.delay);
  printf("Task4: ID=%u, FREQ=%llu Hz, PRIORITY=%u, DELAY=%llu ms, MAX_EXEC_CNT=%llu\n",
         task4.id,
         task4.exec_freq,
         task4.priority,
         task4.delay,
         task4.exec_cnt_max);
  printf("\nSTART EXEC TASK...\n");
  printf("PRESS Ctrl+C TO STOP TASK\n\n");

  u64 start_ts      = get_mono_ts_ms();
  u64 last_print_ts = start_ts;

  while (1) {
    // 每1秒打印一次统计信息
    u64 curr_ts = get_mono_ts_ms();
    if (curr_ts - last_print_ts >= 1000) {
      printf("\n=== STATISTICS (RUN TIME: %llu ms) ===\n", curr_ts - start_ts);
      printf("Task1 EXEC_CNT: %u\n", task1_data.cnt);
      printf("Task2 EXEC_CNT: %u\n", task2_data.cnt);
      printf("Task3 EXEC_CNT: %u\n", task3_data.cnt);
      printf("Task4 EXEC_CNT: %u\n", task4_data.cnt);
      printf("================================\n\n");
      last_print_ts = curr_ts;
    }
  }

  return 0;
}
