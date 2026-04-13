#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <implot.h>

#include "serie.h"

using namespace std::chrono;

namespace rtm
{
    ImVec4 generate_random_color()
    {
        // procedural color generator: the gold ratio
        static double next_color_hue = 1.0 / (rand() % 99 + 1);
        constexpr double golden_ratio_conjugate = 0.618033988749895; // 1 / phi
        next_color_hue += golden_ratio_conjugate;
        next_color_hue = std::fmod(next_color_hue, 1.0);

        ImVec4 next_color{0, 0, 0, 1};
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(next_color_hue), 0.5f, 0.99f,
                                    next_color.x, next_color.y, next_color.z);
        return next_color;
    }


    void Serie::split_serie(std::vector<Section>& sections, std::vector<Point> const& flat)
    {
        Section section;
        seconds_f min = seconds_f{flat.front().x};
        seconds_f max = min + SECTION_SIZE;
        for (auto const& point : flat)
        {
            while (point.x >= max.count())
            {
                if (not section.points.empty())
                {
                    section.min = min;
                    section.max = max;
                    sections.emplace_back(std::move(section));
                    section = Section{};
                }

                min += SECTION_SIZE;
                max += SECTION_SIZE;
            }
            section.points.push_back(point);
        }

        if (not section.points.empty())
        {
            section.min = min;
            section.max = seconds_f{section.points.back().x};
            sections.emplace_back(std::move(section));
        }
    }


    Serie::Serie(std::string const& name, std::vector<Point>&& raw_serie, ImVec4 color)
    {
        constexpr uint32_t DECIMATION = 8'000;

        nanoseconds t1 = since_epoch();

        if (not raw_serie.empty())
        {
            split_serie(sections_, raw_serie);
        }

        //serie_ = minmax_downsampler(diffs_full_, DECIMATION);
        serie_ = lttb(raw_serie, DECIMATION);
        //serie_ = minmax_lttb(diffs_full_, DECIMATION);

        if (serie_.size() != raw_serie.size())
        {
            is_downsampled_ = true;
        }

        nanoseconds t2 = since_epoch();
        printf("loaded in %f ms (%ld)\n", duration_cast<milliseconds_f>(t2 - t1).count(), raw_serie.size());

        name_   = name;
        color_  = color;
    }

    std::string const& Serie::name() const
    {
        if (display_name_.empty())
        {
            return name_;
        }
        return display_name_;
    }

    std::string Serie::plot_id() const
    {
        std::string label = name();
        label += "###";
        label += name_;
        return label;
    }

    void Serie::plot_visible(ImPlotRect const& limits, Point const* data, int count) const
    {
        auto begin = data;
        auto end   = data + count;

        Point lo{limits.X.Min, 0};
        Point hi{limits.X.Max, 0};
        auto cmp = [](Point const& a, Point const& b) { return a.x < b.x; };

        auto vis_begin = std::lower_bound(begin, end, lo, cmp);
        auto vis_end   = std::upper_bound(vis_begin, end, hi, cmp);

        if (vis_begin != begin) --vis_begin;
        if (vis_end   != end)   ++vis_end;

        int vis_count = static_cast<int>(vis_end - vis_begin);
        if (vis_count > 0)
        {
            ImPlot::PlotLine(plot_id().c_str(), &vis_begin->x, &vis_begin->y,
                vis_count, 0, 0, sizeof(Point));
        }
    }

    bool Serie::plot() const
    {
        if (serie_.empty())
        {
            //TODO: Display something to say its empty?
            return false;
        }

        ImPlot::SetNextLineStyle(color_);

        auto limits = ImPlot::GetPlotLimits();

        if (not is_downsampled_)
        {
            plot_visible(limits, serie_.data(), static_cast<int>(serie_.size()));
            return is_downsampled_;
        }

        if (limits.X.Size() < SECTION_SIZE.count())
        {
            auto it = std::find_if(sections_.begin(), sections_.end(), [&](Section const& s)
            {
                return (s.min.count() > limits.X.Min) and (s.max.count() >= limits.X.Max);
            });
            if (it == sections_.end())
            {
                it = std::prev(sections_.end());
            }
            if (it != sections_.begin())
            {
                it--;
            }

            for (int i = 0; i < 3; ++i)
            {
                plot_visible(limits, it->points.data(), static_cast<int>(it->points.size()));

                it++;
                if (it == sections_.end())
                {
                    break;
                }
            }

            return false;
        }
        else
        {
            plot_visible(limits, serie_.data(), static_cast<int>(serie_.size()));
            return is_downsampled_;
        }
    }


    static Point const* nearest_in(Point const* data, int count, double x)
    {
        if (count == 0)
        {
            return nullptr;
        }

        Point target{x, 0};
        auto cmp = [](Point const& a, Point const& b) { return a.x < b.x; };
        auto it = std::lower_bound(data, data + count, target, cmp);

        int idx = static_cast<int>(it - data);
        int start = std::max(0, idx - 1);
        int end = std::min(count, idx + 2);

        Point const* best = nullptr;
        double best_dx = std::numeric_limits<double>::max();
        for (int i = start; i < end; ++i)
        {
            double dx = std::abs(data[i].x - x);
            if (dx < best_dx)
            {
                best_dx = dx;
                best = &data[i];
            }
        }
        return best;
    }

    Point const* Serie::find_nearest(double x, ImPlotRect const& limits) const
    {
        if (serie_.empty())
        {
            return nullptr;
        }

        if (not is_downsampled_ or limits.X.Size() >= SECTION_SIZE.count())
        {
            return nearest_in(serie_.data(), static_cast<int>(serie_.size()), x);
        }

        // Zoomed in: search the same sections that plot() displays
        auto it = std::find_if(sections_.begin(), sections_.end(), [&](Section const& s)
        {
            return (s.min.count() > limits.X.Min) and (s.max.count() >= limits.X.Max);
        });
        if (it == sections_.end())
        {
            it = std::prev(sections_.end());
        }
        if (it != sections_.begin())
        {
            it--;
        }

        Point const* best = nullptr;
        double best_dx = std::numeric_limits<double>::max();
        for (int i = 0; i < 3; ++i)
        {
            Point const* candidate = nearest_in(it->points.data(),
                static_cast<int>(it->points.size()), x);
            if (candidate)
            {
                double dx = std::abs(candidate->x - x);
                if (dx < best_dx)
                {
                    best_dx = dx;
                    best = candidate;
                }
            }

            it++;
            if (it == sections_.end())
            {
                break;
            }
        }
        return best;
    }

    Statistics Serie::compute_statistics(double begin, double end) const
    {
        if (sections_.empty())
        {
            return Statistics{};
        }

        // Binary search for first section containing 'begin'
        auto it_first = std::lower_bound(sections_.begin(), sections_.end(), begin,
            [](Section const& s, double val) { return s.max.count() <= val; });

        // Binary search for last section containing 'end'
        auto it_last = std::lower_bound(sections_.begin(), sections_.end(), end,
            [](Section const& s, double val) { return s.max.count() < val; });

        if (it_first == sections_.end())
        {
            if (begin < sections_.front().min.count())
            {
                it_first = sections_.begin();
            }
            else
            {
                return Statistics{};
            }
        }

        if (it_last == sections_.end())
        {
            if (end > sections_.back().max.count())
            {
                it_last = sections_.end() - 1;
            }
            else
            {
                return Statistics{};
            }
        }

        auto const& first_section = it_first->points;
        auto const& last_section  = it_last->points;

        Point begin_pt{begin, 0};
        Point end_pt{end, 0};
        auto pt_cmp = [](Point const& a, Point const& b) { return a.x < b.x; };

        auto first_point = std::lower_bound(first_section.begin(), first_section.end(), begin_pt, pt_cmp);
        if (first_point == first_section.end())
        {
            first_point = first_section.begin();
        }

        auto last_point = std::lower_bound(last_section.begin(), last_section.end(), end_pt, pt_cmp);
        if (last_point == last_section.end())
        {
            last_point = last_section.end() - 1;
        }

        Statistics stats;

        int range_size = 0;
        double accumulated = 0;
        double square_accumulated = 0;
        stats.min = std::numeric_limits<double>::max();
        stats.max = std::numeric_limits<double>::lowest();

        auto compute_section = [&](std::vector<Point>::const_iterator section_begin, std::vector<Point>::const_iterator section_end)
        {
            for (auto it = section_begin; it != section_end; ++it)
            {
                stats.min = std::min(stats.min, it->y);
                stats.max = std::max(stats.max, it->y);
                accumulated += it->y;
                square_accumulated += (it->y * it->y);
                ++range_size;
            }
        };

        auto finalize = [&]()
        {
            if (range_size == 0)
            {
                return Statistics{};
            }
            stats.valid = true;
            double range_d = static_cast<double>(range_size);
            stats.average = accumulated / range_d;
            stats.rms = std::sqrt(square_accumulated / range_d);
            stats.standard_deviation = std::sqrt((square_accumulated / range_d) - stats.average * stats.average);
            return stats;
        };

        if (it_first == it_last)
        {
            // only one section
            compute_section(first_point, last_point);
            return finalize();
        }

        // first section (partial)
        compute_section(first_point, first_section.end());

        // middle section(s) (full)
        for (auto it_section = it_first + 1; it_section != it_last; ++it_section)
        {
            compute_section(it_section->points.begin(), it_section->points.end());
        }

        // last section (partial)
        compute_section(last_section.begin(), last_point);

        return finalize();
    }
}
