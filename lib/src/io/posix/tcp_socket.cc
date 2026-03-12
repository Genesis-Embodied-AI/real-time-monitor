#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include "rtm/io/posix/tcp_socket.h"

namespace rtm
{
    static void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }


    // --- TcpSocket (client) ---

    TcpSocket::TcpSocket(std::string_view host, uint16_t port)
        : host_{host}
        , port_{port}
    {
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    TcpSocket::TcpSocket(os_socket fd, access::Mode modes)
    {
        fd_ = fd;
        modes_ = modes;
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    TcpSocket::~TcpSocket()
    {
        if (is_open())
        {
            close();
        }
    }

    std::error_code TcpSocket::do_open(access::Mode mode)
    {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        auto port_str = std::to_string(port_);
        struct addrinfo* res = nullptr;

        int rc = ::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res);
        if (rc != 0)
        {
            return from_errno(EADDRNOTAVAIL);
        }

        for (auto* rp = res; rp != nullptr; rp = rp->ai_next)
        {
            fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd_ == -1)
            {
                continue;
            }

            if (::connect(fd_, rp->ai_addr, rp->ai_addrlen) == 0)
            {
                break;
            }

            ::close(fd_);
            fd_ = -1;
        }

        ::freeaddrinfo(res);

        if (fd_ == -1)
        {
            return from_errno(ECONNREFUSED);
        }

        int enable = 1;
        ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

        if (mode & access::Mode::NON_BLOCKING)
        {
            set_nonblocking(fd_);
        }

        return {};
    }


    // --- TcpListener ---

    TcpListener::TcpListener(std::string_view bind_address, uint16_t port)
        : bind_address_{bind_address}
        , port_{port}
    {
    }

    TcpListener::~TcpListener()
    {
        if (fd_ != -1)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    std::error_code TcpListener::listen(int backlog)
    {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        auto port_str = std::to_string(port_);
        struct addrinfo* res = nullptr;
        int rc = 0;

        if (bind_address_.empty())
        {
            rc = ::getaddrinfo(nullptr, port_str.c_str(), &hints, &res);
        }
        else
        {
            rc = ::getaddrinfo(bind_address_.c_str(), port_str.c_str(), &hints, &res);
        }
        if (rc != 0)
        {
            return from_errno(EADDRNOTAVAIL);
        }

        for (auto* rp = res; rp != nullptr; rp = rp->ai_next)
        {
            fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd_ == -1)
            {
                continue;
            }

            int reuse = 1;
            ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

            if (::bind(fd_, rp->ai_addr, rp->ai_addrlen) == 0)
            {
                break;
            }

            ::close(fd_);
            fd_ = -1;
        }

        ::freeaddrinfo(res);

        if (fd_ == -1)
        {
            return from_errno(EADDRINUSE);
        }

        set_nonblocking(fd_);

        if (::listen(fd_, backlog) == -1)
        {
            auto err = from_errno(errno);
            ::close(fd_);
            fd_ = -1;
            return err;
        }

        return {};
    }

    std::unique_ptr<AbstractSocket> TcpListener::accept(access::Mode mode)
    {
        int socket_fd = ::accept(fd_, nullptr, nullptr);
        if (socket_fd == -1)
        {
            return nullptr;
        }

        int enable = 1;
        ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

        if (mode & access::Mode::NON_BLOCKING)
        {
            set_nonblocking(socket_fd);
        }

        return std::unique_ptr<AbstractSocket>(
            new TcpSocket(socket_fd, mode | access::Mode::READ_WRITE)
        );
    }
}
