#ifndef RTM_PROBE_IO_H
#define RTM_PROBE_IO_H

#include <cstdint>
#include <fstream>

namespace rtm
{
    class AbstractWriteIO
    {
    public:
        virtual ~AbstractWriteIO() = default;

        virtual void write(void const* data, std::size_t data_size) = 0;
        virtual void seek(std::size_t pos) = 0;
    };

    class AbstractReadIO
    {
    public:
        virtual ~AbstractReadIO() = default;

        virtual int64_t read(void* data, std::size_t data_size) = 0;
        virtual void seek(std::size_t pos) = 0;
    };


    class FileWrite final : public AbstractWriteIO
    {
    public:
        FileWrite(char const* filename);
        virtual ~FileWrite() = default;

        void write(void const* data, std::size_t data_size) override;
        void seek(std::size_t pos) override;

    private:
        std::ofstream ofs_;
    };


    class FileRead final : public AbstractReadIO
    {
    public:
        FileRead(char const* filename);
        virtual ~FileRead() = default;

        int64_t read(void* data, std::size_t data_size) override;
        void seek(std::size_t pos) override;

    private:
        std::ifstream ifs_;
    };

    // Write nothing
    class NullWrite final : public AbstractWriteIO
    {
    public:
        NullWrite() = default;
        virtual ~NullWrite() = default;

        void write(void const*, std::size_t) override {}
        void seek(std::size_t) override {}
    };
}

#endif
