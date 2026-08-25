#include <array>
#include <cstring>
#include <cstdio>

#include "commands.h"
#include "parser.h"
#include "probe.h"
#include "serializer.h"

namespace rtm
{
    ProbeGuard::ProbeGuard(Probe& probe)
        : probe_(&probe)
    {
        probe_->log();
    }

    ProbeGuard::~ProbeGuard()
    {
        probe_->log();
    }

    Probe::Probe()
    {
        samples_.reserve(MAX_SAMPLES);
    }

    Probe::~Probe()
    {
        if (io_ != nullptr)
        {
            flush();
            uint32_t sentinel = ESCAPE | Command::DATA_STREAM_END;
            io_->write(&sentinel, sizeof(sentinel));
        }
    }

    void Probe::init(std::string_view process, std::string_view task_name,
                         nanoseconds process_start_time, nanoseconds task_period, int32_t task_priority,
                         std::unique_ptr<AbstractIO> io)
    {
        io_ = std::move(io);

        std::array<uint8_t, 16> uuid = {0}; // TODO
        std::vector<uint8_t> header_buffer = build_tick_header(
            uuid, process_start_time, process, task_name);

        io_->write(header_buffer.data(), header_buffer.size());

        // Initial update of period/prio/ref
        update_period(task_period);
        update_priority(task_priority);
    }

    void Probe::update_priority(int32_t priority)
    {
        priority_ = priority;
        write_command(*io_, Command::UPDATE_PRIORITY, priority_);
    }

    void Probe::update_period(nanoseconds period)
    {
        period_ = period;
        write_command(*io_, Command::UPDATE_PERIOD, period);
    }

    void Probe::set_threshold(nanoseconds threshold)
    {
        flush();
        write_command(*io_, Command::SET_THRESHOLD, threshold);
    }

    void Probe::update_reference(nanoseconds new_ref)
    {
        flush();
        last_reference_ = new_ref;
        write_command(*io_, Command::UPDATE_REFERENCE, last_reference_);
    }

    void Probe::log(nanoseconds timestamp)
    {
        constexpr nanoseconds MAX_WINDOW = nanoseconds(ESCAPE);

        nanoseconds relative_timestamp = timestamp - last_reference_;
        if (relative_timestamp > MAX_WINDOW)
        {
            update_reference(timestamp);
            return;
        }

        uint32_t sample = static_cast<uint32_t>(relative_timestamp.count());
        samples_.push_back(sample);
        if (samples_.size() > MAX_SAMPLES)
        {
            flush();
        }
    }

    void Probe::flush()
    {
        if (not samples_.empty())
        {
            io_->write(samples_.data(), samples_.size() * sizeof(decltype(samples_)::value_type));
            samples_.clear();
        }
    }
}
