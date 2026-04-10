#include <cstring>

#include "parser.h"

namespace rtm
{
    // Absolute cap on a single metadata payload. Metadata entries are small
    // (display name, weight, visibility). Anything past this is corruption.
    constexpr uint32_t MAX_PAYLOAD_SIZE = 64 * 1024;

    void Parser::load_metadata()
    {
        if (header_.metadata_footer_offset <= 0)
        {
            return;
        }

        io_->seek(header_.metadata_footer_offset);

        uint32_t entry_count = 0;
        if (io_->read(&entry_count, sizeof(entry_count)) != sizeof(entry_count))
        {
            printf("[Metadata] Truncated footer: missing entry_count\n");
            return;
        }

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint16_t key_id = 0;
            uint32_t payload_size = 0;
            if (io_->read(&key_id, sizeof(key_id)) != sizeof(key_id) or
                io_->read(&payload_size, sizeof(payload_size)) != sizeof(payload_size))
            {
                printf("[Metadata] Truncated footer at entry %u\n", i);
                return;
            }

            if (payload_size > MAX_PAYLOAD_SIZE)
            {
                printf("[Metadata] Payload size %u exceeds cap (%u) at entry %u — "
                       "footer is likely corrupt, aborting.\n",
                       payload_size, MAX_PAYLOAD_SIZE, i);
                return;
            }

            std::vector<char> buf(payload_size);
            if (io_->read(buf.data(), payload_size) != static_cast<int64_t>(payload_size))
            {
                printf("[Metadata] Truncated payload at entry %u\n", i);
                return;
            }

            switch (key_id)
            {
                case MetadataKey::DISPLAY_NAME:
                {
                    metadata_.display_name.assign(buf.begin(), buf.end());
                    break;
                }
                case MetadataKey::DEFAULT_VISIBILITY:
                {
                    if (payload_size >= 1)
                    {
                        metadata_.default_visibility = static_cast<uint8_t>(buf[0]);
                    }
                    break;
                }
                case MetadataKey::DISPLAY_WEIGHT:
                {
                    if (payload_size >= sizeof(int32_t))
                    {
                        std::memcpy(&metadata_.display_weight, buf.data(), sizeof(int32_t));
                    }
                    break;
                }
                default:
                {
                    printf("[Metadata] Unknown key id: %u\n", key_id);
                    break;
                }
            }
        }
    }
}
