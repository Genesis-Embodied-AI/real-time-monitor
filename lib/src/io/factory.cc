#include "io/factory.h"
#include "io/null.h"
#include "io/posix/local_socket.h"

namespace rtm
{
    std::unique_ptr<AbstractIO> make_null_io()
    {
        return std::make_unique<NullIO>();
    }

    std::unique_ptr<AbstractIO> make_local_socket_io(std::string_view path)
    {
        if (path.empty())
        {
            return std::make_unique<LocalSocket>(DEFAULT_LISTENING_PATH);
        }
        return std::make_unique<LocalSocket>(path);
    }
}
