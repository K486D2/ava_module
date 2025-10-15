#ifndef NET_H
#define NET_H

#ifdef __linux__
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
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

#include "ds/list.h"
#include "util/util.h"

#define MAX_IP_SIZE       16
#define MAX_RESP_BUF_SIZE 1024
#define MAX_IP_NUM        255

typedef enum {
        NET_TYPE_NULL,
        NET_TYPE_UDP,
        NET_TYPE_TCP,
} net_type_e;

typedef enum {
        NET_RECV_YIELD,
        NET_RECV_SPIN,
} net_recv_e;

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
        net_recv_e   recv_mode;
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
        net_cfg_t *cfg = &net->cfg;                                                                \
        net_lo_t  *lo  = &net->lo;                                                                 \
        ARG_UNUSED(net);                                                                           \
        ARG_UNUSED(cfg);                                                                           \
        ARG_UNUSED(lo);

#define DECL_NET_PTR_RENAME(net, name)                                                             \
        net_t *name = (net);                                                                       \
        ARG_UNUSED(name);

static inline int net_init(net_t *net, net_cfg_t net_cfg) {
        DECL_PTRS(net, cfg, lo);

        *cfg = net_cfg;

        list_init(&lo->ch.node);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        return 0;
}

static inline int net_set_nonblock(net_ch_t *ch) {
#ifdef __linux__
        int flags = fcntl(ch->sock, F_GETFL, 0);
        if (flags < 0)
                return flags;

        flags |= O_NONBLOCK;
        int ret = fcntl(ch->sock, F_SETFL, flags);
        return ret;
#elif defined(_WIN32)
        unsigned long mode = 1;
        int           ret  = ioctlsocket(ch->sock, FIONBIO, &mode);
        return ret;
#endif
}

static inline int net_add_ch(net_t *net, net_ch_t *ch) {
        DECL_PTRS(net, cfg, lo);

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

        int ret;
        ret = net_set_nonblock(ch);
        if (ret < 0)
                return ret;

        struct sockaddr_in remote_addr = {0};
        remote_addr.sin_family         = AF_INET;
        remote_addr.sin_port           = htons(ch->remote_port);
        remote_addr.sin_addr.s_addr    = inet_addr(ch->remote_ip);

        if (strlen(ch->local_ip) != 0 && ch->local_port != 0) {
                struct sockaddr_in local_addr = {0};
                local_addr.sin_family         = AF_INET;
                local_addr.sin_port           = htons(ch->local_port);
                local_addr.sin_addr.s_addr    = inet_addr(ch->local_ip);

                ret = bind(ch->sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
                if (ret < 0)
                        goto cleanup;
        }

        ret = connect(ch->sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        if (ret < 0)
                goto cleanup;

        list_add(&ch->node, &lo->ch.node);
        return 0;

cleanup:
        CLOSE_SOCKET(ch->sock);
        return ret;
}

static inline int net_send(net_ch_t *ch, void *tx_buf, u32 size) {
#ifdef __linux__
        int ret = send(ch->sock, tx_buf, size, 0);
#elif defined(_WIN32)
        int ret = send(ch->sock, (const char *)tx_buf, size, 0);
#endif
        return ret;
}

static inline int net_recv_yield(net_ch_t *ch, void *rx_buf, u32 size, u32 timeout_ms) {
        struct timeval tv = {
            .tv_sec  = (i32)timeout_ms / 1000,                      // 秒
            .tv_usec = (i32)(timeout_ms - tv.tv_sec * 1000) * 1000, // 微秒
        };

#ifdef __linux__
        setsockopt(ch->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        int ret = recv(ch->sock, rx_buf, size, 0);
#elif defined(_WIN32)
        setsockopt(ch->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        int ret = recv(ch->sock, (char *)rx_buf, size, 0);
#endif
        return ret;
}

static inline int net_recv_spin(net_ch_t *ch, void *rx_buf, u32 size, u32 timeout_ms) {
        u64 start_ts_ns = get_mono_ts_ns();
        u64 curr_ts_ns  = 0;
        while (curr_ts_ns < start_ts_ns + (u64)(timeout_ms * 1e6)) {
#ifdef __linux__
                int ret = recv(ch->sock, rx_buf, size, 0);
#elif defined(_WIN32)
                int ret = recv(ch->sock, (char *)rx_buf, size, 0);
#endif
                if (ret > 0)
                        return ret;
                curr_ts_ns = get_mono_ts_ns();
        }
        return -METIMEOUT;
}

static inline int net_recv(net_ch_t *ch, void *rx_buf, u32 size, u32 timeout_ms) {
        int ret;
        switch (ch->recv_mode) {
        case NET_RECV_YIELD:
                ret = net_recv_yield(ch, rx_buf, size, timeout_ms);
                break;
        case NET_RECV_SPIN:
                ret = net_recv_spin(ch, rx_buf, size, timeout_ms);
                break;
        default:
                return -MEINVAL;
        }
        return ret;
}

static inline int
net_send_recv(net_ch_t *ch, void *tx_buf, u32 tx_size, void *rx_buf, u32 rx_size, u32 timeout_ms) {
        int ret;

        ret = net_send(ch, tx_buf, tx_size);
        if (ret <= 0)
                return ret;

        ret = net_recv(ch, rx_buf, rx_size, timeout_ms);
        return ret;
}

typedef struct {
        char remote_ip[MAX_IP_SIZE];
        char resp[MAX_RESP_BUF_SIZE];
} net_broadcast_t;

// static inline int net_broadcast(const char      *remote_ip,
//                                 u16              remote_port,
//                                 const u8        *tx_buf,
//                                 u32              size,
//                                 net_broadcast_t *resp,
//                                 u32              timeout_ms) {
//   return 0;
// }

#endif // !NET_H
