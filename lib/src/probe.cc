#include <array>
#include <cstring>
#include <cstdio>

#include "commands.h"
#include "probe.h"
#include "serializer.h"

namespace rtm
{
    Probe::Probe()
    {
        samples_.reserve(MAX_SAMPLES);
    }

    Probe::~Probe()
    {
        if (io_ != nullptr)
        {
            flush();
        }
    }

    void Probe::init(std::string_view process, std::string_view task_name,
                         nanoseconds process_start_time, nanoseconds task_period, uint32_t task_priority,
                         std::unique_ptr<AbstractIO> io)
    {
        constexpr uint16_t PROTOCOL_VERSION = 1;
        constexpr uint8_t PADDING[6] = {0};

        io_ = std::move(io);

        std::vector<uint8_t> header_buffer;

        append(header_buffer, PROTOCOL_VERSION);
        append(header_buffer, PADDING);

        // data offset dummy write - waiting for computation
        int64_t data_offset = 0;
        append(header_buffer, data_offset);

        // write session UUID
        std::array<uint8_t, 16> uuid = {0}; // TODO
        append(header_buffer, uuid);

        // write processus startup time since epoch in ns
        append(header_buffer, process_start_time);

        uint16_t process_size = static_cast<uint16_t>(process.size());
        append(header_buffer, process_size);
        append(header_buffer, process);

        uint16_t task_name_size = static_cast<uint16_t>(task_name.size());
        append(header_buffer, task_name_size);
        append(header_buffer, task_name);

        // Align the data section on 8 bytes boundaries
        data_offset = header_buffer.size();
        if (data_offset % 8)
        {
            data_offset += (8 - data_offset % 8);
        }
        header_buffer.resize(data_offset);

        // write back data offset
        std::memcpy(header_buffer.data() + 8, &data_offset, sizeof(data_offset));

        // write data section header
        append(header_buffer, PROTOCOL_VERSION);
        append(header_buffer, PADDING);

        // Write header
        io_->write(header_buffer.data(), header_buffer.size());

        // Initial update of period/prio/ref
        update_period(task_period);
        update_priority(task_priority);
    }

    void Probe::update_priority(uint32_t priority)
    {
        priority_ = priority;

        constexpr uint32_t oob = ESCAPE | Command::UPDATE_PRIORITY;
        io_->write(&oob, sizeof(uint32_t));
        io_->write(&priority_, sizeof(uint32_t));
    }

    void Probe::update_period(nanoseconds period)
    {
        period_ = period;

        constexpr uint32_t oob = ESCAPE | Command::UPDATE_PERIOD;
        io_->write(&oob, sizeof(uint32_t));
        io_->write(&period_, sizeof(period_));
    }

    void Probe::update_reference(nanoseconds new_ref)
    {
        flush(); // shall be written before the new reference to keep coherency

        last_reference_ = new_ref;
        uint64_t raw_ref = last_reference_.count();

        constexpr uint32_t oob = ESCAPE | Command::UPDATE_REFERENCE;
        io_->write(&oob, sizeof(uint32_t));
        io_->write(&raw_ref, sizeof(uint64_t));
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
        io_->write(samples_.data(), samples_.size() * sizeof(decltype(samples_)::value_type));
        samples_.clear();
    }
}
