#include <string.h>

#include "../communicate/net.h"

net_t net;

static int init(void) {
  net_cfg_t net_cfg = {.type = NET_TYPE_UDP};
  net_init(&net, net_cfg);
}

static int exec(void) {
  net_ch_t ch = {
      .remote_ip = "192.168.137.101",
      // .remote_ip   = "127.0.0.1",
      .remote_port = 2333,
      .local_ip    = "192.168.137.1",
      // .local_ip   = "127.0.0.1",
      .local_port = 2334,
      .tx_flag    = MSG_WAITALL,
      .rx_flag    = MSG_WAITALL,
  };

  int ret;
  ret = net_add_ch(&net, &ch);
  if (ret < 0)
    return -1;

  u64  cnt         = 0;
  char txbuf[1024] = {0};
  char rxbuf[1024] = {0};

  while (true) {
    sprintf(txbuf, "Hello World, cnt: %llu!", cnt);
    ret = net_send(&ch, txbuf, strlen(txbuf));
    if (ret < 0)
      printf("recv: %d\n", ret);
    ret = net_recv(&ch, rxbuf, sizeof(rxbuf), 1);
    if (ret > 0)
      printf("%s\n", rxbuf);
  }
  return 0;
}

int main() {
  init();
  exec();
  return 0;
}
