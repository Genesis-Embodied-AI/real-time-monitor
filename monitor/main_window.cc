#include "main_window.h"

namespace rtm
{
    void MainWindow::load_dataset(std::filesystem::path const& folder)
    {
        std::vector<std::string> detected_files;
        for (auto const& entry : std::filesystem::directory_iterator(folder))
        {
            if (entry.is_regular_file())
            {
                if (entry.path().extension() == ".tick")
                {
                    detected_files.push_back(entry.path().string());
                }
            }
        }
        std::sort(detected_files.begin(), detected_files.end());

        for (auto const& entry : detected_files)
        {
            auto io = std::make_unique<rtm::FileRead>(entry.c_str());
            Parser p{std::move(io)};
            p.load_header();
            p.load_samples();
            auto color = generate_random_color();

            {
                // diff times
                Serie s{p.header(), p.generate_times_diff(), color};
                diff_.add_serie(std::move(s), p.diff_max(), p.end());
            }

            {
                // up times
                Serie s{p.header(), p.generate_times_up(), color};
                up_.add_serie(std::move(s), p.up_max(), p.end());
            }
        }
    }

    void MainWindow::draw()
    {
        if (ImGui::BeginTabBar("GraphsTabBar"))
        {
            diff_.draw();
            up_.draw();
            ImGui::EndTabBar();
        }
    }

}
