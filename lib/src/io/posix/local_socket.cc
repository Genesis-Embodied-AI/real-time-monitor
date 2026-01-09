#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include "rtm/io/posix/local_socket.h"

namespace rtm
{
    static void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static void setup_address(std::string const& path, struct sockaddr_un& addr)
    {
        std::memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    }

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

        // For now, accepted sockets are always non blocking. To be changed if blocking behavior is needed at one point
        set_nonblocking(fd_);

        struct sockaddr_un addr;
        setup_address(local_path_, addr);

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
        int socket_fd = ::accept(fd_, nullptr, nullptr);
        if (socket_fd == -1)
        {
            return nullptr;
        }

        if (mode & access::Mode::NON_BLOCKING)
        {
            set_nonblocking(socket_fd);
        }

        return std::make_unique<LocalSocket>(socket_fd, mode | access::Mode::READ_WRITE);
    }


    LocalSocket::LocalSocket(os_socket fd, access::Mode modes)
        : AbstractSocket()
    {
        fd_ = fd;
        modes_ = modes;
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    LocalSocket::LocalSocket(std::string_view address)
        : AbstractSocket()
        , local_path_{address}
    {
        fd_ = -1;
        supported_modes_ = access::Mode::READ_WRITE | access::Mode::NON_BLOCKING;
    }

    LocalSocket::~LocalSocket()
    {
        close();
    }

    std::error_code LocalSocket::do_open(access::Mode mode)
    {
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ == -1)
        {
            return from_errno(errno);
        }

        if (mode & access::Mode::NON_BLOCKING)
        {
            set_nonblocking(fd_);
        }

        if (not local_path_.empty())
        {
            struct sockaddr_un addr;
            setup_address(local_path_, addr);

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
