#ifndef RTM_LIB_IO_FILE_H
#define RTM_LIB_IO_FILE_H

#include <string>
#include <string_view>

#include "rtm/io/io.h"
#include "rtm/os/types.h"

namespace rtm
{
    class File final : public AbstractIO
    {
    public:
        File(std::string_view filename);
        virtual ~File();

        int64_t read(void* data, int64_t data_size) override;
        int64_t write(void const* data, int64_t data_size) override;
        std::error_code seek(int64_t pos) override;
        std::error_code sync() override;

    private:
        std::error_code do_open(access::Mode) override;
        std::error_code do_close() override;

        os_file fd_;
        std::string filename_;
    };
}

#endif
