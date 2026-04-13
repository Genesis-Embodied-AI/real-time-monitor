#include <algorithm>
#include <thread>
#include <vector>

#include "test_helpers.h"
#include "rtm/io/file.h"

namespace
{

int count_tick_files(fs::path const& dir)
{
    int count = 0;
    for (auto const& entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() == ".tick")
        {
            count++;
        }
    }
    return count;
}

std::vector<fs::path> find_all_tick_files(fs::path const& dir)
{
    std::vector<fs::path> files;
    for (auto const& entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() == ".tick")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void send_probe_data_with_spike(std::unique_ptr<AbstractIO> io,
                                nanoseconds threshold,
                                int spike_at,
                                nanoseconds spike_amount = 50ms)
{
    Probe probe;
    probe.init("test_process", "test_task", START, 1ms, 42, std::move(io));
    probe.set_threshold(threshold);

    nanoseconds offset{0};
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        if (spike_at >= 0 and i == spike_at)
        {
            offset += spike_amount;
        }
        auto t = START + 20ms + nanoseconds(i * 1'000'000) + offset;
        probe.log(t);
        probe.log(t + 100us);
    }
    probe.flush();
}

} // namespace


bool test_blackbox_no_trigger()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_no_trig";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_no.sock").string();

    Recorder recorder(tmp_dir.string(), 5s, 5s);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
            return;
        }
        send_probe_data_with_spike(std::move(io), 100ms, -1);
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    CHECK(count_tick_files(tmp_dir) == 0,
          "expected no .tick files when threshold is not exceeded");

    fs::remove_all(tmp_dir);
    return true;
}


bool test_blackbox_trigger()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_trig";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_trig.sock").string();

    Recorder recorder(tmp_dir.string(), 5s, 5s);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
            return;
        }
        send_probe_data_with_spike(std::move(io), 5ms, 50);
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    auto files = find_all_tick_files(tmp_dir);
    CHECK(files.size() == 1, "expected exactly one .tick file");

    auto io = std::make_unique<File>(files[0].string());
    auto rc = io->open(access::Mode::READ_ONLY);
    CHECK(not rc, "cannot open blackbox .tick file");

    Parser parser(std::move(io));
    parser.load_header();

    CHECK(parser.header().process == "test_process", "wrong process name");
    CHECK(parser.header().name.find("test_task@") == 0, "wrong task name (expected test_task@...)");
    CHECK(parser.header().start_time == START, "wrong start_time in blackbox file");

    bool loaded = parser.load_samples();
    CHECK(loaded, "failed to load samples from blackbox file");
    CHECK(parser.samples().size() % 2 == 0, "samples not pair-aligned");
    CHECK(parser.samples().size() > 0, "no samples in blackbox file");

    std::string filename = files[0].filename().string();
    CHECK(filename.find("test_process") != std::string::npos, "process name not in filename");
    CHECK(filename.find("test_task") != std::string::npos, "task name not in filename");

    fs::remove_all(tmp_dir);
    return true;
}


bool test_blackbox_multiple_triggers()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_multi";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_multi.sock").string();

    Recorder recorder(tmp_dir.string(), 5s, 2s);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
            return;
        }

        Probe probe;
        probe.init("test_process", "test_task", START, 100ms, 42, std::move(io));
        probe.set_threshold(500ms);

        nanoseconds offset{0};
        for (int i = 0; i < NUM_SAMPLES; ++i)
        {
            if (i == 25 or i == 75)
            {
                offset += 5s;
            }
            auto t = START + 20ms + nanoseconds(static_cast<int64_t>(i) * 100'000'000) + offset;
            probe.log(t);
            probe.log(t + 100us);
        }
        probe.flush();
    });

    recorder_loop(recorder, listener, 3s);
    probe_thread.join();

    auto files = find_all_tick_files(tmp_dir);
    CHECK(files.size() == 2, "expected two .tick files for separate triggers");

    for (auto const& f : files)
    {
        auto fio = std::make_unique<File>(f.string());
        auto frc = fio->open(access::Mode::READ_ONLY);
        CHECK(not frc, "cannot open blackbox .tick file");

        Parser p(std::move(fio));
        p.load_header();
        CHECK(p.header().process == "test_process", "wrong process in multi-trigger file");
        CHECK(p.header().start_time == START, "wrong start_time in multi-trigger file");

        bool ok = p.load_samples();
        CHECK(ok, "failed to load samples from multi-trigger file");
        CHECK(p.samples().size() % 2 == 0, "samples not pair-aligned in multi-trigger file");
    }

    fs::remove_all(tmp_dir);
    return true;
}


bool test_blackbox_retrigger_extends()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_retrig";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_retrig.sock").string();

    Recorder recorder(tmp_dir.string(), 5s, 5s);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
            return;
        }

        Probe probe;
        probe.init("test_process", "test_task", START, 1ms, 42, std::move(io));
        probe.set_threshold(5ms);

        nanoseconds offset{0};
        for (int i = 0; i < NUM_SAMPLES; ++i)
        {
            if (i == 30 or i == 60)
            {
                offset += 50ms;
            }
            auto t = START + 20ms + nanoseconds(i * 1'000'000) + offset;
            probe.log(t);
            probe.log(t + 100us);
        }
        probe.flush();
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    auto files = find_all_tick_files(tmp_dir);
    CHECK(files.size() == 1,
          "expected one .tick file when retrigger extends recording");

    auto fio = std::make_unique<File>(files[0].string());
    auto frc = fio->open(access::Mode::READ_ONLY);
    CHECK(not frc, "cannot open retrigger .tick file");

    Parser p(std::move(fio));
    p.load_header();
    bool ok = p.load_samples();
    CHECK(ok, "failed to load samples from retrigger file");
    CHECK(p.samples().size() % 2 == 0, "samples not pair-aligned in retrigger file");

    fs::remove_all(tmp_dir);
    return true;
}


bool test_blackbox_backward_compat()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_compat";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_compat.sock").string();

    Recorder recorder(tmp_dir.string(), 2min, 2min);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
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


bool test_blackbox_file_header()
{
    auto tmp_dir = fs::temp_directory_path() / "rtm_test_bb_header";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);
    std::string sock_path = (fs::temp_directory_path() / "rtm_bb_header.sock").string();

    Recorder recorder(tmp_dir.string(), 5s, 5s);
    LocalListener listener(sock_path);
    {
        auto rc = listener.listen(1);
        CHECK(not rc, "listen failed");
    }

    std::thread probe_thread([&sock_path]()
    {
        sleep(50ms);
        auto io = std::make_unique<LocalSocket>(sock_path);
        if (io->open(access::Mode::READ_WRITE))
        {
            printf("  connect failed\n");
            return;
        }
        send_probe_data_with_spike(std::move(io), 5ms, 50);
    });

    recorder_loop(recorder, listener, 2s);
    probe_thread.join();

    auto files = find_all_tick_files(tmp_dir);
    CHECK(files.size() == 1, "expected one .tick file");

    auto io = std::make_unique<File>(files[0].string());
    auto rc = io->open(access::Mode::READ_ONLY);
    CHECK(not rc, "cannot open blackbox .tick file");

    Parser parser(std::move(io));
    parser.load_header();

    CHECK(parser.header().major == 2, "wrong protocol major version");
    CHECK(parser.header().process == "test_process", "wrong process name");
    CHECK(parser.header().name.find("test_task@") == 0, "wrong task name (expected test_task@...)");
    CHECK(parser.header().start_time == START, "wrong start_time");

    bool loaded = parser.load_samples();
    CHECK(loaded, "failed to load samples");

    auto times_diff = parser.generate_times_diff();
    CHECK(not times_diff.empty(), "no times_diff entries");

    bool spike_found = false;
    for (auto const& p : times_diff)
    {
        if (p.y > 5.0)
        {
            spike_found = true;
            break;
        }
    }
    CHECK(spike_found, "spike not found in generate_times_diff output");

    auto times_up = parser.generate_times_up();
    CHECK(not times_up.empty(), "no times_up entries");

    fs::remove_all(tmp_dir);
    return true;
}
