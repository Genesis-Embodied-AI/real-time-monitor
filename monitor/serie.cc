#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <implot.h>

#include "serie.h"

namespace rtm
{
    ImVec4 generate_random_color()
    {
        // procedural color generator: the gold ratio
        static double next_color_hue = 1.0 / (rand() % 100);
        constexpr double golden_ratio_conjugate = 0.618033988749895; // 1 / phi
        next_color_hue += golden_ratio_conjugate;
        next_color_hue = std::fmod(next_color_hue, 1.0);

        ImVec4 next_color{0, 0, 0, 1};
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(next_color_hue), 0.5f, 0.99f,
                                    next_color.x, next_color.y, next_color.z);
        return next_color;
    }

    std::string format_iso_timestamp(nanoseconds timestamp)
    {
        constexpr uint32_t ISO_TIMESTAMP_LENGTH = 17;
        constexpr char ISO_TIMESTAMP_FORMAT[] = "%Y%m%dT%H%M%SZ";

        std::time_t const serie_start_time = duration_cast<seconds>(timestamp).count();
        char buffer[ISO_TIMESTAMP_LENGTH];

        std::tm utc_time;
        if (gmtime_r(&serie_start_time, &utc_time) == nullptr)
        {
            return "";
        }
        std::strftime(buffer, ISO_TIMESTAMP_LENGTH, ISO_TIMESTAMP_FORMAT, &utc_time);
        return std::string(buffer);
    }


    void Serie::split_serie(std::vector<Section>& sections, std::vector<Point> const& flat)
    {
        Section section;
        seconds_f min = 0min;
        seconds_f max = SECTION_SIZE;
        for (auto const& point : flat)
        {
            if (point.x < max.count())
            {
                section.points.push_back(point);
            }
            else
            {
                section.min = min;
                section.max = max;
                sections.emplace_back(std::move(section));

                min += SECTION_SIZE;
                max += SECTION_SIZE;
            }
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
        constexpr uint32_t DECIMATION = 200'000;

        nanoseconds t1 = since_epoch();

        split_serie(sections_, raw_serie);

        //serie_ = minmax_downsampler(diffs_full_, DECIMATION);
        serie_ = lttb(raw_serie, DECIMATION);
        //serie_ = minmax_lttb(diffs_full_, DECIMATION);

        if (serie_.size() != raw_serie.size())
        {
            is_downsampled_ = true;
        }

        nanoseconds t2 = since_epoch();
        printf("loaded in %f ms (%ld)\n", duration_cast<milliseconds_f>(t2 - t1).count(), serie_.size());

        name_   = name;
        color_  = color;
    }

    bool Serie::plot() const
    {
        if (serie_.empty())
        {
            //TODO: Display something to say its empty?
            return false;
        }

        ImPlot::SetNextLineStyle(color_);

        if (not is_downsampled_)
        {
            // skip smart display: the serie is small enough
            ImPlot::PlotLine(name_.c_str(), &serie_.at(0).x, &serie_.at(0).y, static_cast<int>(serie_.size()),
                0, 0, sizeof(Point));
            return is_downsampled_;
        }

        auto limits = ImPlot::GetPlotLimits();
        if (limits.X.Size() < SECTION_SIZE.count())
        {
            auto it = std::find_if(sections_.begin(), sections_.end(), [&](Section const& s)
            {
                return (s.min.count() >  limits.X.Min) and (s.max.count() >= limits.X.Max);
            });
            if (it == sections_.end())
            {
                // out of scope: show full serie
                ImPlot::PlotLine(name_.c_str(), &serie_.at(0).x, &serie_.at(0).y, static_cast<int>(serie_.size()),
                    0, 0, sizeof(Point));
                return is_downsampled_;
            }

            // display 3 sections (before, here and after if possible)
            if (it != sections_.begin())
            {
                it--;
            }

            for (int i = 0; i < 3; ++i)
            {
                Point const* data = it->points.data();
                ImPlot::PlotLine(name_.c_str(), &data->x, &data->y, static_cast<int>(it->points.size()),
                    0, 0, sizeof(Point));

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
            // downsampled
            ImPlot::PlotLine(name_.c_str(), &serie_.at(0).x, &serie_.at(0).y, static_cast<int>(serie_.size()),
                0, 0, sizeof(Point));
            return is_downsampled_;
        }
    }


    Statistics Serie::compute_statistics(double begin, double end) const
    {
        // Search first and last section to process
        auto it_first = std::find_if(sections_.begin(), sections_.end(), [&](Section const& s)
        {
            return (s.min.count() <= begin) and (begin < s.max.count());
        });

        auto it_last = std::find_if(sections_.begin(), sections_.end(), [&](Section const& s)
        {
            return (s.min.count() <= end) and (end < s.max.count());
        });

        //
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

        // first section
        // -> search for the begining
        auto first_point = std::find_if(first_section.begin(), first_section.end(), [&](Point const& p)
        {
            return (begin <= p.x);
        });
        if (first_point == first_section.end())
        {
            first_point = first_section.begin();
        }

        // last section
        // -> search for the end
        auto last_point = std::find_if(last_section.begin(), last_section.end(), [&](Point const& p)
        {
            return (end <= p.x);
        });
        if (last_point == last_section.end())
        {
            last_point = last_section.end() - 1;
        }

        Statistics stats;

        int range_size = 0;
        float accumulated = 0;
        float square_accumulated = 0;
        stats.min = first_section.front().y;
        stats.max = first_section.front().y;

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
            float range_f = static_cast<float>(range_size);
            stats.average = accumulated / range_f;
            stats.rms = std::sqrt(square_accumulated / range_f);
            stats.standard_deviation = std::sqrt((square_accumulated / range_f) - stats.average * stats.average);
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
