#include <implot.h>

#include "plot.h"

namespace rtm
{
    int time_formatter(double value, char* buff, int size, void* user_data)
    {
        double to_seconds = *static_cast<double*>(user_data);
        value *= to_seconds;
        double abs_value = std::abs(value);
        int written = 0;

        if (abs_value >= 31536000)  // Years (365 days)
        {
            written = snprintf(buff, size, "%.2fy", value / 31536000);
        }
        else if (abs_value >= 2592000)  // Months (30 days)
        {
            written = snprintf(buff, size, "%.1fmo", value / 2592000);
        }
        else if (abs_value >= 86400)  // Days
        {
            written = snprintf(buff, size, "%.1fd", value / 86400);
        }
        else if (abs_value >= 3600)  // Hours
        {
            written = snprintf(buff, size, "%.1fh", value / 3600);
        }
        else if (abs_value >= 60)  // Minutes
        {
            written = snprintf(buff, size, "%.1fmin", value / 60);
        }
        else if (abs_value >= 1)  // Seconds
        {
            written = snprintf(buff, size, "%.2fs", value);
        }
        else if (abs_value >= 1e-3)  // Milliseconds
        {
            written = snprintf(buff, size, "%.2fms", value * 1e3);
        }
        else if (abs_value >= 1e-6)  // Microseconds
        {
            written = snprintf(buff, size, "%.2fus", value * 1e6);
        }
        else  // Nanoseconds
        {
            written = snprintf(buff, size, "%.2fns", value * 1e9);
        }

        return written;
    }

    Plot::Plot(std::string const& name, std::string const& legend)
        : name_{name}
        , legend_{legend}
    {

    }


    void Plot::add_serie(Serie&& s, milliseconds_f max_y, nanoseconds begin, nanoseconds end)
    {
        series_.emplace_back(std::move(s));
        max_y_ = std::max(max_y_, max_y);
        if (begin_ < 0ns)
        {
            begin_ = begin;
        }
        else
        {
            begin_ = std::min(begin_, begin);
        }
        end_   = std::max(end_, end);
    }


    void Plot::draw_stats_panel()
    {
        // Handle keyboard toggle (like Ctrl+B)
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) and ImGui::IsKeyPressed(ImGuiKey_B))
        {
            show_stats_ = not show_stats_;
        }

        // Get the full available height at the start
        float available_height = ImGui::GetContentRegionAvail().y;

        if (show_stats_)
        {
            ImGui::BeginChild("Sidebar", ImVec2(stats_bar_width, available_height), true);
            ImGui::Text("Statistics");
            ImGui::Separator();

            // Display stats for each series
            for (std::size_t i = 0; i < series_.size() and i < stats_.size(); ++i)
            {
                Statistics const& stats = stats_[i];
                Serie const& serie = series_[i];

                // Create unique ID by combining name with index
                std::string unique_id = serie.name() + "##" + std::to_string(i);

                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNode(unique_id.c_str()))
                {
                    ImGui::Text("Min:     %.3f", stats.min);
                    ImGui::Text("Max:     %.3f", stats.max);
                    ImGui::Text("Average: %.3f", stats.average);
                    ImGui::Text("RMS:     %.3f", stats.rms);
                    ImGui::Text("Std Dev: %.3f", stats.standard_deviation);

                    ImGui::TreePop();
                }
            }

            if (series_.empty())
            {
                ImGui::TextDisabled("No series data available");
            }

            ImGui::EndChild();

            // Splitter with toggle button (full height)
            ImGui::SameLine();

            // Toggle button taking full height
            ImGui::Button("<", ImVec2(20, available_height));

            bool is_active  = ImGui::IsItemActive();
            bool is_clicked = ImGui::IsItemClicked();

            // Handle dragging for resize
            if (is_active and ImGui::GetIO().MouseDelta.x != 0.0f)
            {
                was_dragging_stats_bar = true;
                stats_bar_width += ImGui::GetIO().MouseDelta.x;
            }

            // Handle click to toggle (only if we didn't just drag)
            if (is_clicked)
            {
                was_dragging_stats_bar = false;  // Reset on new click
            }
            else if (not is_active and was_dragging_stats_bar)
            {
                // Just released after dragging, don't toggle
                was_dragging_stats_bar = false;
            }
            else if (not is_active and not was_dragging_stats_bar)
            {
                // Released without dragging, toggle the sidebar
                if (ImGui::IsItemDeactivated())
                {
                    show_stats_ = false;
                }
            }

            ImGui::SameLine();
        }
        else
        {
            // Show button when sidebar is hidden (full height)
            if (ImGui::Button(">", ImVec2(20, available_height)))
            {
                show_stats_ = true;
            }
            ImGui::SameLine();
        }
    }

    void Plot::draw()
    {
        if (ImGui::BeginTabItem(name_.c_str()))
        {
            // Force a start with the full view
            ImPlot::SetNextAxesLimits(seconds_f(begin_).count(), seconds_f(end_).count(),
                                      0, max_y_.count(),
                                      ImPlotCond_Once);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                // Full view on escape
                ImPlot::SetNextAxesLimits(seconds_f(begin_).count(), seconds_f(end_).count(),
                                          0, max_y_.count(),
                                          ImPlotCond_Always);
            }

            // Plot area
            ImGui::BeginChild("PlotRegion", ImVec2(0, -28), true,
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);

            draw_stats_panel();

            is_downsampled_ = false;
            if (ImPlot::BeginPlot("##Plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs))
            {
                double x_to_seconds = 1.0;      // X axis is in seconds
                double y_to_seconds = 0.001;    // Y axis is in milliseconds

                ImPlot::SetupAxisFormat(ImAxis_X1, time_formatter, &x_to_seconds);
                ImPlot::SetupAxisFormat(ImAxis_Y1, time_formatter, &y_to_seconds);

                compute_stats_on_view_update();

                // Get current plot limits once for all series
                auto limits = ImPlot::GetPlotLimits();
                
                // Update caches for all series based on current view
                for (auto& serie : series_)
                {
                    serie.update_section_cache(limits);
                }
                
                // Now plot all series
                for (auto const& serie : series_)
                {
                    is_downsampled_ |= serie.plot(limits);
                }
                
                ImPlot::EndPlot();
            }

            ImGui::EndChild();

            draw_status_bar();
            ImGui::EndTabItem();
        }
    }

    void Plot::draw_status_bar()
    {
        ImGui::Separator();
        ImGui::BeginChild("StatusBar", ImVec2(0, 24), false,
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Status:");
        ImGui::SameLine();

        if (is_downsampled_)
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

    void Plot::compute_stats_on_view_update()
    {
        // Compute when limits changed
        auto limits = ImPlot::GetPlotLimits();
        if (not ImGui::IsMouseDragging(0) and
            (limits.X.Min != old_limits_.X.Min or
             limits.X.Max != old_limits_.X.Max or
             limits.Y.Min != old_limits_.Y.Min or
             limits.Y.Max != old_limits_.Y.Max))
        {
            stats_.clear();
            for (auto const& serie : series_)
            {
                Statistics stats = serie.compute_statistics(limits.X.Min, limits.X.Max);
                stats_.push_back(stats);
            }
            old_limits_ = limits;
        }
    }
}
