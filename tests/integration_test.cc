#include <cstdlib>
#include <thread>

#include "test_helpers.h"
#include "rtm/io/factory.h"
#include "rtm/probe_factory.h"
#include "rtm/io/file.h"
#include "rtm/io/null.h"
#include "rtm/io/posix/tcp_socket.h"

namespace
{
constexpr uint16_t TCP_TEST_PORT = 19770;
constexpr uint16_t TCP_UNBOUND_PORT = 19772;
constexpr uint16_t UDP_TEST_PORT = 19773;
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
        CHECK(parser.header().major == 2, "wrong protocol major version");

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


bool test_io_factory()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_factory";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);

    // The discard sink takes a whole stream with nowhere to put it.
    {
        auto io = make_null_io();
        CHECK(not io->open(access::Mode::READ_WRITE), "cannot open the discard sink");
        send_probe_data(std::move(io));
    }

    // The file sink writes a stream the parser reads back.
    {
        auto const tick_path = (tmp_dir / "factory.tick").string();
        auto io = make_file_io(tick_path);
        CHECK(not io->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE),
              "cannot open the file sink");
        send_probe_data(std::move(io));
    }
    CHECK(verify_tick_file(tmp_dir), "file sink stream does not parse");

    // A local socket dials the path it is given, so an absent recorder is reported from there
    // and not from the default path.
    {
        auto const dead_path = (tmp_dir / "absent.sock").string();
        auto io = make_local_socket_io(dead_path);
        CHECK(io->open(access::Mode::READ_WRITE), "connecting to an absent recorder should fail");
    }

    // A TCP socket carries the host and port it is given: nothing listens on this one.
    {
        auto io = make_tcp_io("127.0.0.1", TCP_UNBOUND_PORT);
        CHECK(io->open(access::Mode::READ_WRITE), "connecting to an unbound port should fail");
    }

    // The two UDP forms address each other: `bind_port` receives, `host` and `port` send.
    {
        auto receiver = make_udp_io("", 0, UDP_TEST_PORT);
        CHECK(not receiver->open(access::Mode::READ_WRITE), "cannot bind the UDP receiver");

        auto sender = make_udp_io("127.0.0.1", UDP_TEST_PORT);
        CHECK(not sender->open(access::Mode::READ_WRITE), "cannot open the UDP sender");

        constexpr int64_t payload_size = static_cast<int64_t>(sizeof(uint32_t));
        uint32_t const sent = 0xA5A5A5A5;
        CHECK(sender->write(&sent, payload_size) == payload_size, "short UDP write");

        uint32_t received = 0;
        CHECK(receiver->read(&received, payload_size) == payload_size, "short UDP read");
        CHECK(received == sent, "UDP payload does not round-trip");
    }

    fs::remove_all(tmp_dir);
    return true;
}


bool test_connect_probe()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_connect";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (tmp_dir / "recorder.sock").string();

    // No recorder on the path: the failure is reported and the probe is still usable.
    {
        Probe probe;
        auto rc = connect_probe(probe, "test_process", "test_task", START, 1ms, 42, sock_path);
        CHECK(rc, "connecting to an absent recorder should report an error");

        probe.set_threshold(10ms);
        log_probe_samples(probe);
    }

    Recorder recorder(tmp_dir.string());
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "local listen() failed");
    }

    bool connected = false;
    std::thread probe_thread([&sock_path, &connected]()
    {
        sleep(50ms);
        Probe probe;
        auto rc = connect_probe(probe, "test_process", "test_task", START, 1ms, 42, sock_path);
        if (rc)
        {
            printf("  connect_probe() failed: %s\n", rc.message().c_str());
            return;
        }
        connected = true;
        log_probe_samples(probe);
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    CHECK(connected, "connect_probe() failed against a live listener");

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
