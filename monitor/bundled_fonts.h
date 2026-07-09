#ifndef RTM_MONITOR_BUNDLED_FONTS_H
#define RTM_MONITOR_BUNDLED_FONTS_H

namespace rtm
{
    struct BundledFont
    {
        char const*          name;
        unsigned char const* data;
        unsigned int         size;
    };

    // Fonts embedded at build time from monitor/assets/fonts/*.ttf.
    // Returns the table and writes its length to *count.
    BundledFont const* bundledFonts(int* count);
}

#endif
