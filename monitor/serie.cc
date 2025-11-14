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


    Serie::Serie(Parser const& parser)
    {
        diffs_full_  = parser.generate_times_diff();
        ups_full_    = parser.generate_times_up();

        using namespace std::chrono;
        auto  since_epoch = []()
        {
            auto now = time_point_cast<nanoseconds>(system_clock::now());
            return now.time_since_epoch();
        };
        using milliseconds_f = std::chrono::duration<float, std::milli>;

        constexpr uint32_t DECIMATION = 300'000; // 5min @ 1ms

        nanoseconds t1 =since_epoch();
        //auto rc_diffs = minmax_downsampler(diffs_full_, DECIMATION);
        auto rc_diffs = lttb(diffs_full_, DECIMATION);
        //auto rc_diffs = minmax_lttb(diffs_full_, DECIMATION);
        if (rc_diffs)
        {
            diffs_downsampled_ = *rc_diffs;
        }

        //auto rc_ups = minmax_downsampler(ups_full_, DECIMATION);
        auto rc_ups = lttb(ups_full_, DECIMATION);
        //auto rc_ups = minmax_lttb(ups_full_, DECIMATION);
        if (rc_ups)
        {
            ups_downsampled_ = *rc_ups;
        }
        nanoseconds t2 = since_epoch();
        printf("loaded in %f ms (%ld)\n", duration_cast<milliseconds_f>(t2 - t1).count(), diffs_downsampled_.size());

        name_ = parser.header().process;
        name_ += '.';
        name_ += parser.header().name;
        name_ += " (";
        name_ += format_iso_timestamp(parser.header().start_time);
        name_ += ')';

        color_  = generate_random_color();
    }

    bool Serie::plot_diffs() const
    {
        ImPlot::SetNextLineStyle(color_);

        auto limits = ImPlot::GetPlotLimits();
        if (limits.X.Size() < 10.0)
        {
            ImPlot::PlotLine(name_.c_str(), &diffs_full_.at(0).x, &diffs_full_.at(0).y, static_cast<int>(diffs_full_.size()),
                0, 0, sizeof(Parser::Point));
            return false;
        }
        else
        {
            ImPlot::PlotLine(name_.c_str(), &diffs_downsampled_.at(0).x, &diffs_downsampled_.at(0).y, static_cast<int>(diffs_downsampled_.size()),
            0, 0, sizeof(Parser::Point));
            return diffs_full_.size() != diffs_downsampled_.size();
        }
    }

    bool Serie::plot_ups() const
    {
        ImPlot::SetNextLineStyle(color_);

        auto limits = ImPlot::GetPlotLimits();
        if (limits.X.Size() < 10.0)
        {
            ImPlot::PlotLine(name_.c_str(), &ups_full_.at(0).x, &ups_full_.at(0).y, static_cast<int>(ups_full_.size()),
                0, 0, sizeof(Parser::Point));
            return false;
        }
        else
        {
            ImPlot::PlotLine(name_.c_str(), &ups_downsampled_.at(0).x, &ups_downsampled_.at(0).y, static_cast<int>(ups_downsampled_.size()),
                0, 0, sizeof(Parser::Point));

            printf("why? %ld %ld\n", ups_full_.size(), ups_downsampled_.size());
            return ups_full_.size() != ups_downsampled_.size();
        }
    }
}
