#include <algorithm>
#include <limits>
#include <implot.h>
#include <implot_internal.h>

#include "plot.h"

namespace rtm
{
    constexpr double Y_AXIS_MARGIN = 0.05;
    int format_duration(char* buff, int size, seconds_f duration, int precision = 3)
    {
        double seconds = duration.count();
        double abs_val = std::abs(seconds);

        if (abs_val >= 31536000)  // Years (365 days)
        {
            return snprintf(buff, size, "%.*f y", precision, seconds / 31536000);
        }
        else if (abs_val >= 2592000)  // Months (30 days)
        {
            return snprintf(buff, size, "%.*f mo", precision, seconds / 2592000);
        }
        else if (abs_val >= 86400)  // Days
        {
            return snprintf(buff, size, "%.*f d", precision, seconds / 86400);
        }
        else if (abs_val >= 3600)  // Hours
        {
            return snprintf(buff, size, "%.*f h", precision, seconds / 3600);
        }
        else if (abs_val >= 60)  // Minutes
        {
            return snprintf(buff, size, "%.*f min", precision, seconds / 60);
        }
        else if (abs_val >= 1)  // Seconds
        {
            return snprintf(buff, size, "%.*f s", precision, seconds);
        }
        else if (abs_val >= 1e-3)  // Milliseconds
        {
            return snprintf(buff, size, "%.*f ms", precision, seconds * 1e3);
        }
        else if (abs_val >= 1e-6)  // Microseconds
        {
            return snprintf(buff, size, "%.*f us", precision, seconds * 1e6);
        }
        else  // Nanoseconds
        {
            return snprintf(buff, size, "%.*f ns", precision, seconds * 1e9);
        }
    }

    int time_formatter(double value, char* buff, int size, void* user_data)
    {
        double to_seconds = *static_cast<double*>(user_data);
        return format_duration(buff, size, seconds_f{value * to_seconds}, 2);
    }

    Plot::Plot(std::string const& name, std::string const& legend)
        : name_{name}
        , legend_{legend}
    {

    }


    void Plot::add_serie(std::shared_ptr<Serie> s, milliseconds_f min_y, milliseconds_f max_y, nanoseconds begin, nanoseconds end, bool visible)
    {
        series_.push_back(std::move(s));
        serie_bounds_.push_back({seconds_f(begin), seconds_f(end), min_y, max_y, visible});
    }

    bool Plot::compute_visible_limits(double& x_min, double& x_max,
                                      double& y_min, double& y_max) const
    {
        x_min = std::numeric_limits<double>::max();
        x_max = std::numeric_limits<double>::lowest();
        y_min = std::numeric_limits<double>::max();
        y_max = std::numeric_limits<double>::lowest();

        bool any_visible = false;
        for (auto const& b : serie_bounds_)
        {
            if (not b.visible)
            {
                continue;
            }
            any_visible = true;
            x_min = std::min(x_min, b.begin.count());
            x_max = std::max(x_max, b.end.count());
            y_min = std::min(y_min, b.min_y.count());
            y_max = std::max(y_max, b.max_y.count());
        }

        if (not any_visible)
        {
            return false;
        }

        if (y_max <= y_min)
        {
            // Flat-line series (all samples share the same y): expand with a
            // small absolute margin so the curve is still visible on first
            // render instead of sitting on a zero-height axis range.
            constexpr double FLAT_LINE_Y_PAD = 1.0; // milliseconds
            y_min -= FLAT_LINE_Y_PAD;
            y_max += FLAT_LINE_Y_PAD;
        }
        else
        {
            double y_range = y_max - y_min;
            y_min -= y_range * Y_AXIS_MARGIN;
            y_max += y_range * Y_AXIS_MARGIN;
        }
        return true;
    }

    void Plot::sort_series()
    {
        std::vector<std::pair<std::shared_ptr<Serie>, SerieBounds>> zipped;
        zipped.reserve(series_.size());
        for (std::size_t i = 0; i < series_.size(); ++i)
        {
            zipped.emplace_back(std::move(series_[i]), serie_bounds_[i]);
        }

        std::stable_sort(zipped.begin(), zipped.end(),
            [](auto const& a, auto const& b)
            {
                return a.first->display_weight() < b.first->display_weight();
            });

        for (std::size_t i = 0; i < zipped.size(); ++i)
        {
            series_[i]       = std::move(zipped[i].first);
            serie_bounds_[i] = zipped[i].second;
        }
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
            if (stats_future_.valid())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(computing...)");
            }
            ImGui::Separator();

            // Display stats for each series
            for (std::size_t i = 0; i < series_.size() and i < stats_.size(); ++i)
            {
                Statistics const& stats = stats_[i];
                Serie const& serie = *series_[i];

                // Create unique ID by combining name with index
                std::string unique_id = serie.name() + "##" + std::to_string(i);

                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNode(unique_id.c_str()))
                {
                    if (stats.valid)
                    {
                        char buf[32];
                        format_duration(buf, sizeof(buf), milliseconds_f{stats.min});
                        ImGui::Text("Min:     %s", buf);
                        format_duration(buf, sizeof(buf), milliseconds_f{stats.max});
                        ImGui::Text("Max:     %s", buf);
                        format_duration(buf, sizeof(buf), milliseconds_f{stats.average});
                        ImGui::Text("Average: %s", buf);
                        format_duration(buf, sizeof(buf), milliseconds_f{stats.rms});
                        ImGui::Text("RMS:     %s", buf);
                        format_duration(buf, sizeof(buf), milliseconds_f{stats.standard_deviation});
                        ImGui::Text("Std Dev: %s", buf);
                    }
                    else
                    {
                        ImGui::TextDisabled("Out of view");
                    }

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
            double x_min, x_max, y_min, y_max;
            bool has_visible_limits = compute_visible_limits(x_min, x_max, y_min, y_max);

            // Force a start with the full view (fit to visible series only).
            if (has_visible_limits)
            {
                ImPlot::SetNextAxesLimits(x_min, x_max, y_min, y_max, ImPlotCond_Once);
            }

            if (has_visible_limits and ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImPlot::SetNextAxesLimits(x_min, x_max, y_min, y_max, ImPlotCond_Always);
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
                for (std::size_t i = 0; i < series_.size(); ++i)
                {
                    ImPlot::HideNextItem(!serie_bounds_[i].visible, ImPlotCond_Once);
                    is_downsampled_ |= series_[i]->plot();
                    ImPlotItem* item = ImPlot::GetItem(series_[i]->plot_id().c_str());
                    serie_bounds_[i].visible = (item == nullptr or item->Show);
                }

                // Snap Tooltip
                if (ImPlot::IsPlotHovered()) 
                {
                    ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                    auto limits = ImPlot::GetPlotLimits();

                    const rtm::Serie* closest_serie = nullptr;
                    const Point* closest_point = nullptr;
                    double best_dx = std::numeric_limits<double>::max();

                    for (std::size_t i = 0; i < series_.size(); ++i) 
                    {
                        if (not serie_bounds_[i].visible)
                            continue;

                        Point const* pt = series_[i]->find_nearest(mouse.x, limits);
                        if (pt)
                        {
                            double dx = std::abs(pt->x - mouse.x);
                            if (dx < best_dx)
                            {
                                best_dx = dx;
                                closest_serie = series_[i].get();
                                closest_point = pt;
                            }
                        }
                    }

                    if (closest_serie and closest_point) 
                    {
                        // Tooltip
                        ImGui::BeginTooltip();
                        ImGui::Text("Serie: %s", closest_serie->name().c_str());
                        ImGui::Separator();
                        ImGui::Text("x: %.3f s", closest_point->x);
                        ImGui::Text("y: %.3f %s", closest_point->y, legend_.c_str());
                        ImGui::EndTooltip();

                        // Marker on nearest point
                        ImVec4 no_fill = ImVec4(0, 0, 0, 0);
                        ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross, 12,
                            no_fill, 2.0f, closest_serie->color());
                        ImPlot::PlotScatter("##closest", &closest_point->x, &closest_point->y, 1);
                    }
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
        if (stats_future_.valid() and
            stats_future_.wait_for(0s) == std::future_status::ready)
        {
            stats_ = stats_future_.get();
        }

        auto limits = ImPlot::GetPlotLimits();
        if (not stats_future_.valid() and
            not ImGui::IsMouseDragging(0) and
            (limits.X.Min != old_limits_.X.Min or
             limits.X.Max != old_limits_.X.Max))
        {
            old_limits_ = limits;
            double x_min = limits.X.Min;
            double x_max = limits.X.Max;

            stats_future_ = std::async(std::launch::async,
                [this, x_min, x_max]()
                {
                    std::vector<Statistics> result;
                    result.reserve(series_.size());
                    for (auto const& serie : series_)
                    {
                        result.push_back(serie->compute_statistics(x_min, x_max));
                    }
                    return result;
                });
        }
    }
}
