#include "serializer.h"
#include "parser.h"

namespace rtm
{
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
        int64_t file_base = static_cast<int64_t>(header_.data_section_offset) + 8;

        auto refill = [&]() -> int64_t
        {
            std::size_t remaining = available_bytes - (pos - buffer);
            file_base += (pos - buffer);
            // pos overlaps with buffer (it is a pointer into buffer), so memmove is required.
            std::memmove(buffer, pos, remaining);
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

        bool end_of_stream = false;
        while (not end_of_stream)
        {
            // read a chunk
            if (refill() <= 0)
            {
                header_.sentinel_pos = -(file_base + available_bytes);
                break;
            }

            // parse data in the chunk
            while (check_boundary(sizeof(uint32_t)))
            {
                uint32_t raw_sample = extract_data<uint32_t>(pos);
                if (raw_sample & ESCAPE)
                {
                    if (raw_sample == (ESCAPE | Command::DATA_STREAM_END))
                    {
                        header_.sentinel_pos = file_base + (pos - buffer) - 4;
                        end_of_stream = true;
                        break;
                    }

                    if (raw_sample & Command::UPDATE_REFERENCE)
                    {
                        if (not check_boundary(sizeof(uint64_t)))
                        {
                            refill();
                            if (not check_boundary(sizeof(uint64_t)))
                            {
                                end_of_stream = true;
                                break;
                            }
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
                            if (not check_boundary(sizeof(uint64_t)))
                            {
                                end_of_stream = true;
                                break;
                            }
                        }

                        extract_data<uint64_t>(pos);
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PRIORITY)
                    {
                        if (not check_boundary(sizeof(int32_t)))
                        {
                            refill();
                            if (not check_boundary(sizeof(int32_t)))
                            {
                                end_of_stream = true;
                                break;
                            }
                        }

                        extract_data<int32_t>(pos);
                        continue;
                    }

                    if (raw_sample & Command::SET_THRESHOLD)
                    {
                        if (not check_boundary(sizeof(uint64_t)))
                        {
                            refill();
                            if (not check_boundary(sizeof(uint64_t)))
                            {
                                end_of_stream = true;
                                break;
                            }
                        }

                        extract_data<uint64_t>(pos);
                        continue;
                    }

                    // Unrecognized escape command — the stream is desynced,
                    // any further parsing would produce garbage. Bail out.
                    printf("Something wrong happened: command not recognized! (%08x)\n", raw_sample);
                    end_of_stream = true;
                    break;
                }

                nanoseconds sample = nanoseconds(raw_sample) + (last_reference - header_.start_time);
                samples_.push_back(sample);
            }

        }

        if (samples_.empty())
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
            diff_min_ = std::min(diff_min_, y);
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
            up_min_ = std::min(up_min_, y);
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

    milliseconds_f Parser::diff_min() const
    {
        return diff_min_;
    }

    milliseconds_f Parser::diff_max() const
    {
        return diff_max_;
    }

    milliseconds_f Parser::up_min() const
    {
        return up_min_;
    }

    milliseconds_f Parser::up_max() const
    {
        return up_max_;
    }
}
