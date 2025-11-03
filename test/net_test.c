#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "comm/net.h"
#include "ds/mp.h"
#include "util/printops.h"

net_t net;
mp_t  mp;

#define WRITE_THREAD_NUM 255

u8       LOG_FLUSH_BUF[128];
u8       LOG_BUF[1024 * 1024];
mpsc_p_t PRODUCERS[WRITE_THREAD_NUM];

HAPI void
log_stdout(void *fp, const u8 *src, size_t size)
{
        fwrite(src, size, 1, fp);
        fflush(fp);
}

void
on_send_done(net_ch_t *ch, void *buf, int ret)
{
        ARG_UNUSED(ch);

        printf("[SEND][%llu] %d bytes: %.*s\n", get_mono_ts_ms(), ret, ret < 0 ? 0 : ret, (char *)buf);

        mp_free(&mp, buf);
}

void
on_recv_done(net_ch_t *ch, void *buf, int ret)
{
        ARG_UNUSED(ch);

        if (ret == -ETIME)
                print_err("[RECV][%llu] timeout occurred\n", get_mono_ts_ms());
        else
                printf("[RECV][%llu] %d bytes: %.*s\n", get_mono_ts_ms(), ret, ret < 0 ? 0 : ret, (char *)buf);

        mp_free(&mp, buf);
}

void *
send_recv_thread(void *arg)
{
        net_ch_t *ch  = (net_ch_t *)arg;
        u64       cnt = 0;
        while (true) {
                char *tx_buf = mp_calloc(&mp, 64);
                if (!tx_buf) {
                        printf("mempool full!\n");
                        continue;
                }
                sprintf(tx_buf, "CNT_%llu", cnt++);

                char *rx_buf = mp_calloc(&mp, 64);
                if (!rx_buf) {
                        printf("mempool full!\n");
                        continue;
                }

                const u64 begin_us = get_mono_ts_us();
                net_send_recv(&net, ch, tx_buf, strlen(tx_buf), rx_buf, 1024, MS2US(200));
                const u64 end_us = get_mono_ts_us();
                printf("cnt: %llu, elapsed: %llu us\n", cnt, end_us - begin_us);
                delay_ms(2, SPIN);
        }
        return NULL;
}

HAPI int
init(void)
{
        mp_init(&mp);

        FILE *fp = fopen("net_log.bin", "a+");

        net_cfg_t net_cfg = {
            .e_type   = NET_TYPE_UDP,
            .mp       = &mp,
            .ring_len = 256,
            .log_cfg =
                {
                    .e_mode     = LOG_MODE_SYNC,
                    .e_level    = LOG_LEVEL_DEBUG,
                    .fp         = fp,
                    .buf        = (void *)LOG_BUF,
                    .cap        = sizeof(LOG_BUF),
                    .flush_buf  = LOG_FLUSH_BUF,
                    .flush_cap  = sizeof(LOG_FLUSH_BUF),
                    .producers  = (mpsc_p_t *)&PRODUCERS,
                    .nproducers = ARRAY_LEN(PRODUCERS),
                    .f_flush    = log_stdout,
                    .f_get_ts   = get_mono_ts_us,
                },
        };
        int ret = net_init(&net, net_cfg);

        // net_resp_t  devs[255];
        // const char *tx_buf = "{\"method\":\"GET\",\"reqTarget\":\"/custom\",\"cnt\":\"    "
        //                      "0\",\"type\":true,\"mcu_fw_version\":true,\"mac_address\":true,\"static_IP\":true}";
        // net_broadcast(&net, "192.168.137.255", 2334, tx_buf, strlen(tx_buf), devs, 1000);

        // for (int i = 0; i < 255; i++)
        //         printf("%s\n", devs[i].buf);

        return ret;
}

HAPI int
exec(void)
{
        net_ch_t ch = {
            .dst_ip = "127.0.0.1",
            //     .dst_ip   = "192.168.137.101",
            .dst_port = 2333,
            //     .src_ip    = "127.0.0.1",
            //     .src_port  = 2334,
            .e_mode    = NET_MODE_SYNC_YIELD,
            .f_send_cb = on_send_done,
            .f_recv_cb = on_recv_done,
        };

        int ret = net_add_ch(&net, &ch);
        if (ret < 0)
                return -1;

        pthread_t tid;
        pthread_create(&tid, NULL, send_recv_thread, &ch);

        while (true) {
                // net_poll(&net);
                // delay_ms(10, YIELD);
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
