#include <string.h>
#include <unistd.h>

#include "comm/net.h"

net_t net;

HAPI int
init(void)
{
        net_cfg_t net_cfg = {.e_type = NET_TYPE_UDP};
        return net_init(&net, net_cfg);
}

HAPI int
exec(void)
{
        net_ch_t ch = {
            // .remote_ip = "192.168.137.101",
            .remote_ip   = "127.0.0.1",
            .remote_port = 2333,
            // .local_ip    = "192.168.137.1",
            .local_ip   = "127.0.0.1",
            .local_port = 2334,
            //     .e_mode     = NET_SYNC_YIELD,
            //     .e_mode = NET_SYNC_SPIN,
            .e_mode = NET_ASYNC,
        };

        int ret;
        ret = net_add_ch(&net, &ch);
        if (ret < 0)
                return -1;

        u64  cnt          = 0;
        char tx_buf[1024] = {0};
        char rx_buf[1024] = {0};

        while (true) {
                sprintf(tx_buf, "%llu", cnt++);
                u64 begin_us = get_mono_ts_us();
                ret          = net_send_recv(&net, &ch, tx_buf, strlen(tx_buf), rx_buf, sizeof(rx_buf), 1);
                u64 end_us   = get_mono_ts_us();
                if (ret > 0)
                        printf("recv cnt: %s, elapsed: %llu us\n", rx_buf, end_us - begin_us);
                else
                        printf("recv timeout\n");
                delay_s(1, YIELD);
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
