#include "comm/net.h"
#include "ds/mp.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

net_t net;
mp_t  mp;

void
on_send_done(net_ch_t *ch, void *buf, int ret)
{
        ARG_UNUSED(ch);
        printf("[CALLBACK] send %d bytes: %.*s\n", ret, ret < 0 ? 0 : ret, (char *)buf);
}

void
on_recv_done(net_ch_t *ch, void *buf, int ret)
{
        ARG_UNUSED(ch);
        printf("[CALLBACK] recv %d bytes: %.*s\n", ret, ret < 0 ? 0 : ret, (char *)buf);
}

void *
send_recv_thread(void *arg)
{
        net_ch_t *ch  = (net_ch_t *)arg;
        u64       cnt = 0;
        while (true) {
                char *tx_buf = mp_alloc(&mp, 64);
                if (!tx_buf) {
                        printf("mempool full!\n");
                        continue;
                }
                sprintf(tx_buf, "CNT_%llu", cnt++);

                char *rx_buf = mp_alloc(&mp, 64);
                if (!rx_buf) {
                        printf("mempool full!\n");
                        continue;
                }

                u64 begin_us = get_mono_ts_us();
                net_send_recv(&net, ch, tx_buf, strlen(tx_buf), rx_buf, 1024, 1000);
                u64 end_us = get_mono_ts_us();
                printf("elapsed: %llu us\n", end_us - begin_us);
                delay_ms(100, YIELD);
        }
        return NULL;
}

HAPI int
init(void)
{
        net_cfg_t net_cfg = {
            .e_type = NET_TYPE_UDP,
            .mp     = &mp,
        };
        int ret = net_init(&net, net_cfg);
        mp_init(&mp);
        return ret;
}

HAPI int
exec(void)
{
        net_ch_t ch = {
            .remote_ip   = "127.0.0.1",
            .remote_port = 2333,
            .local_ip    = "127.0.0.1",
            .local_port  = 2334,
            .e_mode      = NET_MODE_ASYNC,
            .f_send_cb   = on_send_done,
            .f_recv_cb   = on_recv_done,
        };

        int ret = net_add_ch(&net, &ch);
        if (ret < 0)
                return -1;

        pthread_t tid;
        pthread_create(&tid, NULL, send_recv_thread, &ch);

        while (true) {
                net_poll(&net);
                delay_ms(10, YIELD);
        }

        pthread_join(tid, NULL);
        return 0;
}

int
main()
{
        init();
        exec();
        return 0;
}