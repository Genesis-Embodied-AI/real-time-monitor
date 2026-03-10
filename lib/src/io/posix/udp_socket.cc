#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "rtm/io/posix/udp_socket.h"

namespace rtm
{
    UdpSocket::UdpSocket(uint16_t bind_port)
        : bind_port_{bind_port}
    {
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    UdpSocket::UdpSocket(std::string_view remote_host, uint16_t remote_port, uint16_t bind_port)
        : remote_host_{remote_host}
        , remote_port_{remote_port}
        , bind_port_{bind_port}
    {
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    UdpSocket::~UdpSocket()
    {
        if (is_open())
        {
            close();
        }
    }

    std::error_code UdpSocket::do_open(access::Mode mode)
    {
        struct addrinfo* peer_addr = nullptr;
        int family = AF_INET;

        if (not remote_host_.empty())
        {
            struct addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;

            auto port_str = std::to_string(remote_port_);
            int rc = ::getaddrinfo(remote_host_.c_str(), port_str.c_str(), &hints, &peer_addr);
            if (rc != 0)
            {
                return from_errno(EADDRNOTAVAIL);
            }

            family = peer_addr->ai_family;
            fd_ = ::socket(family, SOCK_DGRAM, 0);
            if (fd_ == -1)
            {
                ::freeaddrinfo(peer_addr);
                return from_errno(errno);
            }

            if (family == AF_INET6)
            {
                int no = 0;
                ::setsockopt(fd_, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
            }
        }
        else if (bind_port_ != 0)
        {
            // Prefer IPv6 dual-stack to accept both IPv4 and IPv6 senders
            fd_ = ::socket(AF_INET6, SOCK_DGRAM, 0);
            if (fd_ != -1)
            {
                family = AF_INET6;
                int no = 0;
                ::setsockopt(fd_, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
            }
            else
            {
                fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
                family = AF_INET;
            }

            if (fd_ == -1)
            {
                return from_errno(errno);
            }
        }
        else
        {
            return from_errno(EINVAL);
        }

        if (mode & access::Mode::NON_BLOCKING)
        {
            int flags = fcntl(fd_, F_GETFL, 0);
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }

        if (bind_port_ != 0)
        {
            int reuse = 1;
            ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

            struct sockaddr_storage addr{};
            socklen_t addr_len = 0;

            if (family == AF_INET6)
            {
                auto& a = reinterpret_cast<struct sockaddr_in6&>(addr);
                a.sin6_family = AF_INET6;
                a.sin6_addr = in6addr_any;
                a.sin6_port = htons(bind_port_);
                addr_len = sizeof(struct sockaddr_in6);
            }
            else
            {
                auto& a = reinterpret_cast<struct sockaddr_in&>(addr);
                a.sin_family = AF_INET;
                a.sin_addr.s_addr = INADDR_ANY;
                a.sin_port = htons(bind_port_);
                addr_len = sizeof(struct sockaddr_in);
            }

            if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), addr_len) == -1)
            {
                auto err = from_errno(errno);
                if (peer_addr)
                {
                    ::freeaddrinfo(peer_addr);
                }
                ::close(fd_);
                fd_ = -1;
                return err;
            }
        }

        if (peer_addr)
        {
            if (::connect(fd_, peer_addr->ai_addr, peer_addr->ai_addrlen) == -1)
            {
                auto err = from_errno(errno);
                ::freeaddrinfo(peer_addr);
                ::close(fd_);
                fd_ = -1;
                return err;
            }
            ::freeaddrinfo(peer_addr);
        }

        return {};
    }
}
