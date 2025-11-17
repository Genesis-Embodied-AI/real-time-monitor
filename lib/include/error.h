#ifndef RTM_ERROR_H
#define RTM_ERROR_H

#include <expected>
#include <system_error>

namespace rtm
{
    template<typename T, typename E = std::error_code>
    using result = std::expected<T, E>;

    std::error_code from_errno(int code);
}

#endif
