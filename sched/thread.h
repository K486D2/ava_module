#ifndef SCHED_THREAD_H
#define SCHED_THREAD_H

#include <stdio.h>

#include "../util/macrodef.h"
#include "../util/typedef.h"

struct sched;
HAPI int sched_exec(struct sched *sched);

#ifdef __linux__
#include <pthread.h>
#include <sched.h>

HAPI void *
sched_thread_exec(void *arg)
{
        struct sched *t = (struct sched *)arg;
        while (true)
                sched_exec(t);
        return NULL;
}

HAPI void
sched_bind_thread_to_cpu(pthread_t thread_tid, int cpu_id)
{
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        int ret = pthread_setaffinity_np(thread_tid, sizeof(cpu_set_t), &cpuset);
        if (ret)
                printf("[SCHED]set thread affinity failed, errcode: %d\n", ret);
        printf("[SCHED]bind thread to CPU %d success\n", cpu_id);
}
#elif defined(_WIN32)
#include <windows.h>

HAPI DWORD WINAPI sched_thread_exec(LPVOID arg);
HAPI void
sched_bind_thread_to_cpu(HANDLE thread_handle, int cpu_id)
{
        DWORD_PTR mask = 1u << cpu_id;
        DWORD_PTR ret  = SetThreadAffinityMask(thread_handle, mask);
        if (!ret)
                printf("[SCHED]set thread affinity failed, errcode: %lu\n", GetLastError());
        printf("[SCHED]bind thread to CPU %d success\n", cpu_id);
}

HAPI DWORD WINAPI
sched_thread_exec(LPVOID arg)
{
        struct sched *t = (struct sched *)arg;
        while (true)
                sched_exec(t);
        return 0;
}
#endif

HAPI void
sched_thread_init(void *arg, int cpu_id)
{
#ifdef __linux__
        pthread_t sched_tid;
        int       ret = pthread_create(&sched_tid, NULL, sched_thread_exec, arg);
        if (ret != 0) {
                printf("[SCHED]create thread failed, errcode: %d\n", ret);
                return;
        }
#elif defined(_WIN32)
        DWORD  thread_id;
        HANDLE sched_tid = CreateThread(NULL,              // 默认安全属性
                                        0,                 // 默认堆栈大小
                                        sched_thread_exec, // 线程函数
                                        arg,               // 传递给线程函数的参数
                                        0,                 // 默认创建标志
                                        &thread_id         // 用于接收线程ID
        );
        if (sched_tid == NULL) {
                printf("[SCHED]create thread failed, errcode: %lu\n", GetLastError());
                return;
        }
#endif

#if defined(__linux__) || defined(_WIN32)
        sched_bind_thread_to_cpu(sched_tid, cpu_id);
#endif
}

#endif // !SCHED_THREAD_H
