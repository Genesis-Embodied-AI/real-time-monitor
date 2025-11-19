#include <implot.h>

#include "plot.h"

namespace rtm
{
    void Plot::load_dataset(std::filesystem::path const& folder)
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

            // determine the end of the whole graph
            end_ = std::max(end_, p.end());

            {
                // diff times
                Serie s{p.header(), p.generate_times_diff(), color};
                diffs_.emplace_back(std::move(s));
                diff_max_ = std::max(diff_max_, p.diff_max());
            }

            {
                // up times
                Serie s{p.header(), p.generate_times_up(), color};
                ups_.emplace_back(std::move(s));
                up_max_ = std::max(up_max_, p.up_max());
            }
        }
    }

    void Plot::draw()
    {
        bool request_fit = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            request_fit = true;
        }

        if (ImGui::BeginTabBar("GraphsTabBar"))
        {
            draw_graphs("Times Diff", "jitter (ms)", diff_max_.count(), request_fit, diffs_);

            draw_graphs("Times Up", "up time (ms)", up_max_.count(), request_fit, ups_);

            ImGui::EndTabBar();
        }
    }

    void Plot::draw_graphs(char const* tab_name, char const* legend, float max_y, bool request_fit, std::vector<Serie> const& series)
    {
        if (ImGui::BeginTabItem(tab_name))
        {
            // Force a start with the full view
            ImPlot::SetNextAxesLimits(0, seconds_f(end_).count(),
                                    0, max_y,
                                    ImPlotCond_Once);

            if (request_fit)
            {
                ImPlot::SetNextAxesLimits(0, seconds_f(end_).count(),
                                        0, max_y,
                                        ImPlotCond_Always);

            }

            // Plot area
            ImGui::BeginChild("PlotRegion", ImVec2(0, -28), true,
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);

            bool is_downsampled = false;
            if (ImPlot::BeginPlot("##Plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs))
            {
                ImPlot::SetupAxes("time (s)", legend);
                for (auto const& serie : series)
                {
                    is_downsampled |= serie.plot();
                }
                ImPlot::EndPlot();
            }

            ImGui::EndChild();
            draw_status_bar(is_downsampled);
            ImGui::EndTabItem();
        }
    }

    void Plot::draw_status_bar(bool is_downsampled)
    {
        ImGui::Separator();                    // Divider line above status bar
        ImGui::BeginChild("StatusBar", ImVec2(0, 24), false,
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Status:");
        ImGui::SameLine();

        if (is_downsampled)
        {
            ImVec4 orange = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
            ImGui::ColorButton("##ds_col", orange,
                            ImGuiColorEditFlags_NoTooltip |
                            ImGuiColorEditFlags_NoBorder);
            ImGui::SameLine();
            ImGui::TextColored(orange, "Downsampled");
        }
        else
        {
            ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            ImGui::ColorButton("##normal_col", green,
                           ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoBorder);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Normal");
        }

        ImGui::EndChild();
    }
}
