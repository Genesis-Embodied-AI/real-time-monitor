#include <cinttypes>

#include "serializer.h"
#include "parser.h"

namespace rtm
{
    Parser::Parser(std::unique_ptr<AbstractIO> io)
        : io_{std::move(io)}
    {

    }

    void Parser::load_header()
    {
        auto read_string =[&]()
        {
            uint16_t next_str_size;
            io_->read(&next_str_size, sizeof(uint16_t));

            std::vector<char> buffer;
            buffer.resize(next_str_size);
            io_->read(buffer.data(), next_str_size);

            return std::string{buffer.begin(), buffer.end()};
        };

        io_->read(&header_.version, 2);
        io_->seek(8);

        io_->read(&header_.data_section_offset, sizeof(uint64_t));
        io_->read(header_.uuid.data(), header_.uuid.size());
        io_->read(&header_.start_time, sizeof(nanoseconds));

        header_.process = read_string();
        header_.name = read_string();
    }

    void Parser::print_header()
    {
        printf("version: %u\n", header_.version);
        printf("offset:  %" PRIu64 "\n", header_.data_section_offset);
        printf("start:   %" PRId64 "\n", header_.start_time.count());
        printf("process: %s\n", header_.process.c_str());
        printf("thread:  %s\n", header_.name.c_str());
    }

    bool Parser::load_samples()
    {
        // jump to data section
        io_->seek(header_.data_section_offset);

        // Get version
        io_->read(&header_.data_version, 2);
        io_->seek(header_.data_section_offset + 8);


        // extracts samples
        nanoseconds last_reference{};
        std::vector<nanoseconds> samples;

        constexpr std::size_t BUFFER_SIZE = 2 << 15; // 64KB;
        uint8_t buffer[BUFFER_SIZE];
        uint8_t const* pos = buffer;
        int64_t available_bytes = 0;

        auto refill = [&]() -> int64_t
        {
            std::size_t remaining = available_bytes - (pos - buffer);
            std::memcpy(buffer, pos, remaining);
            pos = buffer;
            int64_t newly_read = io_->read(buffer + remaining, BUFFER_SIZE - remaining);
            if (newly_read > 0)
            {
                available_bytes = static_cast<int64_t>(remaining) + newly_read;
                return available_bytes;
            }
            available_bytes = static_cast<int64_t>(remaining);
            return 0;
        };

        auto check_boundary = [&](std::size_t up_to)
        {
            return ((pos + up_to) <= (buffer + available_bytes));
        };

        while (true)
        {
            // read a chunk
            if (refill() <= 0)
            {
                break;
            }

            // parse data in the chunk
            while (check_boundary(sizeof(uint32_t)))
            {
                uint32_t raw_sample = extract_data<uint32_t>(pos);
                if (raw_sample & ESCAPE)
                {
                    if (raw_sample & Command::UPDATE_REFERENCE)
                    {
                        if (not check_boundary(sizeof(uint64_t)))
                        {
                            refill();
                        }

                        last_reference = nanoseconds{extract_data<uint64_t>(pos)};
                        samples_.push_back(last_reference - header_.start_time);
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PERIOD)
                    {
                        if (not check_boundary(sizeof(uint64_t)))
                        {
                            refill();
                        }

                        extract_data<uint64_t>(pos);
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PRIORITY)
                    {
                        if (not check_boundary(sizeof(int32_t)))
                        {
                            refill();
                        }

                        extract_data<int32_t>(pos);
                        continue;
                    }

                    printf("Something wrong happened: command not recognized! (%08x)\n", raw_sample);
                    continue;
                }

                nanoseconds sample = nanoseconds(raw_sample) + (last_reference - header_.start_time);
                samples_.push_back(sample);
            }
        }

        if (samples_.size() == 0)
        {
            return false;
        }
        begin_ = samples_.front();
        end_   = samples_.back();
        return true;
    }


    std::vector<Point> Parser::generate_times_diff()
    {
        std::vector<Point> serie;
        serie.reserve(samples_.size() / 2);

        for (std::size_t i = 2; i < samples_.size(); i += 2)
        {
            seconds_f x = samples_[i];
            milliseconds_f y = samples_[i] - samples_[i - 2];
            diff_max_ = std::max(diff_max_, y);
            serie.push_back({x.count(), y.count()});
        }

        return serie;
    }

    std::vector<Point> Parser::generate_times_up()
    {
        std::vector<Point> serie;
        serie.reserve(samples_.size() / 2);

        for (std::size_t i = 1; i < samples_.size(); i += 2)
        {
            seconds_f x = samples_[i - 1];
            milliseconds_f y = samples_[i] - samples_[i - 1];
            up_max_ = std::max(up_max_, y);
            serie.push_back({x.count(), y.count()});
        }

        return serie;
    }

    nanoseconds Parser::begin() const
    {
        return begin_;
    }

    nanoseconds Parser::end() const
    {
        return end_;
    }

    milliseconds_f Parser::diff_max() const
    {
        return diff_max_;
    }

    milliseconds_f Parser::up_max() const
    {
        return up_max_;
    }
}
