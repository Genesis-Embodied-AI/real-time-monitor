#ifndef RTM_TEST_HELPERS_H
#define RTM_TEST_HELPERS_H

#include <cstdio>
#include <filesystem>
#include <memory>

#include "rtm/probe.h"
#include "rtm/recorder.h"
#include "rtm/parser.h"
#include "rtm/io/file.h"
#include "rtm/io/io.h"
#include "rtm/io/posix/local_socket.h"
#include "rtm/os/time.h"

namespace fs = std::filesystem;
using namespace rtm;
using namespace std::chrono;

constexpr int NUM_SAMPLES = 100;
constexpr nanoseconds START = 8'000'000s;

#define CHECK(cond, msg) do                                         \
{                                                                   \
    if (not (cond))                                                 \
    {                                                               \
        printf("  FAIL: %s (line %d)\n", (msg), __LINE__);         \
        return false;                                               \
    }                                                               \
} while(0)

struct TestCase
{
    char const* name;
    bool (*fn)();
};

inline int run_tests(TestCase* tests, int count)
{
    int failures = 0;
    for (int i = 0; i < count; ++i)
    {
        printf("[test] %s\n", tests[i].name);
        if (not tests[i].fn())
        {
            printf("  FAIL\n");
            failures++;
        }
        else
        {
            printf("  PASS\n");
        }
    }

    printf("\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}

inline void send_probe_data(std::unique_ptr<AbstractIO> io)
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

inline void recorder_loop(Recorder& recorder, AbstractListener& listener, nanoseconds timeout)
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

inline fs::path find_tick_file(fs::path const& dir)
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

inline bool verify_tick_file(fs::path const& dir)
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
    CHECK(parser.header().major == 2, "wrong protocol major version");

    bool loaded = parser.load_samples();
    CHECK(loaded, "failed to load samples");

    auto const& samples = parser.samples();
    CHECK(samples.size() == 200, "unexpected sample count");

    CHECK(samples[0] == 20ms, "wrong sample[0]: expected 20ms (reference)");
    CHECK(samples[1] == 20100us, "wrong sample[1]: expected 20.1ms");
    CHECK(samples[2] == 21ms, "wrong sample[2]: expected 21ms");
    CHECK(samples[3] == 21100us, "wrong sample[3]: expected 21.1ms");

    CHECK(samples[198] == 119ms, "wrong sample[198]: expected 119ms");
    CHECK(samples[199] == 119100us, "wrong sample[199]: expected 119.1ms");

    return true;
}

#endif
