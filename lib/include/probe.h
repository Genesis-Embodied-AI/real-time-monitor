#ifndef RTM_PROBE_PROBE_H
#define RTM_PROBE_PROBE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>
#include <vector>

#include "io.h"

namespace rtm
{
    using namespace std::chrono;

    class Probe
    {
    public:
        Probe();
        ~Probe();

        void init(std::string_view process, std::string_view task_name,
                  nanoseconds process_start_time, nanoseconds task_period, uint32_t task_priority,
                  std::unique_ptr<AbstractWriteIO> io);

        void update_priority(uint32_t priority);
        void update_period(nanoseconds period);
        void log(nanoseconds timestamp);
        void flush();

    private:
        void update_reference(nanoseconds new_ref);

        nanoseconds period_{};
        uint32_t priority_{};

        // buffer to reduce write accesses
        static constexpr std::size_t MAX_SAMPLES = 10;
        std::vector<uint32_t> samples_{};

        nanoseconds last_reference_{};

        std::unique_ptr<AbstractWriteIO> io_{};
    };
}

#endif
