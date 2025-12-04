#include <cmath>

#include "data.h"

namespace rtm
{
    result<std::vector<Point>> minmax_downsampler(std::vector<Point> const& series, uint32_t threshold)
    {
        if (series.empty() or threshold < 4)
        {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        uint32_t const n = static_cast<uint32_t>(series.size());
        if (threshold >= n)
        {
            return series;
        }

        uint32_t const bucket_size = n / (threshold / 2);

        std::vector<Point> out;
        out.reserve(threshold);

        for (uint32_t i = 0; i < n; i += bucket_size)
        {
            uint32_t end = std::min(i + bucket_size, n);

            float minY = std::numeric_limits<float>::infinity();
            float maxY = -std::numeric_limits<float>::infinity();
            Point minP, maxP;

            for (uint32_t j = i; j < end; ++j)
            {
                auto const& p = series[j];

                if (p.y < minY)
                {
                    minY = p.y;
                    minP = p;
                }
                if (p.y > maxY)
                {
                    maxY = p.y;
                    maxP = p;
                }
            }

            out.push_back(minP);
            out.push_back(maxP);
        }

        return out;
    }


    // Helper function to calculate triangle area using three points
    inline float calculate_triangle_area(Point const& a,
                                         Point const& b,
                                         Point const& c)
    {
        return std::abs((a.x - c.x) * (b.y - a.y) - (a.x - b.x) * (c.y - a.y)) * 0.5f;
    }

    result<std::vector<Point>> lttb(std::vector<Point> const& serie, uint32_t threshold)
    {
        // Validate input
        if (serie.empty() or threshold < 3)
        {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        // If threshold is greater than or equal to data size, return original data
        uint32_t const n = static_cast<uint32_t>(serie.size());
        if (n <= threshold)
        {
            return serie;
        }

        std::vector<Point> sampled;
        sampled.reserve(threshold);

        // Always add the first point
        sampled.push_back(serie.front());

        // Bucket size (excluding first and last points)
        float const bucket_size = static_cast<float>(n - 2) / float(threshold - 2);

        uint32_t a = 0; // Initially point A is the first point

        for (uint32_t i = 0; i < threshold - 2; ++i)
        {
            // Calculate bucket range for current bucket
            uint32_t const bucket_start = static_cast<uint32_t>(std::floor(float(i + 0) * bucket_size)) + 1;
            uint32_t const bucket_end   = static_cast<uint32_t>(std::floor(float(i + 1) * bucket_size)) + 1;

            // Calculate average point for next bucket (used as point C)
            float avg_x = 0.0, avg_y = 0.0;
            uint32_t avg_range_start, avg_range_end;

            if (i < threshold - 3)
            {
                // Not the last bucket
                avg_range_start = static_cast<uint32_t>(std::floor(float(i + 1) * bucket_size)) + 1;
                avg_range_end   = static_cast<uint32_t>(std::floor(float(i + 2) * bucket_size)) + 1;
            }
            else
            {
                // Last bucket - use the last point
                avg_range_start = n - 1;
                avg_range_end   = n;
            }

            // Calculate average
            avg_range_end = std::min(avg_range_end, n);
            float const avg_range_length = static_cast<float>(avg_range_end - avg_range_start);

            for (uint32_t j = avg_range_start; j < avg_range_end; ++j)
            {
                avg_x += serie[j].x;
                avg_y += serie[j].y;
            }
            avg_x /= avg_range_length;
            avg_y /= avg_range_length;

            Point const avg_point{avg_x, avg_y};

            // Find point in current bucket with largest triangle area
            uint32_t max_area_point = bucket_start;
            float max_area = -1.0;

            uint32_t const actual_bucket_end = std::min(bucket_end, n);

            for (uint32_t j = bucket_start; j < actual_bucket_end; ++j)
            {
                float const area = calculate_triangle_area(serie[a], serie[j], avg_point);

                if (area > max_area)
                {
                    max_area = area;
                    max_area_point = j;
                }
            }

            // Add the point with the largest area
            sampled.push_back(serie[max_area_point]);
            a = max_area_point; // This point becomes the new point A
        }

        // Always add the last point
        sampled.push_back(serie.back());

        return sampled;
    }


    result<std::vector<Point>> minmax_lttb(std::vector<Point> const& series, uint32_t threshold)
    {
        // 1. min–max preselection
        auto preselect = minmax_downsampler(series, threshold * 4);
        if (not preselect)
        {
            return std::unexpected(preselect.error());
        }

        // 2. LTTB
        auto l = lttb(*preselect, threshold);
        if (not l)
        {
            return std::unexpected(l.error());
        }

        return l;
    }
}
