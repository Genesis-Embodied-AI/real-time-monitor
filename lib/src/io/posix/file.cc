#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rtm/io/file.h"

namespace rtm
{
    File::File(std::string_view filename)
        : fd_{-1}
        , filename_{filename}
    {
        supported_modes_ = access::Mode::READ_ONLY     | access::Mode::WRITE_ONLY | access::Mode::READ_WRITE |
                           access::Mode::NON_BLOCKING  | access::Mode::TRUNCATE   | access::Mode::NEW_ONLY   |
                           access::Mode::EXISTING_ONLY | access::Mode::APPEND;
    }

    File::~File()
    {
        close();
    }

    int64_t File::read(void* buffer, int64_t size)
    {
        return ::read(fd_, buffer, size);
        //return static_cast<int64_t>(rc);
    }

    void File::write(void const* buffer, int64_t size)
    {
        ssize_t rc = ::write(fd_, buffer, size);
        if (rc < 0)
        {
            throw std::system_error(errno, std::generic_category());
        }
    }

    std::error_code File::do_open(access::Mode access)
    {
        int posix_flag = 0;
        if (access & access::Mode::READ_ONLY)
        {
            posix_flag |= O_RDONLY;
        }
        if (access & access::Mode::WRITE_ONLY)
        {
            posix_flag |= O_WRONLY;
            posix_flag |= O_CREAT;
        }
        if (access & access::Mode::READ_WRITE)
        {
            posix_flag |= O_RDWR;
        }
        if (access & access::Mode::NON_BLOCKING)
        {
            posix_flag |= O_NONBLOCK;
        }
        if (access & access::Mode::TRUNCATE)
        {
            posix_flag |= O_TRUNC;
        }
        if (access & access::Mode::NEW_ONLY)
        {
            posix_flag |= O_EXCL;
        }
        if (access & access::Mode::EXISTING_ONLY)
        {
            posix_flag &= ~O_CREAT;
        }
        if (access & access::Mode::APPEND)
        {
            posix_flag |= O_APPEND;
        }

        fd_ = ::open(filename_.c_str(), posix_flag, S_IRUSR | S_IWUSR);
        if (fd_ < 0)
        {
            return from_errno(errno);
        }

        return {};
    }

    std::error_code File::do_close()
    {
        int rc = ::close(fd_);
        if (rc < 0)
        {
            return from_errno(errno);
        }

        fd_ = -1;
        return {};
    }

    std::error_code File::seek(int64_t pos)
    {
        off_t rc = ::lseek(fd_, pos, SEEK_SET);
        if (rc < 0)
        {
            return from_errno(errno);
        }

        return {};
    }
}
