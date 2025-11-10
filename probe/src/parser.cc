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

    void Parser::load_samples(nanoseconds begin, nanoseconds end)
    {
        // jump to data section
        io_->seek(header_.data_section_offset);

        // Get version
        io_->read(&header_.data_version, 2);
        io_->seek(header_.data_section_offset + 8);


        // extracts samples
        nanoseconds last_reference{};
        std::vector<nanoseconds> samples;

        while (true)
        {
            // read a chunk
            uint8_t buffer[2 << 15]; // 64KB
            int64_t available_bytes = io_->read(buffer, sizeof(buffer));
            if (available_bytes <= 0)
            {
                break;
            }


            // parse data in the chunk
            uint8_t const* pos = buffer;
            while (pos < (buffer + available_bytes))
            {
                uint32_t raw_sample = extract_data<uint32_t>(pos);
                if (raw_sample & ESCAPE)
                {
                    printf("out of band data!\n");
                    if (raw_sample & Command::UPDATE_REFERENCE)
                    {
                        last_reference = nanoseconds{extract_data<uint64_t>(pos)};
                        printf("  last ref: %ld\n", last_reference.count());
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PERIOD)
                    {
                        nanoseconds new_period = nanoseconds{extract_data<uint64_t>(pos)};
                        printf("  period:   %ld\n", new_period.count());
                        continue;
                    }

                    if (raw_sample & Command::UPDATE_PRIORITY)
                    {
                        uint32_t new_priority = extract_data<uint32_t>(pos);
                        printf("  priority:   %d\n", new_priority);
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
}
