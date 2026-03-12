#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>

#include "rtm/probe.h"
#include "rtm/recorder.h"
#include "rtm/parser.h"
#include "rtm/io/file.h"
#include "rtm/io/null.h"
#include "rtm/io/posix/local_socket.h"
#include "rtm/io/posix/tcp_socket.h"
#include "rtm/os/time.h"

namespace fs = std::filesystem;
using namespace rtm;
using namespace std::chrono;

namespace
{

constexpr int NUM_SAMPLES = 100;
constexpr nanoseconds START = 8'000'000s;
constexpr uint16_t TCP_TEST_PORT = 19770;

int test_failures = 0;

#define CHECK(cond, msg) do                                         \
{                                                                   \
    if (not (cond))                                                 \
    {                                                               \
        printf("  FAIL: %s (line %d)\n", (msg), __LINE__);         \
        return false;                                               \
    }                                                               \
} while(0)


void send_probe_data(std::unique_ptr<AbstractIO> io)
{
    Probe probe;
    probe.init("test_process", "test_task", START, 1ms, 42, std::move(io));

    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        auto t = START + 20ms + nanoseconds(i * 1'000'000);
        probe.log(t);
        probe.log(t + 100us);
    }
    probe.flush();
}


fs::path find_tick_file(fs::path const& dir)
{
    for (auto const& entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() == ".tick")
        {
            return entry.path();
        }
    }
    return {};
}


bool verify_tick_file(fs::path const& dir)
{
    auto tick_file = find_tick_file(dir);
    CHECK(not tick_file.empty(), "no .tick file found");
    CHECK(fs::file_size(tick_file) > 64, ".tick file too small");

    auto io = std::make_unique<File>(tick_file.string());
    auto rc = io->open(access::Mode::READ_ONLY);
    CHECK(not rc, "cannot open .tick file for reading");

    Parser parser(std::move(io));
    parser.load_header();

    CHECK(parser.header().process == "test_process", "wrong process name");
    CHECK(parser.header().name == "test_task", "wrong task name");
    CHECK(parser.header().version == 1, "wrong protocol version");

    bool loaded = parser.load_samples();
    CHECK(loaded, "failed to load samples");

    auto const& samples = parser.samples();
    CHECK(samples.size() == 200, "unexpected sample count");

    CHECK(samples[0] == 20ms, "wrong sample[0]: expected 20ms (reference)");
    CHECK(samples[1] == 20100us, "wrong sample[1]: expected 20.1ms");
    CHECK(samples[2] == 21ms, "wrong sample[2]: expected 21ms");
    CHECK(samples[3] == 21100us, "wrong sample[3]: expected 21.1ms");

    // Last pair: i=99 → t = START + 119ms
    CHECK(samples[198] == 119ms, "wrong sample[198]: expected 119ms");
    CHECK(samples[199] == 119100us, "wrong sample[199]: expected 119.1ms");

    return true;
}


// -- Test 1: Probe -> File -> Parser round-trip (no network) --

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


// -- Helper: run the recorder accept+process loop for stream-based transports --

void recorder_loop(Recorder& recorder, AbstractListener& listener, nanoseconds timeout)
{
    auto deadline = since_epoch() + timeout;
    while (since_epoch() < deadline)
    {
        auto io = listener.accept(access::Mode::NON_BLOCKING);
        if (io != nullptr)
        {
            recorder.add_client(std::move(io));
        }
        recorder.process();
        sleep(1ms);
    }
}


// -- Test 2: Local (Unix) socket transport --

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


// -- Test 3: TCP transport --

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


// -- Test 4: Valid header but no logged samples --

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


// -- Test 5: File truncated mid-data --

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


// -- Test 6: Completely corrupted file (random garbage) --

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
        // No value checks: we only verify the parser does not crash
    }

    fs::remove_all(tmp_dir);
    return true;
}

} // namespace


// -- Main --

int main()
{
    struct TestCase
    {
        char const* name;
        bool (*fn)();
    };

    TestCase tests[] =
    {
        {"file_sink",        test_file_sink},
        {"local_socket",     test_local_socket},
        {"tcp",              test_tcp},
        {"empty_data",       test_empty_data},
        {"truncated_data",   test_truncated_data},
        {"corrupted_data",   test_corrupted_data},
    };

    for (auto& [name, fn] : tests)
    {
        printf("[test] %s\n", name);
        if (not fn())
        {
            printf("  FAIL\n");
            test_failures++;
        }
        else
        {
            printf("  PASS\n");
        }
    }

    printf("\n%d test(s) failed\n", test_failures);
    if (test_failures == 0)
    {
        return 0;
    }
    return 1;
}
