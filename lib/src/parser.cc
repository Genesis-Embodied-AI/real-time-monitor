#include <cmath>

#include "serializer.h"
#include "parser.h"

namespace rtm
{
    Parser::Parser(std::unique_ptr<AbstractReadIO> io)
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

        printf("version: %d\n", header_.version);
        printf("offset:  %ld\n", header_.data_section_offset);
        printf("start:   %ld\n", header_.start_time.count());
        printf("process: %s\n", header_.process.c_str());
        printf("thread:  %s\n", header_.name.c_str());
    }

    void Parser::load_samples()
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

        auto refill = [&]()
        {

            // move end of the buffer to the beginning (may overlap!)
            std::size_t to_copy = available_bytes - (pos - buffer);
            std::memcpy(buffer, pos, to_copy);
            pos = buffer;
            available_bytes = io_->read(buffer + to_copy, BUFFER_SIZE - to_copy);
            return available_bytes;
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
                            available_bytes = refill();
                        }

                        last_reference = nanoseconds{extract_data<uint64_t>(pos)};
                        samples_.push_back(last_reference - header_.start_time);
                        //printf("  last ref: %ld\n", last_reference.count());
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PERIOD)
                    {
                        if (not check_boundary(sizeof(uint64_t)))
                        {
                            available_bytes = refill();
                        }

                        nanoseconds new_period = nanoseconds{extract_data<uint64_t>(pos)};
                        //printf("  period:   %ld\n", new_period.count());
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PRIORITY)
                    {
                        if (not check_boundary(sizeof(uint32_t)))
                        {
                            available_bytes = refill();
                        }

                        uint32_t new_priority = extract_data<uint32_t>(pos);
                        //printf("  priority:   %d\n", new_priority);
                        continue;
                    }

                    printf("Something wrong happened: command not recognized! (%08x)\n", raw_sample);
                    continue;
                }

                nanoseconds sample = nanoseconds(raw_sample) + (last_reference - header_.start_time);
                samples_.push_back(sample);
            }
        }

        printf("whole size: %ld\n", samples_.size());
    }


    std::vector<Parser::Point> Parser::generate_times_diff() const
    {
        using seconds_f = std::chrono::duration<float>;
        using milliseconds_f = std::chrono::duration<float, std::milli>;

        std::vector<Point> serie;
        serie.reserve(samples_.size() / 2);

        for (std::size_t i = 2; i < samples_.size(); i += 2)
        {
            seconds_f x = samples_[i];
            milliseconds_f y = samples_[i] - samples_[i - 2];
            serie.push_back({x.count(), y.count()});
        }

        return serie;
    }

    std::vector<Parser::Point> Parser::generate_times_up() const
    {
        using seconds_f = std::chrono::duration<float>;
        using milliseconds_f = std::chrono::duration<float, std::milli>;

        std::vector<Point> serie;
        serie.reserve(samples_.size() / 2);

        for (std::size_t i = 2; i < samples_.size(); i += 2)
        {
            seconds_f x = samples_[i - 2];
            milliseconds_f y = samples_[i - 1] - samples_[i - 2];
            serie.push_back({x.count(), y.count()});
        }

        return serie;
    }


    result<std::vector<Parser::Point>> minmax_downsampler(std::vector<Parser::Point> const& series, uint32_t threshold)
    {
        if (series.empty() or threshold < 4)
        {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        uint32_t const n = static_cast<uint32_t>(series.size());
        if (threshold >= n)
        {
            return series;
        }

        uint32_t const bucket_size = n / (threshold / 2);

        std::vector<Parser::Point> out;
        out.reserve(threshold);

        for (uint32_t i = 0; i < n; i += bucket_size)
        {
            uint32_t end = std::min(i + bucket_size, n);

            double minY = std::numeric_limits<double>::infinity();
            double maxY = -std::numeric_limits<double>::infinity();
            Parser::Point minP, maxP;

            for (uint32_t j = i; j < end; ++j)
            {
                auto const& p = series[j];

                if (p.y < minY)
                {
                    minY = p.y;
                    minP = p;
                }
                if (p.y > maxY)
                {
                    maxY = p.y;
                    maxP = p;
                }
            }

            out.push_back(minP);
            out.push_back(maxP);
        }

        return out;
    }


    // Helper function to calculate triangle area using three points
    inline double calculate_triangle_area(Parser::Point const& a,
                                          Parser::Point const& b,
                                          Parser::Point const& c)
    {
        return std::abs((a.x - c.x) * (b.y - a.y) - (a.x - b.x) * (c.y - a.y)) * 0.5;
    }

    result<std::vector<Parser::Point>> lttb(std::vector<Parser::Point> const& serie, uint32_t threshold)
    {
        // Validate input
        if (serie.empty() or threshold < 3)
        {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        // If threshold is greater than or equal to data size, return original data
        uint32_t const n = static_cast<uint32_t>(serie.size());
        if (n <= threshold)
        {
            return serie;
        }

        std::vector<Parser::Point> sampled;
        sampled.reserve(threshold);

        // Always add the first point
        sampled.push_back(serie.front());

        // Bucket size (excluding first and last points)
        double const bucket_size = static_cast<double>(n - 2) / (threshold - 2);

        uint32_t a = 0; // Initially point A is the first point

        for (uint32_t i = 0; i < threshold - 2; ++i)
        {
            // Calculate bucket range for current bucket
            uint32_t const bucket_start = static_cast<uint32_t>(std::floor((i + 0) * bucket_size)) + 1;
            uint32_t const bucket_end   = static_cast<uint32_t>(std::floor((i + 1) * bucket_size)) + 1;

            // Calculate average point for next bucket (used as point C)
            double avg_x = 0.0, avg_y = 0.0;
            uint32_t avg_range_start, avg_range_end;

            if (i < threshold - 3)
            {
                // Not the last bucket
                avg_range_start = static_cast<uint32_t>(std::floor((i + 1) * bucket_size)) + 1;
                avg_range_end   = static_cast<uint32_t>(std::floor((i + 2) * bucket_size)) + 1;
            }
            else
            {
                // Last bucket - use the last point
                avg_range_start = n - 1;
                avg_range_end   = n;
            }

            // Calculate average
            avg_range_end = std::min(avg_range_end, n);
            uint32_t const avg_range_length = avg_range_end - avg_range_start;

            for (uint32_t j = avg_range_start; j < avg_range_end; ++j)
            {
                avg_x += serie[j].x;
                avg_y += serie[j].y;
            }
            avg_x /= avg_range_length;
            avg_y /= avg_range_length;

            Parser::Point const avg_point{avg_x, avg_y};

            // Find point in current bucket with largest triangle area
            uint32_t max_area_point = bucket_start;
            double max_area = -1.0;

            uint32_t const actual_bucket_end = std::min(bucket_end, n);

            for (uint32_t j = bucket_start; j < actual_bucket_end; ++j)
            {
                double const area = calculate_triangle_area(serie[a], serie[j], avg_point);

                if (area > max_area)
                {
                    max_area = area;
                    max_area_point = j;
                }
            }

            // Add the point with the largest area
            sampled.push_back(serie[max_area_point]);
            a = max_area_point; // This point becomes the new point A
        }

        // Always add the last point
        sampled.push_back(serie.back());

        return sampled;
    }


    result<std::vector<Parser::Point>> minmax_lttb(std::vector<Parser::Point> const& series, uint32_t threshold)
    {
        // 1. min–max preselection
        auto preselect = minmax_downsampler(series, threshold * 4);
        if (not preselect)
        {
            return std::unexpected(preselect.error());
        }

        // 2. LTTB
        auto l = lttb(*preselect, threshold);
        if (not l)
        {
            return std::unexpected(l.error());
        }

        return l;
    }
}
