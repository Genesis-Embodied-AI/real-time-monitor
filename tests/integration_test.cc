#include <cstdlib>
#include <thread>

#include "test_helpers.h"
#include "rtm/io/file.h"
#include "rtm/io/null.h"
#include "rtm/io/posix/tcp_socket.h"

namespace
{
constexpr uint16_t TCP_TEST_PORT = 19770;
}


bool test_file_sink()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_file";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    auto tick_path = tmp_dir / "test.tick";

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);
        CHECK(not rc, "cannot open file for writing");

        send_probe_data(std::move(io));
    }

    CHECK(fs::exists(tick_path), "tick file not created");
    CHECK(fs::file_size(tick_path) > 64, "tick file too small");

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::READ_ONLY);
        CHECK(not rc, "cannot open file for reading");

        Parser parser(std::move(io));
        parser.load_header();

        CHECK(parser.header().process == "test_process", "wrong process name");
        CHECK(parser.header().name == "test_task", "wrong task name");
        CHECK(parser.header().version == 1, "wrong protocol version");

        bool loaded = parser.load_samples();
        CHECK(loaded, "failed to load samples");
        CHECK(parser.samples().size() >= 10, "too few samples parsed");
    }

    fs::remove_all(tmp_dir);
    return true;
}


bool test_local_socket()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_local";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_test.sock").string();

    Recorder recorder(tmp_dir.string());
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "local listen() failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  probe connect failed\n");
            return;
        }
        send_probe_data(std::move(io));
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    bool ok = verify_tick_file(tmp_dir);
    fs::remove_all(tmp_dir);
    return ok;
}


bool test_tcp()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_tcp";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);

    Recorder recorder(tmp_dir.string());
    TcpListener listener("", TCP_TEST_PORT);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "TCP listen() failed");
    }

    std::thread probe_thread([]()
    {
        sleep(50ms);
        auto io = std::make_unique<TcpSocket>("127.0.0.1", TCP_TEST_PORT);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  probe TCP connect failed\n");
            return;
        }
        send_probe_data(std::move(io));
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    bool ok = verify_tick_file(tmp_dir);
    fs::remove_all(tmp_dir);
    return ok;
}


bool test_empty_data()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_empty";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    auto tick_path = tmp_dir / "empty.tick";

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);
        CHECK(not rc, "cannot open file for writing");

        Probe probe;
        probe.init("test_process", "test_task", START, 1ms, 42, std::move(io));
        probe.flush();
    }

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::READ_ONLY);
        CHECK(not rc, "cannot open file for reading");

        Parser parser(std::move(io));
        parser.load_header();

        CHECK(parser.header().process == "test_process", "wrong process name");
        CHECK(parser.header().name == "test_task", "wrong task name");

        bool loaded = parser.load_samples();
        CHECK(not loaded, "expected load_samples to return false for empty data");
        CHECK(parser.samples().empty(), "expected no samples");
    }

    fs::remove_all(tmp_dir);
    return true;
}


bool test_truncated_data()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_trunc";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    auto tick_path = tmp_dir / "truncated.tick";

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);
        CHECK(not rc, "cannot open file for writing");

        send_probe_data(std::move(io));
    }

    auto full_size = fs::file_size(tick_path);
    fs::resize_file(tick_path, full_size / 2);

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::READ_ONLY);
        CHECK(not rc, "cannot open truncated file for reading");

        Parser parser(std::move(io));
        parser.load_header();

        bool loaded = parser.load_samples();
        if (loaded)
        {
            CHECK(parser.samples().size() > 0, "should have parsed some samples");
            CHECK(parser.samples().size() < 200, "truncated file should have fewer samples");
        }
    }

    fs::remove_all(tmp_dir);
    return true;
}


bool test_corrupted_data()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_corrupt";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    auto tick_path = tmp_dir / "corrupted.tick";

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);
        CHECK(not rc, "cannot create corrupted file");

        uint8_t garbage[256];
        for (int i = 0; i < 256; ++i)
        {
            garbage[i] = static_cast<uint8_t>(rand());
        }
        io->write(garbage, sizeof(garbage));
    }

    {
        auto io = std::make_unique<File>(tick_path.string());
        auto rc = io->open(access::Mode::READ_ONLY);
        CHECK(not rc, "cannot open corrupted file");

        Parser parser(std::move(io));
        parser.load_header();
        parser.load_samples();
    }

    fs::remove_all(tmp_dir);
    return true;
}
