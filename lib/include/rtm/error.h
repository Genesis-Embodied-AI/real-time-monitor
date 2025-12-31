#ifndef RTM_LIB_ERROR_H
#define RTM_LIB_ERROR_H

#include <system_error>

namespace rtm
{
    std::error_code from_errno(int code);
}

#endif
