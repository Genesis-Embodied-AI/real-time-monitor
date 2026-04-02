#include <algorithm>

#include "rtm/parser.h"
#include "rtm/io/file.h"

#include "main_window.h"

namespace rtm
{
    void MainWindow::load_dataset(std::vector<std::filesystem::path> const& inputs)
    {
        std::vector<std::string> detected_files;
        for (auto const& input : inputs)
        {
            if (std::filesystem::is_directory(input))
            {
                for (auto const& entry : std::filesystem::directory_iterator(input))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".tick")
                        detected_files.push_back(entry.path().string());
                }
            }
            else if (std::filesystem::is_regular_file(input) && input.extension() == ".tick")
            {
                detected_files.push_back(input.string());
            }
        }
        std::sort(detected_files.begin(), detected_files.end());
        detected_files.erase(std::unique(detected_files.begin(), detected_files.end()), detected_files.end());

        for (auto const& entry : detected_files)
        {
            auto io = std::make_unique<rtm::File>(entry.c_str());
            io->open(access::Mode::READ_ONLY);
            Parser p{std::move(io)};
            p.load_header();
            p.print_header();
            if (not p.load_samples())
            {
                printf("Empty file, skipping.\n");
                continue;
            }
            auto color = generate_random_color();

            std::string name;
            name = p.header().process;
            name += '.';
            name += p.header().name;
            name += " (";
            name += format_iso_timestamp(p.header().start_time);
            name += ')';

            {
                // diff times
                Serie s{name, p.generate_times_diff(), color};
                diff_.add_serie(std::move(s), p.diff_min(), p.diff_max(), p.begin(), p.end());
            }

            {
                // up times
                Serie s{name, p.generate_times_up(), color};
                up_.add_serie(std::move(s), p.up_min(), p.up_max(), p.begin(), p.end());
            }
        }
    }

    void MainWindow::draw()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::MenuItem("Software anti-aliasing", nullptr,
                    &ImGui::GetStyle().AntiAliasedLines);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("GraphsTabBar"))
        {
            diff_.draw();
            up_.draw();
            ImGui::EndTabBar();
        }
    }

}
