#ifndef RTM_LIB_IO_IO_H
#define RTM_LIB_IO_IO_H

#include <cstdint>

#include "rtm/error.h"

namespace rtm
{
    namespace access
    {
        enum Mode
        {
            CLOSE           = 0x000,
            READ_ONLY       = 0x001,
            WRITE_ONLY      = 0x002,
            READ_WRITE      = 0x004,
            APPEND          = 0x008,
            TRUNCATE        = 0x010,
            UNBUFFERED      = 0x020,
            NEW_ONLY        = 0x040,
            EXISTING_ONLY   = 0x080,
            NON_BLOCKING    = 0x100,
        };

        constexpr enum Mode operator |  (Mode const& lhs, Mode const& rhs) { return static_cast<enum Mode>(static_cast<int>(lhs) | static_cast<int>(rhs)); }
        constexpr enum Mode operator &  (Mode const& lhs, Mode const& rhs) { return static_cast<enum Mode>(static_cast<int>(lhs) & static_cast<int>(rhs)); }
        constexpr enum Mode operator |= (Mode& lhs, Mode const& rhs)       { return lhs = lhs | rhs; }
        constexpr enum Mode operator &= (Mode& lhs, Mode const& rhs)       { return lhs = lhs & rhs; }
        constexpr enum Mode operator ~(Mode mode)                          { int tmp_mode = ~static_cast<int>(mode); return static_cast<Mode>(tmp_mode); }

        // Helpers to ease common manipulations
        constexpr bool is_writable(access::Mode modes) { return (modes & access::Mode::WRITE_ONLY) or (modes & access::Mode::READ_WRITE); }
        constexpr bool is_readable(access::Mode modes) { return (modes & access::Mode::READ_ONLY)  or (modes & access::Mode::READ_WRITE); }
        constexpr bool is_open(access::Mode modes)     { return (modes != access::Mode::CLOSE); }
        constexpr bool is_blocking(access::Mode modes) { return not (modes & access::Mode::NON_BLOCKING); }
    }

    class AbstractIO
    {
    public:
        AbstractIO() = default;
        virtual ~AbstractIO() = default;

        std::error_code open(access::Mode access);
        std::error_code close();

        bool is_open() const;
        bool is_readable() const;
        bool is_writable() const;
        bool is_blocking() const;

        virtual int64_t read(void* data, int64_t data_size) = 0;
        virtual void write(void const* data, int64_t data_size) = 0;
        virtual std::error_code seek(int64_t pos);

    protected:
        virtual std::error_code do_open(access::Mode mode) = 0;
        virtual std::error_code do_close() = 0;

        access::Mode modes_{access::Mode::CLOSE};               ///< Current mode.
        access::Mode supported_modes_{access::Mode::CLOSE};     ///< Supported modes of the device.
    };
}

#endif
