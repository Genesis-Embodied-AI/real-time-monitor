#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

#include "rtm/io/file.h"

#include "commands.h"
#include "metadata.h"
#include "parser.h"
#include "serializer.h"

namespace rtm
{
    namespace
    {
        int64_t metadata_ptr_offset(TickHeader const& header)
        {
            // 40 (fixed header) + 2 + process.size() + 2 + name.size()
            return 40
                + 2 + static_cast<int64_t>(header.process.size())
                + 2 + static_cast<int64_t>(header.name.size());
        }

        uint32_t serialize_entries(std::vector<uint8_t>& buf, TickMetadata const& meta)
        {
            uint32_t count = 0;

            if (not meta.display_name.empty())
            {
                uint16_t key_id = MetadataKey::DISPLAY_NAME;
                uint32_t payload_size = static_cast<uint32_t>(meta.display_name.size());
                append(buf, key_id);
                append(buf, payload_size);
                buf.insert(buf.end(),
                           meta.display_name.begin(),
                           meta.display_name.end());
                count++;
            }

            // Visibility and weight are always serialized: payloads are tiny
            // and it avoids a "default vs explicit" sentinel ambiguity.
            {
                uint16_t key_id = MetadataKey::DEFAULT_VISIBILITY;
                uint32_t payload_size = 1;
                append(buf, key_id);
                append(buf, payload_size);
                buf.push_back(meta.default_visibility);
                count++;
            }

            {
                uint16_t key_id = MetadataKey::DISPLAY_WEIGHT;
                uint32_t payload_size = sizeof(int32_t);
                append(buf, key_id);
                append(buf, payload_size);
                append(buf, meta.display_weight);
                count++;
            }

            return count;
        }
    }

    void save_metadata(AbstractIO& io, TickHeader const& header, TickMetadata const& metadata)
    {
        if (header.sentinel_pos <= 0)
        {
            return;
        }

        std::vector<uint8_t> entries;
        uint32_t entry_count = serialize_entries(entries, metadata);

        int64_t footer_offset = header.sentinel_pos + 4;
        io.seek(footer_offset);
        io.write(&entry_count, sizeof(entry_count));
        if (not entries.empty())
        {
            io.write(entries.data(), static_cast<int64_t>(entries.size()));
        }

        int64_t new_size = footer_offset + 4 + static_cast<int64_t>(entries.size());
        io.truncate(new_size);

        io.seek(metadata_ptr_offset(header));
        io.write(&footer_offset, sizeof(footer_offset));
        io.sync();
    }

    void repair_sentinel(AbstractIO& io, int64_t eof_pos)
    {
        io.seek(eof_pos);
        uint32_t sentinel = ESCAPE | Command::DATA_STREAM_END;
        io.write(&sentinel, sizeof(sentinel));
    }

    namespace
    {
        // Read the v1 file contents and build the equivalent v2 byte buffer.
        // Returns empty vector on failure.
        std::vector<uint8_t> build_v2_from_v1(AbstractIO& src)
        {
            src.seek(0);
            uint16_t major = 0;
            if (src.read(&major, 2) != 2)
            {
                return {};
            }
            if (major >= 2)
            {
                // Already migrated — caller handles this as success.
                return {};
            }

            src.seek(8);
            uint64_t old_data_offset = 0;
            if (src.read(&old_data_offset, 8) != 8)
            {
                return {};
            }

            src.seek(40);
            uint16_t str_size = 0;
            if (src.read(&str_size, 2) != 2)
            {
                return {};
            }
            std::vector<uint8_t> process_str(str_size);
            if (src.read(process_str.data(), str_size) != str_size)
            {
                return {};
            }

            if (src.read(&str_size, 2) != 2)
            {
                return {};
            }
            std::vector<uint8_t> task_str(str_size);
            if (src.read(task_str.data(), str_size) != str_size)
            {
                return {};
            }

            // Read uuid (16B) + start_time (8B) from offset 16..40
            std::array<uint8_t, 16> uuid;
            nanoseconds start_time{};
            src.seek(16);
            if (src.read(uuid.data(), uuid.size()) != static_cast<int64_t>(uuid.size()) or
                src.read(&start_time, sizeof(start_time)) != sizeof(start_time))
            {
                return {};
            }

            // Read the data section events until EOF.
            // The v1 data section begins with u16 data_version + 6B padding.
            // build_tick_header() below will re-emit those 8 bytes, so skip
            // them here to avoid duplicating them in the migrated file
            // (which would be parsed as a spurious first timestamp delta).
            src.seek(static_cast<int64_t>(old_data_offset) + 8);
            std::vector<uint8_t> data_section;
            uint8_t chunk[4096];
            while (true)
            {
                int64_t n = src.read(chunk, sizeof(chunk));
                if (n <= 0)
                {
                    break;
                }
                data_section.insert(data_section.end(), chunk, chunk + n);
            }

            std::string_view process_view(reinterpret_cast<char const*>(process_str.data()), process_str.size());
            std::string_view task_view(reinterpret_cast<char const*>(task_str.data()), task_str.size());

            std::vector<uint8_t> new_file = build_tick_header(uuid, start_time, process_view, task_view);
            new_file.reserve(new_file.size() + data_section.size() + sizeof(uint32_t));

            new_file.insert(new_file.end(), data_section.begin(), data_section.end());

            uint32_t sentinel = ESCAPE | Command::DATA_STREAM_END;
            append(new_file, sentinel);

            return new_file;
        }
    }

    bool migrate_v1_to_v2(std::filesystem::path const& path)
    {
        // Read the v1 source.
        std::vector<uint8_t> new_file;
        {
            File src(path.c_str());
            if (src.open(access::Mode::READ_ONLY))
            {
                printf("[Metadata] Cannot open %s for migration\n", path.c_str());
                return false;
            }
            new_file = build_v2_from_v1(src);
            src.close();
        }

        if (new_file.empty())
        {
            // Either the source was already v2 (check again) or a read failed.
            File probe(path.c_str());
            if (probe.open(access::Mode::READ_ONLY))
            {
                return false;
            }
            uint16_t major = 0;
            probe.read(&major, 2);
            probe.close();
            return major >= PROTOCOL_MAJOR;
        }

        // Write the new payload to a temp file next to the target, fsync, then
        // rename over the original. A crash mid-write leaves the original intact.
        std::filesystem::path tmp_path = path;
        tmp_path += ".migrate.tmp";

        {
            File tmp(tmp_path.c_str());
            if (tmp.open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE))
            {
                printf("[Metadata] Cannot open temp %s for migration\n", tmp_path.c_str());
                return false;
            }

            int64_t written = tmp.write(new_file.data(), static_cast<int64_t>(new_file.size()));
            if (written < 0 or static_cast<std::size_t>(written) != new_file.size())
            {
                printf("[Metadata] Short write during migration (%ld / %zu)\n",
                       static_cast<long>(written), new_file.size());
                tmp.close();
                std::error_code ec;
                std::filesystem::remove(tmp_path, ec);
                return false;
            }
            tmp.sync();
            tmp.close();
        }

        std::error_code ec;
        std::filesystem::rename(tmp_path, path, ec);
        if (ec)
        {
            printf("[Metadata] Rename failed during migration: %s\n", ec.message().c_str());
            std::filesystem::remove(tmp_path, ec);
            return false;
        }

        return true;
    }

}
