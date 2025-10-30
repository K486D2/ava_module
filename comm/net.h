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
typedef SOCKET sockfd_t;
#define CLOSE_SOCKET closesocket
#endif

#include <stdio.h>
#include <string.h>

#include "../ds/list.h"
#include "../ds/mp.h"
#include "../util/errdef.h"
#include "../util/timeops.h"

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
        u32 size;        // 数据长度
} net_log_meta_t;
#pragma pack(pop)

struct net_ch;
typedef void (*net_async_cb_f)(struct net_ch *ch, void *buf, int ret);
typedef void (*net_log_cb_f)(FILE *fd, const net_log_meta_t *log_meta, const void *log_data);

typedef struct net_ch {
        list_head_t ch_node;
        net_mode_e  e_mode;
        char        remote_ip[MAX_IP_SIZE], local_ip[MAX_IP_SIZE];
        u16         remote_port, local_port;

        sockfd_t     fd;
        net_log_cb_f f_log_cb;

        net_op_e       e_op;
        net_async_cb_f f_send_cb, f_recv_cb;
} net_ch_t;

typedef struct {
        net_ch_t      *ch;
        void          *buf;
        usz            size;
        net_async_cb_f f_cb;
        ATOMIC(bool) processed;
#ifdef _WIN32
        OVERLAPPED ov;
#endif
} net_async_req_t;

typedef struct {
        net_type_e e_type;
        mp_t      *mp;
        u32        ring_len;
} net_cfg_t;

typedef struct {
        list_head_t ch_root;
#ifdef __linux__
        struct io_uring ring;
#elif defined(_WIN32)
        HANDLE iocp;
#endif
} net_lo_t;

typedef struct net {
        net_cfg_t cfg;
        net_lo_t  lo;
} net_t;

typedef struct {
        char remote_ip[MAX_IP_SIZE];
        char resp[MAX_RESP_BUF_SIZE];
} net_broadcast_t;

HAPI int  net_init(net_t *net, net_cfg_t net_cfg);
HAPI void net_destory(net_t *net);
HAPI int  net_set_nonblock(net_ch_t *ch);
HAPI int  net_add_ch(net_t *net, net_ch_t *ch);

HAPI int net_sync_send(net_ch_t *ch, void *tx_buf, usz size);
HAPI int net_sync_recv_yield(net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us);
HAPI int net_sync_recv_spin(net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us);

HAPI int net_async_send(net_t *net, net_ch_t *ch, void *tx_buf, usz size);
HAPI int net_async_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us);
HAPI int net_poll(net_t *net);

HAPI int net_send(net_t *net, net_ch_t *ch, void *tx_buf, usz size);
HAPI int net_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us);
HAPI int net_send_recv(net_t *net, net_ch_t *ch, void *tx_buf, usz size, void *rx_buf, usz cap, u32 timeout_us);

HAPI int net_broadcast(
    net_t *net, const char *remote_ip, u16 remote_port, const void *tx_buf, u32 size, net_broadcast_t *resp, u32 timeout_us);

HAPI int
net_init(net_t *net, net_cfg_t net_cfg)
{
        DECL_PTRS(net, cfg, lo);

        *cfg = net_cfg;

        list_init(&lo->ch_root);

        int ret = 0;
#ifdef __linux__
        ret = io_uring_queue_init(cfg->ring_len, &lo->ring, 0);
#elif defined(_WIN32)
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        lo->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
#endif
        return ret;
}

HAPI void
net_destory(net_t *net)
{
        DECL_PTRS(net, cfg, lo);

        list_head_t *node;
        LIST_FOR_EACH(node, &lo->ch_root)
        {
                net_ch_t *ch = CONTAINER_OF(node, net_ch_t, ch_node);
                CLOSE_SOCKET(ch->fd);
        }

#ifdef __linux__
        io_uring_queue_exit(&lo->ring);
#elif defined(_WIN32)
        WSACleanup();
#endif
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
#ifdef __linux__
                        ch->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#elif defined(_WIN32)
                        ch->fd = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
#endif
                        break;
                }
                case NET_TYPE_TCP: {
#ifdef __linux__
                        ch->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#elif defined(_WIN32)
                        ch->fd = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
#endif
                        break;
                }
                default:
                        return -MEINVAL;
        }

        struct sockaddr_in remote_addr;
        memset(&remote_addr, 0, sizeof(remote_addr));
        remote_addr.sin_family      = AF_INET;
        remote_addr.sin_port        = htons(ch->remote_port);
        remote_addr.sin_addr.s_addr = inet_addr(ch->remote_ip);

        int ret;
        if (strlen(ch->local_ip) != 0 && ch->local_port != 0) {
                struct sockaddr_in local_addr;
                memset(&local_addr, 0, sizeof(local_addr));
                local_addr.sin_family      = AF_INET;
                local_addr.sin_port        = htons(ch->local_port);
                local_addr.sin_addr.s_addr = inet_addr(ch->local_ip);

                ret = bind(ch->fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
                if (ret < 0)
                        goto cleanup;
        }

        ret = connect(ch->fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        if (ret < 0)
                goto cleanup;

        list_add(&ch->ch_node, &lo->ch_root);

#ifdef _WIN32
        CreateIoCompletionPort((HANDLE)ch->fd, lo->iocp, (ULONG_PTR)ch, 0);
#endif

        return 0;

cleanup:
        CLOSE_SOCKET(ch->fd);
        return ret;
}

HAPI int
net_sync_send(net_ch_t *ch, void *tx_buf, usz size)
{
#ifdef __linux__
        return send(ch->fd, tx_buf, size, 0);
#elif defined(_WIN32)
        return send(ch->fd, (const char *)tx_buf, size, 0);
#endif
}

HAPI int
net_sync_recv_yield(net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        struct timeval tv;
        tv.tv_sec  = US2S(timeout_us);
        tv.tv_usec = (timeout_us % 1000000);
#ifdef __linux__
        setsockopt(ch->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        return recv(ch->fd, rx_buf, cap, 0);
#elif defined(_WIN32)
        setsockopt(ch->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        return recv(ch->fd, (char *)rx_buf, cap, 0);
#endif
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
net_async_send(net_t *net, net_ch_t *ch, void *tx_buf, usz size)
{
        DECL_PTRS(net, cfg, lo);

#ifdef __linux__
        struct io_uring_sqe *send_sqe = io_uring_get_sqe(&lo->ring);
        if (!send_sqe)
                return -1;

        net_async_req_t *req = (net_async_req_t *)mp_calloc(cfg->mp, sizeof(net_async_req_t));
        if (!req)
                return -MEALLOC;

        req->ch   = ch;
        req->buf  = tx_buf;
        req->size = size;
        req->f_cb = ch->f_send_cb;

        io_uring_prep_send(send_sqe, ch->fd, tx_buf, size, 0);
        io_uring_sqe_set_data(send_sqe, req);

        return io_uring_submit(&lo->ring);
#elif defined(_WIN32)
        net_async_req_t *req = (net_async_req_t *)mp_calloc(cfg->mp, sizeof(net_async_req_t));
        if (!req)
                return -MEALLOC;

        req->ch   = ch;
        req->buf  = tx_buf;
        req->size = size;
        req->f_cb = ch->f_send_cb;

        WSABUF buf;
        buf.buf = (CHAR *)tx_buf;
        buf.len = (ULONG)size;

        DWORD tx_size;
        int   ret = WSASend(ch->fd, &buf, 1, &tx_size, 0, &req->ov, NULL);
        if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                mp_free(cfg->mp, req);
                return -1;
        }

        req->ov.Pointer = req;
        return tx_size;
#endif
}

HAPI int
net_async_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        DECL_PTRS(net, cfg, lo);

#ifdef __linux__
        net_async_req_t *req = (net_async_req_t *)mp_calloc(cfg->mp, sizeof(net_async_req_t));
        if (!req)
                return -MEALLOC;

        req->ch   = ch;
        req->buf  = rx_buf;
        req->size = cap;
        req->f_cb = ch->f_recv_cb;

        struct io_uring_sqe *recv_sqe = io_uring_get_sqe(&lo->ring);
        if (!recv_sqe)
                return -1;

        io_uring_prep_recv(recv_sqe, ch->fd, rx_buf, cap, 0);
        io_uring_sqe_set_data(recv_sqe, req);
        io_uring_sqe_set_flags(recv_sqe, IOSQE_IO_LINK);

        struct io_uring_sqe     *timeout_sqe = io_uring_get_sqe(&lo->ring);
        struct __kernel_timespec ts;
        ts.tv_sec  = US2S(timeout_us);
        ts.tv_nsec = US2NS(timeout_us % 1000000);
        io_uring_prep_link_timeout(timeout_sqe, &ts, 0);
        io_uring_sqe_set_data(timeout_sqe, req);

        return io_uring_submit(&lo->ring);
#elif defined(_WIN32)
        net_async_req_t *req = (net_async_req_t *)mp_calloc(cfg->mp, sizeof(net_async_req_t));
        if (!req)
                return -MEALLOC;

        req->ch   = ch;
        req->buf  = rx_buf;
        req->size = cap;
        req->f_cb = ch->f_recv_cb;

        WSABUF buf;
        buf.buf = (CHAR *)rx_buf;
        buf.len = (ULONG)cap;

        DWORD flags = 0;
        DWORD rx_size;
        int   ret = WSARecv(ch->fd, &buf, 1, &rx_size, &flags, &req->ov, NULL);
        if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                mp_free(cfg->mp, req);
                return -1;
        }

        req->ov.Pointer = req;
        return rx_size;
#endif
}

HAPI int
net_poll(net_t *net)
{
        DECL_PTRS(net, cfg, lo);

#ifdef __linux__
        struct io_uring_cqe *cqe;
        while (io_uring_peek_cqe(&lo->ring, &cqe) == 0) {
                net_async_req_t *req = (net_async_req_t *)io_uring_cqe_get_data(cqe);
                if (!req) {
                        io_uring_cqe_seen(&lo->ring, cqe);
                        continue;
                }

                if (atomic_exchange(&req->processed, 1) == 0) {
                        req->f_cb(req->ch, req->buf, cqe->res);
                        mp_free(cfg->mp, req);
                }

                io_uring_cqe_seen(&lo->ring, cqe);
        }
        return 0;
#elif defined(_WIN32)
        DWORD       size;
        ULONG_PTR   key;
        OVERLAPPED *ov = NULL;

        for (;;) {
                BOOL ok = GetQueuedCompletionStatus(lo->iocp, &size, &key, &ov, INFINITE);
                if (!ok && ov == NULL)
                        break;

                net_async_req_t *req = (net_async_req_t *)ov->Pointer;
                if (!req)
                        continue;

                if (atomic_exchange(&req->processed, 1) == 0) {
                        req->f_cb(req->ch, req->buf, ok ? (int)size : -1);
                        mp_free(cfg->mp, req);
                }
        }
        return 0;
#endif
}

HAPI int
net_send(net_t *net, net_ch_t *ch, void *tx_buf, usz size)
{
        ch->e_op = NET_OP_SEND;
        switch (ch->e_mode) {
                case NET_MODE_SYNC_SPIN:
                case NET_MODE_SYNC_YIELD:
                        return net_sync_send(ch, tx_buf, size);
                case NET_MODE_ASYNC:
                        return net_async_send(net, ch, tx_buf, size);
                default:
                        return -MEINVAL;
        }
}

HAPI int
net_recv(net_t *net, net_ch_t *ch, void *rx_buf, usz cap, u32 timeout_us)
{
        ch->e_op = NET_OP_RECV;
        switch (ch->e_mode) {
                case NET_MODE_SYNC_YIELD:
                        return net_sync_recv_yield(ch, rx_buf, cap, timeout_us);
                case NET_MODE_SYNC_SPIN:
                        return net_sync_recv_spin(ch, rx_buf, cap, timeout_us);
                case NET_MODE_ASYNC:
                        return net_async_recv(net, ch, rx_buf, cap, timeout_us);
                default:
                        return -MEINVAL;
        }
}

HAPI int
net_send_recv(net_t *net, net_ch_t *ch, void *tx_buf, usz size, void *rx_buf, usz cap, u32 timeout_us)
{
        const int ret = net_send(net, ch, tx_buf, size);
        if (ret <= 0)
                return ret;

        return net_recv(net, ch, rx_buf, cap, timeout_us);
}

HAPI int
net_broadcast(
    net_t *net, const char *remote_ip, u16 remote_port, const void *tx_buf, u32 size, net_broadcast_t *resp, u32 timeout_us)
{
        net_ch_t ch = {
            .e_mode      = NET_MODE_SYNC_YIELD,
            .remote_port = remote_port,
        };
        strncpy(ch.remote_ip, remote_ip, MAX_IP_SIZE - 1);

        int ret = net_add_ch(net, &ch);
        if (ret < 0)
                goto cleanup;

        ret = net_send(net, &ch, (void *)tx_buf, size);
        if (ret <= 0)
                goto cleanup;

        ret = net_recv(net, &ch, resp->resp, MAX_RESP_BUF_SIZE, timeout_us);
        if (ret <= 0)
                goto cleanup;

        strncpy(resp->remote_ip, remote_ip, MAX_IP_SIZE - 1);
        resp->resp[ret] = '\0';

        return 0;

cleanup:
        CLOSE_SOCKET(ch.fd);
        return ret;
}

#endif // !NET_H
