#include "io.h"

namespace rtm
{
    FileWrite::FileWrite(char const* filename)
        : ofs_{filename, std::ios::out | std::ios::binary}
    {

    }

    void FileWrite::write(void const* data, std::size_t data_size)
    {
        ofs_.write(reinterpret_cast<char const*>(data), data_size);
    }

    void FileWrite::seek(std::size_t pos)
    {
        ofs_.seekp(pos, std::ios::beg);
    }


    FileRead::FileRead(char const* filename)
        : ifs_{filename, std::ios::in | std::ios::binary}
    {

    }

    int64_t FileRead::read(void* data, std::size_t data_size)
    {
        ifs_.read(reinterpret_cast<char*>(data), data_size);
        return ifs_.gcount();
    }

    void FileRead::seek(std::size_t pos)
    {
        ifs_.seekg(pos, std::ios::beg);
    }
}
