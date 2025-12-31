#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "rtm/io/posix/local_socket.h"

namespace rtm
{
    LocalListener::LocalListener(std::string_view local_path)
        : AbstractListener()
        , local_path_{local_path}
    {

    }

    LocalListener::~LocalListener()
    {
        if (fd_ != -1)
        {
            ::close(fd_);
            ::unlink(local_path_.c_str());
        }
    }

    std::error_code LocalListener::listen(int backlog)
    {
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ == -1)
        {
            return from_errno(errno);
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, local_path_.c_str(), sizeof(addr.sun_path) - 1);

        (void) unlink(local_path_.c_str()); // Destroy a potential socket with the same name.
        if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            ::close(fd_);
            return from_errno(errno);
        }

        if (::listen(fd_, backlog) == -1)
        {
            ::close(fd_);
            return from_errno(errno);
        }

        return {};
    }


    std::unique_ptr<AbstractSocket> LocalListener::accept(access::Mode mode)
    {
        int flags = 0;
        if (mode & access::Mode::NON_BLOCKING)
        {
            flags = SOCK_NONBLOCK;
        }

        int socket_fd = accept4(fd_, nullptr, nullptr, flags);
        if (socket_fd == -1)
        {
            return nullptr;
        }

        return std::make_unique<LocalSocket>(socket_fd,mode | access::Mode::READ_WRITE);
    }


    LocalSocket::LocalSocket(os_socket fd, access::Mode modes)
        : AbstractSocket()
    {
        fd_ = fd;
        modes_ = modes;
    }

    LocalSocket::LocalSocket(std::string_view address)
        : AbstractSocket()
        , local_path_{address}
    {
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    LocalSocket::~LocalSocket()
    {
        close();
    }

    std::error_code LocalSocket::do_open(access::Mode mode)
    {
        int flags = SOCK_STREAM;
        if (mode & access::Mode::NON_BLOCKING)
        {
            flags |= SOCK_NONBLOCK;
        }

        fd_ = ::socket(AF_UNIX, flags, 0);
        if (fd_ == -1)
        {
            return from_errno(errno);
        }

        if (not local_path_.empty())
        {
            struct sockaddr_un addr;
            addr.sun_family = AF_UNIX;
            std::memset(&addr, 0, sizeof(struct sockaddr_un));
            std::strncpy(addr.sun_path, local_path_.c_str(), sizeof(addr.sun_path) - 1);

            if (::connect(fd_, (struct sockaddr const*)&addr, sizeof(struct sockaddr_un)) == -1)
            {
                auto rc = from_errno(errno);
                ::close(fd_);
                return rc;
            }
        }

        return {};
    }
}
