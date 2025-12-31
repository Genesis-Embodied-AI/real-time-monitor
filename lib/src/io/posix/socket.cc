#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "rtm/io/socket.h"

namespace rtm
{
    int64_t AbstractSocket::read(void* data, int64_t data_size)
    {
        return ::read(fd_, data, static_cast<std::size_t>(data_size));
    }


    int64_t AbstractSocket::write(void const* data, int64_t data_size)
    {
        return ::write(fd_, data, static_cast<std::size_t>(data_size));
    }


    std::error_code AbstractSocket::do_close()
    {
        if (fd_ != -1)
        {
            // Shutdown syscall is not critical and can't barely fail anyway,
            (void) ::shutdown(fd_, SHUT_RDWR);
            int rc = ::close(fd_);
            fd_ = -1;

            if (rc < 0)
            {
                return from_errno(errno);
            }
        }

        return {};
    }
}
