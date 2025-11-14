#include "serializer.h"

namespace rtm
{
    template<>
    void append(std::vector<uint8_t>& buffer, std::string_view const& str)
    {
        uint8_t const* data = reinterpret_cast<const uint8_t*>(str.data());
        buffer.insert(buffer.end(), data, data + str.size());
    }
}
