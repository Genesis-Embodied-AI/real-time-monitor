#include "io/io.h"

namespace rtm
{
    bool AbstractIO::is_open() const
    {
        return access::is_open(modes_);
    }

    bool AbstractIO::is_readable() const
    {
        return access::is_readable(modes_);
    }

    bool AbstractIO::is_writable() const
    {
        return access::is_writable(modes_);
    }

    bool AbstractIO::is_blocking() const
    {
        return access::is_blocking(modes_);
    }

    std::error_code AbstractIO::seek(int64_t)
    {
        return from_errno(ENOSYS);
    }

    std::error_code AbstractIO::open(access::Mode modes)
    {
        if (is_open())
        {
            return {EALREADY, std::system_category()};
        }

        if ((modes & supported_modes_) != modes)
        {
            return {EINVAL, std::system_category()};
        }

        auto rc = do_open(modes);
        if (rc)
        {
            return rc;
        }

        modes_ = modes;
        return {};
    }

    std::error_code AbstractIO::close()
    {
        if (not is_open())
        {
            return {EBADF, std::system_category()};
        }

        auto rc = do_close();
        if (not rc)
        {
            modes_ = access::Mode::CLOSE;
        }
        return rc;
    }
}
