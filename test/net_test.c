#include <string.h>
#include <unistd.h>

#include "comm/net.h"

net_t net;

static inline int init(void) {
        net_cfg_t net_cfg = {.type = NET_TYPE_UDP};
        return net_init(&net, net_cfg);
}

static inline int exec(void) {
        net_ch_t ch = {
            .remote_ip = "192.168.137.101",
            // .remote_ip   = "127.0.0.1",
            .remote_port = 2333,
            .local_ip    = "192.168.137.1",
            // .local_ip   = "127.0.0.1",
            .local_port = 2334,
            .recv_mode  = NET_RECV_SPIN,
        };

        int ret;
        ret = net_add_ch(&net, &ch);
        if (ret < 0)
                return -1;

        u64  cnt         = 0;
        char txbuf[1024] = {0};
        char rxbuf[1024] = {0};

        while (true) {
                sprintf(txbuf, "%llu", cnt++);
                u64 start = get_mono_ts_us();
                ret       = net_send_recv(&ch, txbuf, strlen(txbuf), rxbuf, sizeof(rxbuf), 1);
                u64 end   = get_mono_ts_us();
                if (ret > 0)
                        printf("recv cnt: %s, elapsed: %llu us\n", rxbuf, end - start);
                sleep(1);
        }
        return 0;
}

int main() {
        init();
        exec();
        return 0;
}
