#include <implot.h>

#include "plot.h"

namespace rtm
{
    void Plot::load_dataset(std::filesystem::path const& folder)
    {
        for (auto const& entry : std::filesystem::directory_iterator(folder))
        {
            if (entry.is_regular_file())
            {
                if (entry.path().extension() == ".tick")
                {
                    auto io = std::make_unique<rtm::FileRead>(entry.path().string().c_str());
                    Parser p{std::move(io)};
                    p.load_header();
                    p.load_samples();

                    Serie s{p};
                    series_.emplace_back(std::move(s));
                }
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
            draw_graphs("Times Diff", request_fit,
                        [](const Serie& serie) { return serie.plot_diffs(); });

            draw_graphs("Times Up", request_fit,
                        [](const Serie& serie) { return serie.plot_ups(); });

            ImGui::EndTabBar();
        }
    }

    void Plot::draw_graphs(char const* tab_name, bool request_fit,
                           std::function<bool(const Serie&)> plot_func)
    {
        if (request_fit)
        {
            ImPlot::SetNextAxesToFit();
        }

        if (ImGui::BeginTabItem(tab_name))
        {
            // Plot area
            ImGui::BeginChild("PlotRegion", ImVec2(0, -28), true,
                            ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoScrollWithMouse);

            bool is_downsampled = false;
            if (ImPlot::BeginPlot("##Plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs))
            {
                for (auto const& serie : series_)
                {
                    is_downsampled |= plot_func(serie);
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
