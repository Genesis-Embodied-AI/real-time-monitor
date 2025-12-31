#ifndef RTM_MONITOR_SERIE_H
#define RTM_MONITOR_SERIE_H

#include <imgui.h>
#include <unordered_map>

#include "rtm/data.h"
#include "rtm/os/time.h"

namespace rtm
{
    ImVec4 generate_random_color();

    struct Statistics
    {
        float min;
        float max;
        float average;
        float rms;
        float standard_deviation;
    };

    class Serie
    {
    public:
        Serie(std::string const& name, std::vector<Point>&& raw_serie, ImVec4 color);
        ~Serie() = default;

        bool plot() const;

        std::string const& name() const { return name_; }
        Statistics compute_statistics(double begin, double end) const;

    private:
        static constexpr seconds_f SECTION_SIZE = 2min;
        struct Section
        {
            seconds_f min;
            seconds_f max;
            std::vector<Point> points;
        };
        void split_serie(std::vector<Section>& sections, std::vector<Point> const& flat);

        ImVec4 color_;

        std::string name_;

        std::vector<Section> sections_;
        std::vector<Point> serie_;
        bool is_downsampled_;
    };
}

#endif
