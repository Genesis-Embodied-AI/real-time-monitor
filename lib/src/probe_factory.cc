#include "io/factory.h"
#include "probe_factory.h"

namespace rtm
{
    std::error_code connect_probe(Probe& probe, std::string_view process,
                                  std::string_view task_name, nanoseconds process_start_time,
                                  nanoseconds task_period, int32_t task_priority,
                                  std::string_view path)
    {
        auto io = make_local_socket_io(path);
        auto const rc = io->open(access::Mode::READ_WRITE);
        if (rc)
        {
            io = make_null_io();
            io->open(access::Mode::READ_WRITE);
        }

        probe.init(process, task_name, process_start_time, task_period, task_priority,
                   std::move(io));
        return rc;
    }
}
