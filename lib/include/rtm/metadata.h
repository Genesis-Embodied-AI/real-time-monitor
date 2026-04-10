#ifndef RTM_LIB_METADATA_H
#define RTM_LIB_METADATA_H

#include <cstdint>
#include <filesystem>
#include <string>

#include "rtm/io/io.h"

namespace rtm
{
    enum MetadataKey : uint16_t
    {
        DISPLAY_NAME        = 1,
        DEFAULT_VISIBILITY  = 2,
        DISPLAY_WEIGHT      = 3,
        USER_INFO           = 0xFFFF,
    };

    struct TickHeader;

    struct TickMetadata
    {
        std::string display_name;
        uint8_t default_visibility = 1; // 0 = hidden, 1 = visible
        int32_t display_weight = 0;
    };

    void save_metadata(AbstractIO& io, TickHeader const& header, TickMetadata const& metadata);
    void repair_sentinel(AbstractIO& io, int64_t eof_pos);

    // Migrate a v1 file in place to v2. Writes to a temp file and renames
    // atomically so a failed write cannot destroy the original.
    bool migrate_v1_to_v2(std::filesystem::path const& path);
}

#endif
