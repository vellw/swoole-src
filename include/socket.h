/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#pragma once

#include "swoole.h"
#include "coroutine.h"
#include "connection.h"
#include "socks5.h"

#include <string>

#define SW_DEFAULT_SOCKET_CONNECT_TIMEOUT    1
#define SW_DEFAULT_SOCKET_READ_TIMEOUT      -1
#define SW_DEFAULT_SOCKET_WRITE_TIMEOUT     -1

namespace swoole
{
enum swTimeout_type
{
    SW_TIMEOUT_CONNECT      =  1u << 1,
    SW_TIMEOUT_READ         =  1u << 2,
    SW_TIMEOUT_WRITE        =  1u << 3,
    SW_TIMEOUT_RDWR         =  SW_TIMEOUT_READ | SW_TIMEOUT_WRITE,
    SW_TIMEOUT_ALL          =  0xff,
};
class Socket
{
public:
    static double default_connect_timeout;
    static double default_read_timeout;
    static double default_write_timeout;

    swConnection *socket = nullptr;
    enum swSocket_type type;
    int sock_domain = 0;
    int sock_type = 0;
    int sock_protocol = 0;
    int backlog = 0;
    int errCode = 0;
    const char *errMsg = "";

    bool open_length_check = false;
    bool open_eof_check = false;
    bool http2 = false;

    swProtocol protocol = {0};
    swString *read_buffer = nullptr;
    swString *write_buffer = nullptr;
    swSocketAddress bind_address_info = {{}, 0};

    struct _swSocks5 *socks5_proxy = nullptr;
    struct _http_proxy* http_proxy = nullptr;

#ifdef SW_USE_OPENSSL
    bool open_ssl = false;
    swSSL_option ssl_option = {0};
#endif

    Socket(int domain = AF_INET, int type = SOCK_STREAM, int protocol = IPPROTO_IP);
    Socket(enum swSocket_type type = SW_SOCK_TCP);
    Socket(int _fd, enum swSocket_type _type);
    ~Socket();
    bool connect(std::string host, int port, int flags = 0);
    bool connect(const struct sockaddr *addr, socklen_t addrlen);
    bool shutdown(int how = SHUT_RDWR);
    bool close();
    bool is_connect();
    bool check_liveness();
    ssize_t peek(void *__buf, size_t __n);
    ssize_t recv(void *__buf, size_t __n);
    ssize_t send(const void *__buf, size_t __n);
    ssize_t read(void *__buf, size_t __n);
    ssize_t write(const void *__buf, size_t __n);
    ssize_t recvmsg(struct msghdr *msg, int flags);
    ssize_t sendmsg(const struct msghdr *msg, int flags);
    ssize_t recv_all(void *__buf, size_t __n);
    ssize_t send_all(const void *__buf, size_t __n);
    ssize_t recv_packet(double timeout = 0);
    Socket* accept();
    bool bind(std::string address, int port = 0);
    bool listen(int backlog = 0);
    bool sendfile(char *filename, off_t offset, size_t length);
    ssize_t sendto(char *address, int port, char *data, int len);
    ssize_t recvfrom(void *__buf, size_t __n);
    ssize_t recvfrom(void *__buf, size_t __n, struct sockaddr *_addr, socklen_t *_socklen);
#ifdef SW_USE_OPENSSL
    bool ssl_handshake();
    int ssl_verify(bool allow_self_signed);
    bool ssl_accept();
#endif

    static inline enum swSocket_type get_type(int domain, int type, int protocol = 0)
    {
        switch (domain)
        {
        case AF_INET:
            return type == SOCK_STREAM ? SW_SOCK_TCP : SW_SOCK_UDP;
        case AF_INET6:
            return type == SOCK_STREAM ? SW_SOCK_TCP6 : SW_SOCK_UDP6;
        case AF_UNIX:
            return type == SOCK_STREAM ? SW_SOCK_UNIX_STREAM : SW_SOCK_UNIX_DGRAM;
        default:
            return SW_SOCK_TCP;
        }
    }

    inline int get_fd()
    {
        return socket ? socket->fd : -1;
    }

    inline bool has_bound()
    {
        return read_co || write_co;
    }

    inline Coroutine* get_bound_co(enum swEvent_type event)
    {
        if (likely(event & SW_EVENT_READ))
        {
            return read_co;
        }
        if (event & SW_EVENT_WRITE)
        {
            return write_co;
        }
        return nullptr;
    }

    inline long get_bound_cid(enum swEvent_type event = SW_EVENT_RDWR)
    {
        Coroutine *co = get_bound_co(event);
        return co ? co->get_cid() : 0;
    }

    inline void set_err(int e)
    {
        errCode = errno = e;
        errMsg = e ? swoole_strerror(e) : "";
    }

    inline void set_err(int e, const char *s)
    {
        errCode = errno = e;
        errMsg = s;
    }

    /* set connect read write timeout */
    inline void set_timeout(double timeout, int type = SW_TIMEOUT_ALL)
    {
        if (timeout == 0)
        {
            return;
        }
        if (type & SW_TIMEOUT_CONNECT)
        {
            connect_timeout = timeout;
        }
        if (type & SW_TIMEOUT_READ)
        {
            read_timeout = timeout;
        }
        if (type & SW_TIMEOUT_WRITE)
        {
            write_timeout = timeout;
        }
    }

    inline void set_timeout(struct timeval *timeout, int type = SW_TIMEOUT_ALL)
    {
        set_timeout((double) timeout->tv_sec + ((double) timeout->tv_usec / 1000 / 1000), type);
    }

    inline double get_timeout(enum swTimeout_type type = SW_TIMEOUT_ALL)
    {
        if (type & SW_TIMEOUT_CONNECT)
        {
            return connect_timeout;
        }
        else if (type & SW_TIMEOUT_READ)
        {
            return read_timeout;
        }
        else if (type & SW_TIMEOUT_WRITE)
        {
            return write_timeout;
        }
        return -1;
    }

    inline bool set_option(int level, int optname, int optval)
    {
        if (setsockopt(socket->fd, level, optname, &optval, sizeof(optval)) != 0)
        {
            swSysError("setsockopt(%d, %d, %d, %d) failed.", socket->fd, level, optname, optval);
            return false;
        }
        return true;
    }

    inline swString* get_read_buffer()
    {
        if (unlikely(!read_buffer))
        {
            read_buffer = swString_new(SW_BUFFER_SIZE_STD);
        }
        return read_buffer;
    }

    inline swString* get_write_buffer()
    {
        if (unlikely(!write_buffer))
        {
            write_buffer = swString_new(SW_BUFFER_SIZE_STD);
        }
        return write_buffer;
    }

private:
    swReactor *reactor = nullptr;
    Coroutine *read_co = nullptr;
    Coroutine *write_co = nullptr;
#ifdef SW_USE_OPENSSL
    enum swEvent_type want_event = SW_EVENT_NULL;
#endif

    std::string host;
    int port = 0;
    std::string bind_address;
    int bind_port = 0;

    double connect_timeout = default_connect_timeout;
    double read_timeout = default_read_timeout;
    double write_timeout = default_write_timeout;
    swTimer_node *read_timer = nullptr;
    swTimer_node *write_timer = nullptr;

    bool shutdown_read = false;
    bool shutdown_write = false;
#ifdef SW_USE_OPENSSL
    std::string ssl_host_name;
    SSL_CTX *ssl_context = nullptr;
#endif

    static void timer_callback(swTimer *timer, swTimer_node *tnode);
    static int readable_event_callback(swReactor *reactor, swEvent *event);
    static int writable_event_callback(swReactor *reactor, swEvent *event);
    static int error_event_callback(swReactor *reactor, swEvent *event);

    Socket(int _fd, Socket *socket);
    inline void init_sock_type(enum swSocket_type _type);
    inline bool init_sock();
    inline void init_sock(int fd);
    inline void init_options()
    {
        if (type == SW_SOCK_TCP || type == SW_SOCK_TCP6)
        {
            set_option(IPPROTO_TCP, TCP_NODELAY, 1);
        }
        protocol.package_length_type = 'N';
        protocol.package_length_size = 4;
        protocol.package_body_offset = 0;
        protocol.package_max_length = SW_BUFFER_INPUT_SIZE;
    }

    bool add_event(const enum swEvent_type event);
    bool wait_event(const enum swEvent_type event, const void **__buf = nullptr, size_t __n = 0);

    inline bool is_available(enum swEvent_type event)
    {
        if (event != SW_EVENT_NULL)
        {
            long cid = get_bound_cid(event);
            if (unlikely(cid))
            {
                swoole_error_log(
                    SW_LOG_ERROR, SW_ERROR_CO_HAS_BEEN_BOUND,
                    "Socket#%d has already been bound to another coroutine#%ld, "
                    "%s of the same socket in multiple coroutines at the same time is not allowed.\n",
                    socket->fd, cid, (event == SW_EVENT_READ ? "reading" : (event == SW_EVENT_WRITE ? "writing" : "reading or writing"))
                );
                exit(255);
            }
        }
        if (unlikely(socket->closed))
        {
            set_err(ECONNRESET);
            return false;
        }
        return true;
    }

    // TODO: move to client.cc
    bool socks5_handshake();
    bool http_proxy_handshake();

    class timer_controller
    {
    public:
        timer_controller(swTimer_node **timer_pp, double timeout, void *data, swTimerCallback callback) :
            timer_pp(timer_pp), timeout(timeout), data(data), callback(callback)
        {
        }
        bool start()
        {
            if (timeout != 0 && !*timer_pp)
            {
                enabled = true;
                if (timeout > 0)
                {
                    *timer_pp = swTimer_add(&SwooleG.timer, (long) (timeout * 1000), 0, data, callback);
                    return *timer_pp != nullptr;
                }
                else // if (timeout < 0)
                {
                    *timer_pp = (swTimer_node *) -1;
                }
            }
            return true;
        }
        ~timer_controller()
        {
            if (enabled && *timer_pp)
            {
                if (*timer_pp != (swTimer_node *) -1)
                {
                    swTimer_del(&SwooleG.timer, *timer_pp);
                }
                *timer_pp = nullptr;
            }
        }
    private:
        bool enabled = false;
        swTimer_node** timer_pp;
        double timeout;
        void *data;
        swTimerCallback callback;
    };

public:
    class timeout_setter
    {
    public:
        timeout_setter(Socket *socket, double timeout, const enum swTimeout_type type) :
            socket(socket), timeout(timeout), type(type)
        {
            SW_ASSERT(type == SW_TIMEOUT_CONNECT || type == SW_TIMEOUT_READ || type == SW_TIMEOUT_WRITE);
            original_timeout = socket->get_timeout(type);
            if (timeout == 0)
            {
                this->timeout = original_timeout;
            }
            else if (timeout != original_timeout)
            {
                socket->set_timeout(timeout, type);
            }
        }
        ~timeout_setter()
        {
            if (timeout != original_timeout)
            {
                socket->set_timeout(original_timeout, type);
            }
        }
    protected:
        Socket *socket;
        double timeout;
        enum swTimeout_type type;
        double original_timeout;
    };

    class timeout_controller: public timeout_setter
    {
    public:
        timeout_controller(Socket *socket, double timeout, const enum swTimeout_type type) :
                timeout_setter(socket, timeout, type)
        {
            if (timeout > 0)
            {
                startup_time = swoole_microtime();
            }
        }
        inline bool has_timedout()
        {
            if (timeout > 0)
            {
                double used_time = swoole_microtime() - startup_time;
                if (timeout - used_time < SW_TIMER_MIN_SEC)
                {
                    return true;
                }
                socket->set_timeout(timeout - used_time, type);
            }
            return false;
        }
    protected:
        double startup_time;
    };
};
};
