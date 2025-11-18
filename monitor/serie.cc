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


    void Serie::split_serie(std::vector<Section>& sections, std::vector<Parser::Point> const& flat)
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
    }


    Serie::Serie(TickHeader const& header, std::vector<Parser::Point>&& raw_serie, ImVec4 color)
    {
        using namespace std::chrono;
        auto  since_epoch = []()
        {
            auto now = time_point_cast<nanoseconds>(system_clock::now());
            return now.time_since_epoch();
        };

        constexpr uint32_t DECIMATION = 300'000; // 5min @ 1ms

        nanoseconds t1 =since_epoch();

        split_serie(sections_, raw_serie);

        //auto rc_diffs = minmax_downsampler(diffs_full_, DECIMATION);
        auto rc_downampled = lttb(raw_serie, DECIMATION);
        //auto rc_diffs = minmax_lttb(diffs_full_, DECIMATION);
        if (rc_downampled)
        {
            serie_ = *rc_downampled;
        }
        if (serie_.size() != raw_serie.size())
        {
            is_downsampled_ = true;
        }

        nanoseconds t2 = since_epoch();
        printf("loaded in %f ms (%ld)\n", duration_cast<milliseconds_f>(t2 - t1).count(), serie_.size());

        name_ = header.process;
        name_ += '.';
        name_ += header.name;
        name_ += " (";
        name_ += format_iso_timestamp(header.start_time);
        name_ += ')';

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
                0, 0, sizeof(Parser::Point));
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
                    0, 0, sizeof(Parser::Point));
                return is_downsampled_;
            }

            // display 3 sections (before, here and after if possible)
            if (it != sections_.begin())
            {
                it--;
            }

            for (int i = 0; i < 3; ++i)
            {
                Parser::Point const* data = it->points.data();
                ImPlot::PlotLine(name_.c_str(), &data->x, &data->y, static_cast<int>(it->points.size()),
                    0, 0, sizeof(Parser::Point));

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
                0, 0, sizeof(Parser::Point));
            return is_downsampled_;
        }
    }
}
