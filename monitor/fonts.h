#ifndef RTM_MONITOR_FONTS_H
#define RTM_MONITOR_FONTS_H

struct ImFont;

namespace rtm
{
    // Load the embedded UI font (added first, so it stays the default) and the
    // embedded monospace font into the current ImGui atlas, baking the glyph
    // ranges we use (Latin + punctuation/arrows/math/geometric symbols, incl.
    // the real minus sign U+2212) so axis and unit labels don't render as '?'.
    // Falls back to the built-in font when no font was bundled. Call once, after
    // ImGui::CreateContext() and before the first frame. Font sizes are baked at
    // base pixel size; HiDPI scaling is handled by style.FontScaleDpi.
    void load_fonts();

    // Fixed-width font for streaming numeric values that must not jitter as
    // digits change. Null until load_fonts() runs (callers fall back to the
    // default font).
    ImFont* mono_font();
}

#endif
