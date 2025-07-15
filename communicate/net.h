#ifndef NET_H
#define NET_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#elif defined(__ZEPHYR__)
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#elif defined(_WIN32)
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#endif

#include <stdio.h>

#include "../container/list.h"
#include "../util/util.h"

#define MAX_IP_SIZE   16U
#define MAX_RESP_SIZE 1024U
#define MAX_IP_NUM    255U

typedef enum {
  NET_TYPE_NULL,
  NET_TYPE_UDP,
  NET_TYPE_TCP,
} net_type_e;

typedef enum {
  NET_LOG_SEND,
  NET_LOG_RECV,
} net_log_type_e;

#pragma pack(push, 1)
typedef struct {
  u64 ts;          // 时间戳
  u8  type;        // 事件类型
  u32 remote_ip;   // 设备IP
  u16 remote_port; // 设备端口
  u32 len;         // 数据长度
} net_log_meta_t;
#pragma pack(pop)

typedef void (*net_logger_f)(FILE *file, const net_log_meta_t *log_meta, const void *log_data);

typedef enum {
  SEND,
  RECV,
  SEND_RECV,
} net_flag_e;

typedef struct {
  list_head_t  node;
  char         remote_ip[MAX_IP_SIZE], local_ip[MAX_IP_SIZE];
  u16          remote_port, local_port;
  socket_t     sock;
  net_logger_f f_logger;
  u32          tx_flag, rx_flag;
} net_ch_t;

typedef struct {
  net_type_e type;
} net_cfg_t;

typedef struct {
  net_ch_t       ch;
  net_log_meta_t log_meta;
} net_lo_t;

typedef struct net {
  net_cfg_t cfg;
  net_lo_t  lo;
} net_t;

#define DECL_NET_PTRS(net)                                                                         \
  net_t     *p   = (net);                                                                          \
  net_cfg_t *cfg = &p->cfg;                                                                        \
  net_lo_t  *lo  = &p->lo;                                                                         \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_NET_PTRS_PREFIX(net, prefix)                                                          \
  net_ctl_t *prefix##_p   = (net);                                                                 \
  net_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  net_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_lo);

static int net_init(net_t *net, net_cfg_t net_cfg) {
  DECL_NET_PTRS(net);

  *cfg = net_cfg;

  list_init(&net->lo.ch.node);

#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
  return 0;
}

static int net_add_ch(net_t *net, net_ch_t *ch) {
  DECL_NET_PTRS(net);

  switch (cfg->type) {
  case NET_TYPE_UDP:
    ch->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    break;
  case NET_TYPE_TCP:
    ch->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    break;
  default:
    break;
  }

  struct sockaddr_in remote_addr = {0};
  remote_addr.sin_family         = AF_INET;
  remote_addr.sin_port           = htons(ch->remote_port);
  remote_addr.sin_addr.s_addr    = inet_addr(ch->remote_ip);

  struct sockaddr_in local_addr = {0};
  local_addr.sin_family         = AF_INET;
  local_addr.sin_port           = htons(ch->local_port);
  local_addr.sin_addr.s_addr    = inet_addr(ch->local_ip);

  int ret;
  // #ifdef __linux__
  //   int flags = fcntl(ch->sock, F_GETFL, 0);
  //   if (flags < 0) {
  //     ret = flags;
  //     goto cleanup;
  //   }
  //   flags |= O_NONBLOCK;
  //   ret = fcntl(ch->sock, F_SETFL, flags);
  //   if (ret < 0)
  //     goto cleanup;
  // #elif defined(_WIN32)
  //   unsigned long mode = 1;
  //   ret                = ioctlsocket(ch->sock, FIONBIO, &mode);
  //   if (ret < 0)
  //     goto cleanup;
  // #endif

  ret = bind(ch->sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
  if (ret < 0)
    goto cleanup;

  ret = connect(ch->sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
  if (ret < 0)
    goto cleanup;

  list_add(&ch->node, &lo->ch.node);
  return 0;

cleanup:
  CLOSE_SOCKET(ch->sock);
  return ret;
}

static int net_send(net_ch_t *ch, void *txbuf, u32 size) {
  int ret = send(ch->sock, txbuf, size, ch->tx_flag);
  return ret;
}

static int net_recv(net_ch_t *ch, void *rxbuf, u32 size, u32 timeout_ms) {
  int ret = recv(ch->sock, rxbuf, size, ch->rx_flag);
  return ret;
}

static int net_send_recv(net_ch_t *ch, void *txbuf, u32 tx_size, void *rxbuf, u32 rx_size) {
  return 0;
}

typedef struct {
  char remote_ip[MAX_IP_SIZE];
  char resp[MAX_RESP_SIZE];
} net_broadcast_t;

static int net_broadcast(const char      *remote_ip,
                         u16              remote_port,
                         const u8        *txbuf,
                         u32              size,
                         net_broadcast_t *resp,
                         u32              timeout_ms) {
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif // !NET_H
