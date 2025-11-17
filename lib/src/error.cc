#include "error.h"

namespace rtm
{
    std::error_code from_errno(int code)
    {
        return {code, std::system_category()};
    }
}
