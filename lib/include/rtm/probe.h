#ifndef RTM_LIB_PROBE_H
#define RTM_LIB_PROBE_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "rtm/io/io.h"
#include "rtm/io/posix/local_socket.h"
#include "rtm/os/time.h"

namespace rtm
{
    class Probe;

    class ProbeGuard
    {
    public:
        ProbeGuard(Probe& probe);
        ~ProbeGuard();

        ProbeGuard(ProbeGuard const&) = delete;
        ProbeGuard(ProbeGuard&&) = delete;
        ProbeGuard& operator=(ProbeGuard const&) = delete;
        ProbeGuard& operator=(ProbeGuard&&) = delete;

    private:
        Probe* probe_;
    };

    class Probe
    {
    public:
        Probe();
        ~Probe();

        void init(std::string_view process, std::string_view task_name,
                  nanoseconds process_start_time, nanoseconds task_period, int32_t task_priority,
                  std::unique_ptr<AbstractIO> io);

        void update_priority(int32_t priority);
        void update_period(nanoseconds period);
        void set_threshold(nanoseconds threshold);
        void log(nanoseconds timestamp = since_epoch());
        void flush();

    private:
        void update_reference(nanoseconds new_ref);

        nanoseconds period_{};
        int32_t priority_{};

        // buffer to reduce write accesses
        static constexpr std::size_t MAX_SAMPLES = 10;
        std::vector<uint32_t> samples_{};

        nanoseconds last_reference_{};

        std::unique_ptr<AbstractIO> io_{};
    };

    /// Initialize `probe` onto the recorder listening on `path`, or onto a discard sink when
    /// nothing is listening: the probe is usable either way, so a caller needs no null check.
    /// The connect error comes back for the caller to report -- this library has no logger.
    std::error_code connect_probe(Probe& probe, std::string_view process,
                                  std::string_view task_name, nanoseconds process_start_time,
                                  nanoseconds task_period, int32_t task_priority,
                                  std::string_view path = DEFAULT_LISTENING_PATH);
}

#endif
