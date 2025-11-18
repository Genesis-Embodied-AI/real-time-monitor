#ifndef RTM_LIB_SERIALIZER_H
#define RTM_LIB_SERIALIZER_H

#include <concepts>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace rtm
{
    // Note: standard layout is not mandatory but gere we want to ensure predictable layout
    template <typename T>
    concept memcpy_compatible = std::is_trivially_copyable_v<T> and std::is_standard_layout_v<T>;

    // Helper to extract data from memory
    template <memcpy_compatible T>
    T extract_data(uint8_t const*& pos)
    {
        T data;
        std::memcpy(&data, pos, sizeof(T));
        pos += sizeof(T);

        return data;
    }


    // Helpers to append data to a std::vector<uint8_t>
    template <typename T>
    void append(std::vector<uint8_t>& buffer,  T const& value)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                    "T must be trivially copyable");

        uint8_t const* data = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), data, data + sizeof(T));
    }

    template<> void append(std::vector<uint8_t>& buffer, std::string_view const& value);
}

#endif
