#include "fonts.h"

#include <string>

#include <imgui.h>

#include "bundled_fonts.h"

namespace rtm
{
    namespace
    {
        ImFont* g_mono_font = nullptr;

        ImVector<ImWchar> const& glyphRanges(ImGuiIO& io)
        {
            static ImVector<ImWchar> ranges;
            if (ranges.empty())
            {
                ImFontGlyphRangesBuilder builder;
                builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
                static const ImWchar extra[] = {
                    0x2010, 0x205E,   // general punctuation: dashes, bullet, ...
                    0x2190, 0x21FF,   // arrows
                    0x2200, 0x22FF,   // math operators (incl. minus sign 0x2212)
                    0x25A0, 0x25FF,   // geometric shapes
                    0,
                };
                builder.AddRanges(extra);
                builder.BuildRanges(&ranges);
            }
            return ranges;
        }
    }

    void load_fonts()
    {
        ImGuiIO& io = ImGui::GetIO();

        int count = 0;
        BundledFont const* fonts = bundledFonts(&count);
        if ((fonts == nullptr) or (count == 0))
        {
            io.Fonts->AddFontDefault();   // no bundle: built-in default
            return;
        }

        ImVector<ImWchar> const& ranges = glyphRanges(io);

        BundledFont const* ui   = nullptr;
        BundledFont const* mono = nullptr;
        for (int i = 0; i < count; ++i)
        {
            std::string name = fonts[i].name;
            if (name.find("Mono") != std::string::npos)
            {
                if (mono == nullptr) { mono = &fonts[i]; }
            }
            else if (ui == nullptr)
            {
                ui = &fonts[i];
            }
        }

        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;  // data is a static array in the binary
        if (ui != nullptr)
        {
            io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(ui->data),
                                           static_cast<int>(ui->size), 16.0f, &cfg, ranges.Data);
        }
        if (mono != nullptr)
        {
            ImFontConfig mcfg;
            mcfg.FontDataOwnedByAtlas = false;
            g_mono_font = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(mono->data),
                                                         static_cast<int>(mono->size), 14.0f, &mcfg, ranges.Data);
        }

        if (ui == nullptr and mono == nullptr)
        {
            io.Fonts->AddFontDefault();
        }
    }

    ImFont* mono_font()
    {
        return g_mono_font;
    }
}
