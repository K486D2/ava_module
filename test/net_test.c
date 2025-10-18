#include <stdio.h>
#include <string.h>

#include "comm/net.h"
#include "ds/mp.h"
#include <unistd.h>

net_t net;
mp_t  mp;

void
on_send_done(net_ch_t *ch, void *buf, int ret)
{
        printf("[回调] 发送 %d 字节: %.*s\n", ret, ret < 0 ? 0 : ret, (char *)buf);
}

void
on_recv_done(net_ch_t *ch, void *buf, int ret)
{
        printf("[回调] 收到 %d 字节: %.*s\n", ret, ret < 0 ? 0 : ret, (char *)buf);
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

        int ret;
        ret = net_add_ch(&net, &ch);
        if (ret < 0)
                return -1;

        u64 cnt = 0;
        for (usz i = 0; i < 100; i++) {
                char *tx_buf = mp_alloc(&mp, 64);
                if (!tx_buf) {
                        printf("内存池不足!\n");
                        break;
                }
                sprintf(tx_buf, "CNT_%llu", cnt++);

                char *rx_buf = mp_alloc(&mp, 1024);
                if (!rx_buf) {
                        printf("内存池不足!\n");
                        break;
                }

                u64 begin_us = get_mono_ts_us();
                net_send_recv(&net, &ch, tx_buf, strlen(tx_buf), rx_buf, 1024, 1000);
                u64 end_us = get_mono_ts_us();
                printf("elapsed: %llu us\n", end_us - begin_us);
        }

        while (1) {
                net_poll(&net);
                delay_ms(1, YIELD);
        }

        return 0;
}

int
main()
{
        init();
        exec();
        return 0;
}