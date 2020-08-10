// SPDX-License-Identifier: GPL-3.0-only

#include <d3d9.h>
#include <d3dx9.h>
#include <variant>
#include <filesystem>
#include "../halo_data/tag.hpp"
#include "../chimera.hpp"
#include "../config/ini.hpp"
#include "output.hpp"
#include "../signature/hook.hpp"
#include "../signature/signature.hpp"
#include "draw_text.hpp"
#include "../event/frame.hpp"
#include "../event/d3d9_end_scene.hpp"
#include "../event/d3d9_reset.hpp"
#include "../halo_data/resolution.hpp"
#include "../fix/widescreen_fix.hpp"
#include "../custom_chat/hud_text.hpp"

namespace Chimera {
    #include "color_codes.hpp"

    static LPD3DXFONT system_font_override = nullptr, console_font_override = nullptr, small_font_override = nullptr, large_font_override = nullptr;
    static std::pair<int, int> system_font_shadow, console_font_shadow, small_font_shadow, large_font_shadow;
    static LPDIRECT3DDEVICE9 dev = nullptr;

    static LPD3DXFONT get_override_font(GenericFont font) {
        // Do NOT use these if widescreen fix is disabled unless we're 4:3
        if(!widescreen_fix_enabled()) {
            auto resolution = get_resolution();
            if(resolution.width / 4 * 3 != resolution.height) {
                return nullptr;
            }
        }

        switch(font) {
            case GenericFont::FONT_CONSOLE:
                return console_font_override;
            case GenericFont::FONT_SYSTEM:
                return system_font_override;
            case GenericFont::FONT_SMALL:
                return small_font_override;
            case GenericFont::FONT_LARGE:
                return large_font_override;
            default:
                std::terminate();
        }
    }

    static LPD3DXFONT get_override_font(const std::variant<TagID, GenericFont> &font) {
        auto *generic = std::get_if<1>(&font);
        if(generic) {
            return get_override_font(*generic);
        }
        else {
            return nullptr;
        }
    }

    const TagID &get_generic_font(GenericFont font) noexcept {
        // Get the globals tag
        auto *globals_tag = get_tag("globals\\globals", TagClassInt::TAG_CLASS_GLOBALS);
        auto *interface_bitmaps = *reinterpret_cast<std::byte **>(globals_tag->data + 0x144);

        // Console font is referenced here
        if(font == GenericFont::FONT_CONSOLE) {
            return *reinterpret_cast<const TagID *>(interface_bitmaps + 0x10 + 0xC);
        }
        // System font
        else if(font == GenericFont::FONT_SYSTEM) {
            return *reinterpret_cast<const TagID *>(interface_bitmaps + 0x00 + 0xC);
        }

        // Get HUD globals which has the remaining two fonts.
        auto *hud_globals = get_tag(*reinterpret_cast<const TagID *>(interface_bitmaps + 0x60 + 0xC));
        if(font == GenericFont::FONT_LARGE) {
            return *reinterpret_cast<const TagID *>(hud_globals->data + 0x48 + 0xC);
        }
        else {
            return *reinterpret_cast<const TagID *>(hud_globals->data + 0x58 + 0xC);
        }
    }

    static const TagID &get_generic_font_if_generic(const std::variant<TagID, GenericFont> &font) noexcept {
        auto *generic = std::get_if<1>(&font);
        if(generic) {
            return get_generic_font(*generic);
        }
        else {
            return std::get<0>(font);
        }
    }

    GenericFont generic_font_from_string(const char *str) noexcept {
        if(std::strcmp(str, "console") == 0) {
            return GenericFont::FONT_CONSOLE;
        }
        else if(std::strcmp(str, "system") == 0) {
            return GenericFont::FONT_SYSTEM;
        }
        else if(std::strcmp(str, "small") == 0) {
            return GenericFont::FONT_SMALL;
        }
        else if(std::strcmp(str, "large") == 0) {
            return GenericFont::FONT_LARGE;
        }
        return GenericFont::FONT_CONSOLE;
    }

    struct Text {
        // Text to display
        std::variant<std::string, std::wstring> text;

        // X coordinate
        std::int16_t x;

        // Y coordinate
        std::int16_t y;

        // Width of the box
        std::int16_t width;

        // Height of the box
        std::int16_t height;

        // Color of the text
        ColorARGB color;

        // Font to use
        TagID font;

        // Alignment of the font
        FontAlignment alignment;

        // Are we overriding this bad boy?
        LPD3DXFONT override;
    };

    template<typename String> struct TextRect {
        String text;
        std::int16_t x;
        std::int16_t y;
        std::int16_t width;
        std::int16_t height;
        FontAlignment align;
    };

    template<typename String> static std::vector<TextRect<String>> handle_formatting(String text, std::int16_t x, std::int16_t y, std::int16_t width, std::int16_t height, FontAlignment align, const std::variant<TagID, GenericFont> &font) {
        auto size = text.size();
        std::vector<TextRect<String>> fmt;
        if(size == 0) {
            return fmt;
        }

        std::size_t start = 0;
        auto font_height = font_pixel_height(font);
        int tabs = 0;
        auto original_align = align;

        auto append_thing = [&fmt, &x, &y, &width, &height, &align, &text, &tabs, &start](std::size_t end) {
            auto substr_size = end - start;
            if(substr_size == 0) {
                return;
            }
            auto &new_fmt = fmt.emplace_back();
            auto tab_width = (0.25 * width);
            new_fmt.x = x + tabs * tab_width;
            new_fmt.y = y;
            new_fmt.width = tabs ? tab_width : width;
            new_fmt.height = height;
            new_fmt.text = text.substr(start, substr_size);
            new_fmt.align = align;
        };

        for(std::size_t i = 0; i < size - 1 && height > 0; i++) {
            bool break_it_up = false;

            // Check if we hit an escape character
            if(text[i] == '|') {
                char control_char = text[i+1];
                switch(control_char) {
                    case 'n':
                    case 'r':
                    case 'l':
                    case 'c':
                    case 't':
                        break_it_up = true;
                        break;
                }

                // If we did, break it up
                if(break_it_up) {
                    append_thing(i);
                    switch(control_char) {
                        case 'n':
                            tabs = 0;
                            y += font_height;
                            height -= font_height;
                            align = original_align;
                            break;
                        case 'r':
                            tabs = 0;
                            align = FontAlignment::ALIGN_RIGHT;
                            break;
                        case 'l':
                            tabs = 0;
                            align = FontAlignment::ALIGN_LEFT;
                            break;
                        case 'c':
                            tabs = 0;
                            align = FontAlignment::ALIGN_CENTER;
                            break;
                        case 't':
                            align = FontAlignment::ALIGN_LEFT;
                            if(tabs < 4) {
                                tabs++;
                            }
                            break;
                    }
                    i += 1;
                    start = i + 1;
                }
            }
        }

        append_thing(size);
        return fmt;
    }

    static std::vector<Text> text_list;

    static FontData *font_data;
    FontData &get_current_font_data() noexcept {
        return *font_data;
    }

    extern "C" void display_text(const void *data, std::uint32_t xy, std::uint32_t wh, const void *function_to_use);

    static void *draw_text_8_bit = nullptr;
    static void *draw_text_16_bit = nullptr;

    // This is called every frame, giving us a chance to add text
    static void on_text() {
        if(text_list.size() == 0) {
            return;
        }

        // TODO: SIGNATURE FOR FONT DATA
        auto old_font_data = *font_data;

        for(auto &text : text_list) {
            if(text.override) {
                auto res = get_resolution();
                double scale = res.height / 480.0;

                // Get our rects up
                RECT rect;
                rect.left = text.x * scale;
                rect.right = (text.width) * scale;
                rect.top = (text.y) * scale;
                rect.bottom = (text.height) * scale;

                // Figure out shadow
                std::pair<int,int> shadow_offset;
                if(system_font_override == text.override) {
                    shadow_offset = system_font_shadow;
                }
                else if(small_font_override == text.override) {
                    shadow_offset = small_font_shadow;
                }
                else if(large_font_override == text.override) {
                    shadow_offset = large_font_shadow;
                }
                else if(console_font_override == text.override) {
                    shadow_offset = console_font_shadow;
                }
                bool draw_shadow = shadow_offset.first != 0 || shadow_offset.second != 0;
                RECT rshadow = rect;
                if(draw_shadow) {
                    rshadow.left += shadow_offset.first;
                    rshadow.right += shadow_offset.first;
                    rshadow.top += shadow_offset.second;
                    rshadow.bottom += shadow_offset.second;
                }

                auto align = DT_LEFT;

                // Colors
                D3DCOLOR color = D3DCOLOR_ARGB(
                    static_cast<int>(UINT8_MAX * text.color.alpha),
                    static_cast<int>(UINT8_MAX * text.color.red),
                    static_cast<int>(UINT8_MAX * text.color.green),
                    static_cast<int>(UINT8_MAX * text.color.blue)
                );
                D3DCOLOR color_shadow = D3DCOLOR_ARGB(
                    static_cast<int>(UINT8_MAX * (text.color.alpha * 0.75)),
                    static_cast<int>(UINT8_MAX * (text.color.red * 0.15)),
                    static_cast<int>(UINT8_MAX * (text.color.green * 0.15)),
                    static_cast<int>(UINT8_MAX * (text.color.blue * 0.15))
                );

                switch(text.alignment) {
                    case ALIGN_LEFT:
                        align = DT_LEFT;
                        break;
                    case ALIGN_CENTER:
                        align = DT_CENTER;
                        break;
                    case ALIGN_RIGHT:
                        align = DT_RIGHT;
                        break;
                }

                auto *u8 = std::get_if<std::string>(&text.text);
                auto *u16 = std::get_if<std::wstring>(&text.text);

                auto *override_font = text.override;

                if(u8) {
                    if(draw_shadow) {
                        override_font->DrawText(NULL, u8->data(), -1, &rshadow, align, color_shadow);
                    }
                    override_font->DrawText(NULL, u8->data(), -1, &rect, align, color);
                }
                else {
                    if(draw_shadow) {
                        override_font->DrawTextW(NULL, u16->data(), -1, &rshadow, align, color_shadow);
                    }
                    override_font->DrawTextW(NULL, u16->data(), -1, &rect, align, color);
                }
            }
            else {
                font_data->color = text.color;
                font_data->alignment = text.alignment;
                font_data->font = text.font;

                // Depending on if we're using 8-bit or 16-bit, draw stuff
                auto *u8 = std::get_if<std::string>(&text.text);
                auto *u16 = std::get_if<std::wstring>(&text.text);

                if(u8) {
                    display_text(u8->data(), text.x * 0x10000 + text.y, text.width * 0x10000 + text.height, draw_text_8_bit);
                }
                else {
                    display_text(u16->data(), text.x * 0x10000 + text.y, text.width * 0x10000 + text.height, draw_text_16_bit);
                }
            }
        }

        *font_data = old_font_data;
        text_list.clear();
    }

    std::int16_t font_pixel_height(const std::variant<TagID, GenericFont> &font) noexcept {
        // Find the font
        TagID font_tag = get_generic_font_if_generic(font);
        auto *override_font = get_override_font(font);

        if(override_font) {
            TEXTMETRIC tm;
            override_font->GetTextMetrics(&tm);
            auto res = get_resolution();
            return static_cast<int>((tm.tmAscent + tm.tmDescent) * 480 + 240) / res.height;
        }

        auto *tag = get_tag(font_tag);
        auto *tag_data = tag->data;
        return *reinterpret_cast<std::uint16_t *>(tag_data + 0x4) + *reinterpret_cast<std::uint16_t *>(tag_data + 0x6);
    }

    template <typename C> static void get_dimensions_template(std::int32_t &width, std::int32_t &height, const C *text) {

    }

    template<typename T> std::int16_t text_pixel_length_t(const T *text, const std::variant<TagID, GenericFont> &font) {
        // Find the font
        TagID font_tag = get_generic_font_if_generic(font);
        LPD3DXFONT override_font = get_override_font(font);

        if(override_font) {
            RECT rect;

            // Find spaces on right
            int trailing = 0;
            for(const T *q = text; *q; q++) {
                if(*q == ' ') {
                    trailing++;
                }
                else {
                    trailing = 0;
                }
            }

            // Find how long a space is (yes it's this much of a pain; please don't ask)
            override_font->DrawText(NULL, " .", -1, &rect, DT_CALCRECT, 0xFFFFFFFF);
            int space_dot = rect.right - rect.left;
            override_font->DrawText(NULL, ".", -1, &rect, DT_CALCRECT, 0xFFFFFFFF);
            int dot = rect.right - rect.left;
            int trailing_space = (space_dot - dot) * trailing;

            if(sizeof(T) == sizeof(char)) {
                override_font->DrawText(NULL, reinterpret_cast<const char *>(text), -1, &rect, DT_CALCRECT, 0xFFFFFFFF);
            }
            else {
                override_font->DrawTextW(NULL, reinterpret_cast<const wchar_t *>(text), -1, &rect, DT_CALCRECT, 0xFFFFFFFF);
            }

            auto res = get_resolution();
            return static_cast<int>((rect.right - rect.left + trailing_space) * 480 + 240) / res.height;
        }

        struct Character {
            std::uint16_t character;
            std::uint16_t character_width;
            char i_stopped_caring[16];
        };
        static_assert(sizeof(Character) == 0x14);

        // Get the tag
        auto *tag = get_tag(font_tag);

        // If it's not loaded, don't care
        if(tag->indexed && reinterpret_cast<std::uintptr_t>(tag->data) < 65536) {
            return 0;
        }
        std::int16_t length = 0;

        auto tag_chars_count = *reinterpret_cast<std::uint32_t *>(tag->data + 0x7C);
        auto *tag_chars = *reinterpret_cast<Character **>(tag->data + 0x7C + 4);

        while(*text != 0) {
            auto old_length = length;

            int char_length = 0;
            for(std::size_t i = 0; i < tag_chars_count; i++) {
                bool same = false;
                if(sizeof(T) == 1) {
                    same = *reinterpret_cast<const std::uint8_t *>(text) == tag_chars[i].character;
                }
                else {
                    same = *text == tag_chars[i].character;
                }
                if(same) {
                    char_length = tag_chars[i].character_width;
                    break;
                }
            }

            if(char_length > 0) {
                length += char_length;

                // If overflow, no point continuing.
                if(old_length > length) {
                    return old_length;
                }
            }

            text++;
        }

        return length;
    }

    std::int16_t text_pixel_length(const char *text, const std::variant<TagID, GenericFont> &font) noexcept {
        return text_pixel_length_t(text, font);
    }

    std::int16_t text_pixel_length(const wchar_t *text, const std::variant<TagID, GenericFont> &font) noexcept {
        return text_pixel_length_t(text, font);
    }

    float widescreen_width_480p = 640.0;

    void apply_text(std::variant<std::string, std::wstring> text, std::int16_t x, std::int16_t y, std::int16_t width, std::int16_t height, const ColorARGB &color, const std::variant<TagID, GenericFont> &font, FontAlignment alignment, TextAnchor anchor) noexcept {
        // Find the font
        TagID font_tag = get_generic_font_if_generic(font);
        LPD3DXFONT override_font = get_override_font(font);

        // Adjust the coordinates based on the given anchor
        switch(anchor) {
            case TextAnchor::ANCHOR_TOP_LEFT:
                break;
            case TextAnchor::ANCHOR_TOP_RIGHT:
                x = static_cast<std::int16_t>(widescreen_width_480p - x);
                break;
            case TextAnchor::ANCHOR_BOTTOM_RIGHT:
                x = static_cast<std::int16_t>(widescreen_width_480p - x);
                y = static_cast<std::int16_t>(480 - y);
                break;
            case TextAnchor::ANCHOR_BOTTOM_LEFT:
                y = static_cast<std::int16_t>(480 - y);
                break;
            case TextAnchor::ANCHOR_CENTER:
                y += 240;
                x += static_cast<std::int16_t>(widescreen_width_480p / 2.0f);
                break;
        }

        auto *u8 = std::get_if<0>(&text);
        auto *u16 = std::get_if<1>(&text);

        #define handle_formatting_call(what) handle_formatting(*what, x, y, width, height, alignment, font)

        if(u8) {
            for(auto &i : handle_formatting_call(u8)) {
                text_list.emplace_back(Text { i.text, i.x, i.y, static_cast<std::int16_t>(i.x + i.width), static_cast<std::int16_t>(i.y + i.height), color, font_tag, i.align, override_font } );
            }
        }

        if(u16) {
            for(auto &i : handle_formatting_call(u16)) {
                text_list.emplace_back(Text { i.text, i.x, i.y, static_cast<std::int16_t>(i.x + i.width), static_cast<std::int16_t>(i.y + i.height), color, font_tag, i.align, override_font } );
            }
        }

        #undef handle_formatting_call
    }

    template<class T> static void apply_text_quake_colors_t(T text, std::int16_t x, std::int16_t y, std::int16_t width, std::int16_t height, const ColorARGB &color, const std::variant<TagID, GenericFont> &font, TextAnchor anchor) {
        std::vector<std::tuple<char,T>> segments;

        // Find the font
        TagID font_tag = get_generic_font_if_generic(font);
        LPD3DXFONT override = get_override_font(font);

        // Adjust the base coordinates based on the given anchor
        switch(anchor) {
            case TextAnchor::ANCHOR_TOP_LEFT:
                break;
            case TextAnchor::ANCHOR_TOP_RIGHT:
                x = static_cast<std::int16_t>(widescreen_width_480p - x);
                break;
            case TextAnchor::ANCHOR_BOTTOM_RIGHT:
                x = static_cast<std::int16_t>(widescreen_width_480p - x);
                y = static_cast<std::int16_t>(480 - y);
                break;
            case TextAnchor::ANCHOR_BOTTOM_LEFT:
                y = static_cast<std::int16_t>(480 - y);
                break;
            case TextAnchor::ANCHOR_CENTER:
                y += 240;
                x += static_cast<std::int16_t>(widescreen_width_480p / 2.0f);
                break;
        }

        const auto *text_data = text.data();
        const auto *segment_start_position = text.data();
        bool last_char_was_caret = false;
        int current_color = 8;

        while(*text_data != 0) {
            // Check if this is a color code
            if(last_char_was_caret) {
                // Replace ^^ with ^ so ^ can be used in messages
                if(*text_data == '^') {
                    std::size_t length = text_data - text.data();
                    for(std::size_t i = length - 1; i < text.size(); i++) {
                        text[i] = text[i + 1];
                    }

                    text_data++;
                    last_char_was_caret = false;
                    continue;
                }

                const auto *last_char = text_data - 1;
                std::size_t length = last_char - segment_start_position;

                // If we do have a string, add it.
                if(length > 0) {
                    std::vector<typename T::value_type> substring(length + 1);
                    for(std::size_t i = 0; i < length; i++) {
                        substring[i] = segment_start_position[i];
                    }
                    substring[length] = 0;
                    segments.emplace_back(std::make_tuple(current_color, substring.data()));
                }

                // Record current color
                current_color = *text_data;
                segment_start_position = text_data + 1;

                last_char_was_caret = false;
            }
            else if(*text_data == '^') {
                last_char_was_caret = true;
            }

            text_data++;
        }

        // Add this last segment
        segments.push_back(std::make_tuple(current_color, T(segment_start_position)));

        for(auto &segment : segments) {
            auto color_int = std::get<char>(segment);
            auto &string = std::get<T>(segment);

            std::int16_t old_x = x;

            // Figure out what color is needed
            ColorARGB chosen_color = color;
            color_for_code(color_int, chosen_color);

            // Add the color to the list
            text_list.emplace_back(Text { string, x, y, static_cast<std::int16_t>(x + width), static_cast<std::int16_t>(y + height), chosen_color, font_tag, FontAlignment::ALIGN_LEFT, override });

            // Offset, giving up if we're overflowing or exceed y
            x += text_pixel_length(string.data(), font);
            if(old_x > x) {
                break;
            }
        }
    }

    void apply_text_quake_colors(std::wstring text, std::int16_t x, std::int16_t y, std::int16_t width, std::int16_t height, const ColorARGB &color, const std::variant<TagID, GenericFont> &font, TextAnchor anchor) noexcept {
        for(auto &i : handle_formatting(text, x, y, width, height, FontAlignment::ALIGN_LEFT, font)) {
            apply_text_quake_colors_t(i.text, i.x, i.y, i.width, i.height, color, font, anchor);
        }
    }

    void apply_text_quake_colors(std::string text, std::int16_t x, std::int16_t y, std::int16_t width, std::int16_t height, const ColorARGB &color, const std::variant<TagID, GenericFont> &font, TextAnchor anchor) noexcept {
        for(auto &i : handle_formatting(text, x, y, width, height, FontAlignment::ALIGN_LEFT, font)) {
            apply_text_quake_colors_t(i.text, i.x, i.y, i.width, i.height, color, font, anchor);
        }
    }

    extern "C" {
        const void *draw_text_8_bit_original;
        const void *draw_text_16_bit_original;
        extern "C" void display_text_8_scaled() noexcept;
        extern "C" void display_text_16_scaled() noexcept;
    }

    static void on_add_scene(LPDIRECT3DDEVICE9 device) noexcept {
        if(!dev) {
            dev = device;

            auto *ini = get_chimera().get_ini();
            auto scale = get_resolution().height / 480.0;

            #define generate_font(override_var, override_name, shadow) \
                if(ini->get_value_bool("font_override." override_name "_font_override").value_or(false)) { \
                    auto size = ini->get_value_long("font_override." override_name "_font_size").value_or(12); \
                    auto weight = ini->get_value_long("font_override." override_name "_font_weight").value_or(400); \
                    auto *family = ini->get_value("font_override." override_name "_font_family"); \
                    if(family == nullptr) { \
                        family = "Arial"; \
                    } \
                    shadow.first = ini->get_value_long("font_override." override_name "_font_shadow_offset_x").value_or(2) * (scale/2); \
                    shadow.second = ini->get_value_long("font_override." override_name "_font_shadow_offset_y").value_or(2) * (scale/2); \
                    D3DXCreateFont(device, static_cast<INT>(size * scale), 0, weight, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family, &override_var); \
                }

            generate_font(system_font_override, "system", system_font_shadow);
            generate_font(console_font_override, "console", console_font_shadow);
            generate_font(small_font_override, "small", small_font_shadow);
            generate_font(large_font_override, "large", large_font_shadow);

            #undef generate_font
        }
    }

    static void on_reset(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS *) {
        text_list.clear();
        if(small_font_override) {
            small_font_override->Release();
            small_font_override = nullptr;
        }
        if(large_font_override) {
            large_font_override->Release();
            large_font_override = nullptr;
        }
        if(console_font_override) {
            console_font_override->Release();
            console_font_override = nullptr;
        }
        if(system_font_override) {
            system_font_override->Release();
            system_font_override = nullptr;
        }
        dev = nullptr;
    }

    void setup_text_hook() noexcept {
        static Hook hook;
        auto *text_hook_addr = get_chimera().get_signature("text_hook_sig").data();
        write_jmp_call(reinterpret_cast<void *>(text_hook_addr), hook, reinterpret_cast<const void *>(on_text));
        add_frame_event(+[] { text_list.clear(); }); // unary+ on lamba with no captures decays to a function pointer
        draw_text_8_bit = get_chimera().get_signature("draw_8_bit_text_sig").data();
        draw_text_16_bit = get_chimera().get_signature("draw_16_bit_text_sig").data();
        font_data = *reinterpret_cast<FontData **>(get_chimera().get_signature("text_font_data_sig").data() + 13);

        static Hook draw_scale_8, draw_scale_16;
        // void write_function_override(void *jmp_at, Hook &hook, const void *new_function, const void **original_function);
        write_function_override(draw_text_8_bit, draw_scale_8, reinterpret_cast<const void *>(display_text_8_scaled), &draw_text_8_bit_original);
        write_function_override(draw_text_16_bit, draw_scale_16, reinterpret_cast<const void *>(display_text_16_scaled), &draw_text_16_bit_original);

        auto *chimera_ini = get_chimera().get_ini();
        if(chimera_ini->get_value_bool("font_override.enabled").value_or(false)) {
            auto fonts_dir = std::filesystem::path("fonts");
            if(std::filesystem::is_directory(fonts_dir)) {
                try {
                    for(auto &f : std::filesystem::directory_iterator(fonts_dir)) {
                        if(!f.is_regular_file() || (f.path().extension().string() != ".otf" && f.path().extension().string() != ".ttf" && f.path().extension().string() != ".ttc")) {
                            continue;
                        }

                        std::printf("Loading font %s...", f.path().string().c_str());
                        std::fflush(stdout);
                        if(AddFontResourceEx(f.path().string().c_str(), FR_PRIVATE, 0)) {
                            std::printf("done\n");
                        }
                        else {
                            std::printf("FAILED\n");
                            char error_message[256 + MAX_PATH];
                            std::snprintf(error_message, sizeof(error_message), "Failed to load %s.\nMake sure this is a valid font.", f.path().string().c_str());
                            MessageBox(nullptr, error_message, "Failed to load font", MB_ICONERROR | MB_OK);
                            std::exit(EXIT_FAILURE);
                        }
                    }
                }
                catch(std::exception &e) {
                    MessageBox(nullptr, e.what(), "Failed to iterate through font directory", MB_ICONERROR | MB_OK);
                    std::exit(EXIT_FAILURE);
                }
            }

            add_d3d9_end_scene_event(on_add_scene);
            add_d3d9_reset_event(on_reset);

            // Hell yes
            if(chimera_ini->get_value_bool("font_override.hud_text_enabled").value_or(false)) {
                initialize_hud_text();
            }
        }
    }

    extern "C" void scale_halo_drawn_text(std::uint8_t *) noexcept {
    }

    extern "C" void unscale_halo_drawn_text() noexcept {
    }
}
