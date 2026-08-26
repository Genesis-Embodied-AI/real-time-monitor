// Wires a Probe to its destination, through io/factory.h so that nothing here names a
// concrete backend.

#ifndef RTM_LIB_PROBE_FACTORY_H
#define RTM_LIB_PROBE_FACTORY_H

#include <cstdint>
#include <string_view>
#include <system_error>

#include "rtm/probe.h"

namespace rtm
{
    /// Initialize `probe` onto the recorder listening on `path`, or onto a discard sink when
    /// nothing is listening: the probe is usable either way, so a caller needs no null check.
    /// An empty `path` takes the recorder's default. The connect error comes back for the
    /// caller to report -- this library has no logger.
    std::error_code connect_probe(Probe& probe, std::string_view process,
                                  std::string_view task_name, nanoseconds process_start_time,
                                  nanoseconds task_period, int32_t task_priority,
                                  std::string_view path = {});
}

#endif
