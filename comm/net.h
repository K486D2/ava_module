#ifndef NET_H
#define NET_H

#ifdef __linux__
#include <arpa/inet.h>
#include <fcntl.h>
#include <liburing.h>
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
#include <string.h>

#include "ds/list.h"
#include "util/errdef.h"
#include "util/timeops.h"

#define MAX_IP_SIZE       16
#define MAX_RESP_BUF_SIZE 1024
#define MAX_IP_NUM        255

typedef enum {
        NET_TYPE_NULL,
        NET_TYPE_UDP,
        NET_TYPE_TCP,
} net_type_e;

typedef enum {
        NET_MODE_SYNC_YIELD,
        NET_MODE_SYNC_SPIN,
        NET_MODE_ASYNC,
} net_mode_e;

typedef enum {
        NET_OP_NULL,
        NET_OP_SEND,
        NET_OP_RECV,
} net_op_e;

#pragma pack(push, 1)
typedef struct {
        u64 ts;          // 时间戳
        u8  type;        // 事件类型
        u32 remote_ip;   // 设备IP
        u16 remote_port; // 设备端口
        u32 len;         // 数据长度
} net_log_meta_t;
#pragma pack(pop)

typedef enum {
        SEND,
        RECV,
        SEND_RECV,
} net_flag_e;

typedef void (*net_log_cb_f)(FILE *file, const net_log_meta_t *log_meta, const void *log_data);

struct net_ch;
typedef void (*net_async_cb_f)(struct net_ch *ch, void *buf, int ret);

typedef struct net_ch {
        list_head_t  node;
        net_mode_e   e_mode;
        char         remote_ip[MAX_IP_SIZE], local_ip[MAX_IP_SIZE];
        u16          remote_port, local_port;
        socket_t     fd;
        net_log_cb_f f_log_cb;

        // 异步
        net_op_e       e_status;
        net_async_cb_f f_send_cb, f_recv_cb;
        void          *tx_buf, *rx_buf;
        usz            tx_nbytes, rx_buf_cap;
} net_ch_t;

typedef struct {
        net_type_e e_type;
} net_cfg_t;

typedef struct {
        net_ch_t ch;
#ifdef __linux__
        struct io_uring ring;
#elif defined(_WIN32)
#endif
        net_log_meta_t log_meta;
} net_lo_t;

typedef struct net {
        net_cfg_t cfg;
        net_lo_t  lo;
} net_t;

HAPI int
net_init(net_t *net, net_cfg_t net_cfg)
{
        DECL_PTRS(net, cfg, lo);

        *cfg = net_cfg;

        list_init(&lo->ch.node);

#ifdef __linux__
        int ret = io_uring_queue_init(64, &lo->ring, 0);
        if (ret < 0)
                return ret;
#elif defined(_WIN32)
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        return 0;
}

HAPI int
net_set_nonblock(net_ch_t *ch)
{
#ifdef __linux__
        int flags = fcntl(ch->fd, F_GETFL, 0);
        if (flags < 0)
                return flags;
        flags   |= O_NONBLOCK;
        int ret  = fcntl(ch->fd, F_SETFL, flags);
        return ret;
#elif defined(_WIN32)
        unsigned long mode = 1;
        int           ret  = ioctlsocket(ch->fd, FIONBIO, &mode);
        return ret;
#endif
}

HAPI int
net_add_ch(net_t *net, net_ch_t *ch)
{
        DECL_PTRS(net, cfg, lo);

        switch (cfg->e_type) {
                case NET_TYPE_UDP: {
                        ch->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                        break;
                }
                case NET_TYPE_TCP: {
                        ch->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                        break;
                }
                default:
                        return -MEINVAL;
        }

        struct sockaddr_in remote_addr = {0};
        remote_addr.sin_family         = AF_INET;
        remote_addr.sin_port           = htons(ch->remote_port);
        remote_addr.sin_addr.s_addr    = inet_addr(ch->remote_ip);

        int ret;
        if (strlen(ch->local_ip) != 0 && ch->local_port != 0) {
                struct sockaddr_in local_addr = {0};
                local_addr.sin_family         = AF_INET;
                local_addr.sin_port           = htons(ch->local_port);
                local_addr.sin_addr.s_addr    = inet_addr(ch->local_ip);

                ret = bind(ch->fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
                if (ret < 0)
                        goto cleanup;
        }

        ret = connect(ch->fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        if (ret < 0)
                goto cleanup;

        list_add(&ch->node, &lo->ch.node);
        return 0;

cleanup:
        CLOSE_SOCKET(ch->fd);
        return ret;
}

HAPI int
net_sync_send(net_ch_t *ch, void *tx_buf, usz nbytes)
{
#ifdef __linux__
        int ret = send(ch->fd, tx_buf, nbytes, 0);
#elif defined(_WIN32)
        int ret = send(ch->fd, (const char *)tx_buf, nbytes, 0);
#endif
        return ret;
}

HAPI int
net_sync_recv_yield(net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        struct timeval tv = {
            .tv_sec  = (int)US2S(timeout_us),               // 秒
            .tv_usec = (int)(timeout_us - S2US(tv.tv_sec)), // 微秒
        };

#ifdef __linux__
        setsockopt(ch->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        int ret = recv(ch->fd, rx_buf, cap, 0);
#elif defined(_WIN32)
        setsockopt(ch->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        int ret = recv(ch->fd, (char *)rx_buf, cap, 0);
#endif
        return ret;
}

HAPI int
net_sync_recv_spin(net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        u64 begin_ns = get_mono_ts_ns();
        u64 curr_ns  = 0;
        while (curr_ns < begin_ns + US2NS(timeout_us)) {
#ifdef __linux__
                int ret = recv(ch->fd, rx_buf, cap, MSG_DONTWAIT);
#elif defined(_WIN32)
                int ret = recv(ch->fd, (char *)rx_buf, cap, 0);
#endif
                if (ret > 0)
                        return ret;
                curr_ns = get_mono_ts_ns();
        }
        return -METIMEOUT;
}

HAPI int
net_async_send(net_t *net, net_ch_t *ch, void *tx_buf, usz nbytes)
{
        DECL_PTRS(net, lo);

        struct io_uring_sqe *sqe = io_uring_get_sqe(&lo->ring);
        if (!sqe)
                return -1;

        ch->tx_buf    = tx_buf;
        ch->tx_nbytes = nbytes;

        io_uring_prep_send(sqe, ch->fd, ch->tx_buf, ch->tx_nbytes, 0);
        io_uring_sqe_set_data(sqe, ch);

        int ret = io_uring_submit(&lo->ring);
        return ret;
}

HAPI int
net_async_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap)
{
        DECL_PTRS(net, lo);

        struct io_uring_sqe *sqe = io_uring_get_sqe(&lo->ring);
        if (!sqe)
                return -1;

        ch->rx_buf     = rx_buf;
        ch->rx_buf_cap = cap;

        io_uring_prep_recv(sqe, ch->fd, ch->rx_buf, ch->rx_buf_cap, 0);
        io_uring_sqe_set_data(sqe, ch);

        int ret = io_uring_submit(&lo->ring);
        return ret;
}

HAPI int
net_poll(net_t *net)
{
        DECL_PTRS(net, lo);

        struct io_uring_cqe *cqe;
        while (io_uring_peek_cqe(&lo->ring, &cqe) == 0) {
                net_ch_t *ch  = io_uring_cqe_get_data(cqe);
                int       ret = cqe->res;
                io_uring_cqe_seen(&lo->ring, cqe);

                if (!ch)
                        continue;

                if (ch->e_status == NET_OP_SEND && ch->f_send_cb)
                        ch->f_send_cb(ch, ch->tx_buf, ret);
                else if (ch->e_status == NET_OP_RECV && ch->f_recv_cb)
                        ch->f_recv_cb(ch, ch->rx_buf, ret);

                ch->e_status = NET_OP_NULL;
        }

        return 0;
}

HAPI int
net_send(net_t *net, net_ch_t *ch, void *rx_buf, usz nbytes)
{
        ch->e_status = NET_OP_SEND;

        int ret;
        switch (ch->e_mode) {
                case NET_MODE_SYNC_SPIN:
                case NET_MODE_SYNC_YIELD: {
                        ret = net_sync_send(ch, rx_buf, nbytes);
                        break;
                }
                case NET_MODE_ASYNC: {
                        ret = net_async_send(net, ch, rx_buf, nbytes);
                        break;
                }
                default:
                        return -MEINVAL;
        }
        return ret;
}

HAPI int
net_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        ch->e_status = NET_OP_RECV;

        int ret;
        switch (ch->e_mode) {
                case NET_MODE_SYNC_YIELD: {
                        ret = net_sync_recv_yield(ch, rx_buf, cap, timeout_us);
                        break;
                }
                case NET_MODE_SYNC_SPIN: {
                        ret = net_sync_recv_spin(ch, rx_buf, cap, timeout_us);
                        break;
                }
                case NET_MODE_ASYNC: {
                        ret = net_async_recv(net, ch, rx_buf, cap);
                        break;
                }
                default:
                        return -MEINVAL;
        }
        return ret;
}

HAPI int
net_send_recv(net_t *net, net_ch_t *ch, void *tx_buf, usz nbytes, void *rx_buf, usz cap, u32 timeout_us)
{
        int ret;
        ret = net_send(net, ch, tx_buf, nbytes);
        if (ret <= 0)
                return ret;

        ret = net_recv(net, ch, rx_buf, cap, timeout_us);
        return ret;
}

typedef struct {
        char remote_ip[MAX_IP_SIZE];
        char resp[MAX_RESP_BUF_SIZE];
} net_broadcast_t;

HAPI int
net_broadcast(const char *remote_ip, u16 remote_port, const void *tx_buf, u32 nbytes, net_broadcast_t *resp, u32 timeout_us)
{
        return 0;
}

#endif // !NET_H
