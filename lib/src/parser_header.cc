#include <cinttypes>
#include <cstring>

#include "parser.h"
#include "serializer.h"

namespace rtm
{
    std::vector<uint8_t> build_tick_header(
        std::array<uint8_t, 16> const& uuid,
        nanoseconds start_time,
        std::string_view process,
        std::string_view task)
    {
        constexpr uint8_t HEADER_PADDING[4] = {0};
        constexpr uint16_t DATA_VERSION = 1;
        constexpr uint8_t DATA_PADDING[6] = {0};

        std::vector<uint8_t> header;
        header.reserve(64 + process.size() + task.size());

        append(header, PROTOCOL_MAJOR);
        append(header, PROTOCOL_MINOR);
        append(header, HEADER_PADDING);

        // data offset placeholder — patched in after alignment
        int64_t data_offset = 0;
        append(header, data_offset);

        append(header, uuid);
        append(header, start_time);

        uint16_t process_size = static_cast<uint16_t>(process.size());
        append(header, process_size);
        append(header, process);

        uint16_t task_size = static_cast<uint16_t>(task.size());
        append(header, task_size);
        append(header, task);

        int64_t metadata_footer_offset = 0;
        append(header, metadata_footer_offset);

        // Align the data section on 8-byte boundary, then patch data_offset.
        data_offset = static_cast<int64_t>(header.size());
        if (data_offset % 8)
        {
            data_offset += (8 - data_offset % 8);
        }
        header.resize(static_cast<std::size_t>(data_offset));
        std::memcpy(header.data() + 8, &data_offset, sizeof(data_offset));

        append(header, DATA_VERSION);
        append(header, DATA_PADDING);

        return header;
    }

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

        io_->read(&header_.major, 2);
        io_->read(&header_.minor, 2);
        io_->seek(8);

        io_->read(&header_.data_section_offset, sizeof(uint64_t));
        io_->read(header_.uuid.data(), header_.uuid.size());
        io_->read(&header_.start_time, sizeof(nanoseconds));

        header_.process = read_string();
        header_.name = read_string();

        if (header_.major >= 2)
        {
            io_->read(&header_.metadata_footer_offset, sizeof(int64_t));
        }

        header_.original_name = header_.process;
        header_.original_name += '.';
        header_.original_name += header_.name;
        header_.original_name += " (";
        header_.original_name += format_iso_timestamp(header_.start_time);
        header_.original_name += ')';
    }

    void Parser::print_header()
    {
        printf("version: %u.%u\n", header_.major, header_.minor);
        printf("offset:  %" PRIu64 "\n", header_.data_section_offset);
        printf("start:   %" PRId64 "\n", header_.start_time.count());
        printf("process: %s\n", header_.process.c_str());
        printf("thread:  %s\n", header_.name.c_str());
    }
}
