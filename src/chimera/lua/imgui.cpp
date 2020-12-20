#include "imgui.hpp"
#include "lua_wrapper.hpp"
#include <cstddef>
#include <utility>
#include <type_traits>
#include <imgui.h>

// provide specializations for std::tuple_size and std::get for ImVec2 and ImVec4
// not ideal but certainly makes the code easier to work with
namespace std {
    template<>
    struct tuple_size<ImVec2> : std::integral_constant<std::size_t, 2> { };
    
    template<>
    struct tuple_size<ImVec4> : std::integral_constant<std::size_t, 4> { };
}

namespace {
    
    enum class FloatConst {
        negative_one,
        zero,
        half
    };
    
    enum class Vec2Const {
        negative_one_zero,
        zero,
    };
    
    
    const float& wrap_lookup(FloatConst item)
    {
        static constexpr float negative_one = -1.0f;
        static constexpr float zero = 0.0f;
        static constexpr float half = 0.5f;
        
        switch (item) {
        case FloatConst::negative_one: return negative_one;
        case FloatConst::zero:         return zero;
        case FloatConst::half:         return half;
        }
    }
    
    const ImVec2 vec2_negative_one_zero{-1, 0};
    const ImVec2 vec2_zero{0,0};
    const ImVec2& wrap_lookup(Vec2Const item)
    {
        switch (item) {
        case Vec2Const::negative_one_zero: return vec2_negative_one_zero;
        case Vec2Const::zero: return vec2_zero;
        }
    }
}

namespace {

// The following macros use __VA_OPT__ which was introduced in C++20.
// __VA_OPT__ was available in GCC since 8.1.
// If this is unsatisfactory, it can always be swapped back to the ## extension,
// which should work on older versions of GCC as well as clang pre-C++2a.

// Entry point with no overloads
#define ENTRY_STANDARD(name, ...) \
    {"ImGui" #name, lua_wrapper::wrap_function<&ImGui::name __VA_OPT__(,) __VA_ARGS__>}

// Entry point with no overloads, renaming
#define ENTRY_RENAME(rename, name, ...) \
    {"ImGui" #rename, lua_wrapper::wrap_function<&ImGui::name __VA_OPT__(,) __VA_ARGS__>}

// Entry point with overloads, renaming
#define ENTRY_OVERLOAD(rename, name, type, ...) \
    {"ImGui" #rename, lua_wrapper::wrap_function<static_cast<type>(&ImGui::name) __VA_OPT__(,) __VA_ARGS__>}
    
    using lua_wrapper::embedded_return;
    using lua_wrapper::invoke_native
    using lua_wrapper::array_view;

    // No std::optional for default arguments
    // Reimplement those in lua... when I can get around to it
    // Note on return values:
    //  Some arguments are used as output.
    //  The bindings simply return them back to lua as multiple results.
    //  The C function return value is ordered first, and output parameter(s)
    //  are ordered next, from left to right in the declaration, as return 
    //  results to lua.
    //  For instance: bool ImGui::Begin(const char *name, bool *open, ImGuiWindowFlags flags = 0)
    //      In C++, modifies *open to indicate that the window is open.
    //      The return value indicates whether or not the window is to be drawn.
    //  lua will receive the parameters as draw, open in that order.
    constexpr std::pair<const char *, lua_CFunction> functions[] {
        ENTRY_STANDARD(GetVersion),
        
        // Windows
        {"ImGuiBegin", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* name,
                     std::optional<bool> open,
                     std::optional<ImGuiWindowFlags> flags)
                    -> embedded_return<std::pair<
                        bool                /*draw*/, 
                        std::optional<bool> /*open*/>>
                {
                    const bool draw = ImGui::Begin(
                        name, 
                        open ? &*open : nullptr, flags.value_or(0));
                    return {std::make_pair(draw, open)};
                }
            );
        }},
        ENTRY_STANDARD(End),
        
        // Child Windows
        ENTRY_OVERLOAD(
            BeginChild, BeginChild
            bool(*)(const char*, const ImVec2&, bool, ImGuiWindowFlags),
            Vec2Const::zero, false, 0),
        ENTRY_STANDARD(EndChild),
        
        // Windows Utilities
        ENTRY_STANDARD(IsWindowAppearing),
        ENTRY_STANDARD(IsWindowCollapsed),
        ENTRY_STANDARD(IsWindowFocused, ImGuiFocusedFlags_None),
        ENTRY_STANDARD(IsWindowHovered, 0),
        ENTRY_STANDARD(GetWindowPos),
        ENTRY_STANDARD(GetWindowSize),
        
        ENTRY_STANDARD(SetNextWindowPos, ImGuiCond_None, Vec2Const::zero),
        ENTRY_STANDARD(SetNextWindowSize, ImGuiCond_None),
        // SetNextWindowSizeConstraints has callback, may be messy to implement
        ENTRY_STANDARD(SetNextWindowContentSize),
        ENTRY_STANDARD(SetNextWindowCollapsed, ImGuiCond_None),
        ENTRY_STANDARD(SetNextWindowFocus),
        ENTRY_STANDARD(SetNextWindowBgAlpha),
        
        ENTRY_OVERLOAD(
            SetWindowPos, SetWindowPos,
            void(*)(const ImVec2&, ImGuiCond),
            ImGuiCond_None),
        ENTRY_OVERLOAD(
            SetWindowSize, SetWindowSize,
            void(*)(const ImVec2&, ImGuiCond), 
            ImGuiCond_None),
        ENTRY_STANDARD(SetWindowCollapsed, ImGuiCond_None),
        ENTRY_OVERLOAD(SetWindowFocus, SetWindowFocus, void(*)()),
        ENTRY_STANDARD(SetWindowFontScale),
        
        // Windows Scrolling
        ENTRY_STANDARD(GetScrollX),
        ENTRY_STANDARD(GetScrollY),
        ENTRY_STANDARD(GetScrollMaxX),
        ENTRY_STANDARD(GetScrollMaxY),
        ENTRY_STANDARD(SetScrollX),
        ENTRY_STANDARD(SetScrollY),
        ENTRY_STANDARD(SetScrollHereX, FloatConst::half),
        ENTRY_STANDARD(SetScrollHereY, FloatConst::half),
        ENTRY_STANDARD(SetScrollFromPosX, FloatConst::half),
        ENTRY_STANDARD(SetScrollFromPosY, FloatConst::half),
        
        // Parameter stacks (shared)
        // missing: PushFont, PopFont, GetFont (ImFont* not yet exposed)
        //          GetFontTextUvWhitePixel
        //          GetColorU32 (no guarantee return value can be represented)
        ENTRY_OVERLOAD(
            PushStyleColor, PushStyleColor,
            void(*)(const ImVec4&)),
        ENTRY_STANDARD(PopStyleColor, 1),
        ENTRY_OVERLOAD(
            PushStyleVar, PushStyleVar1f,
            void(*)(ImGuiStyleVar, float)),
        ENTRY_OVERLOAD(
            PushStylevar, PushStyleVarVec2,
            void(*)(ImGuiStyleVar, const ImVec2&)),
        ENTRY_STANDARD(PopStyleVar, 1),
        ENTRY_STANDARD(GetStyleVec4), // keep this API around, but offer renamed version
        ENTRY_RENAME(GetStyleColor, GetStyleColorVec4),
        ENTRY_STANDARD(GetFontSize),
           
        // Parameters stack (current window)
        ENTRY_STANDARD(PushItemWidth),
        ENTRY_STANDARD(PopItemWidth),
        ENTRY_STANDARD(SetNextItemWidth),
        ENTRY_STANDARD(CalcItemWidth),
        ENTRY_STANDARD(PushTextWrapPos, float_zero),
        ENTRY_STANDARD(PopTextWrapPos),
        ENTRY_STANDARD(PushAllowKeyboardFocus),
        ENTRY_STANDARD(PopAllowKeyboardFocus),
        ENTRY_STANDARD(PushButtonRepeat),
        ENTRY_STANDARD(PopButtonRepeat),
        
        // Cursor / Layout
        ENTRY_STANDARD(Separator),
        ENTRY_STANDARD(SameLine, FloatConst::zero, FloatConst::negative_one),
        ENTRY_STANDARD(NewLine),
        ENTRY_STANDARD(Spacing),
        ENTRY_STANDARD(Dummy),
        ENTRY_STANDARD(Indent, FloatConst::zero),
        ENTRY_STANDARD(Unindent, FloatConst::zero),
        ENTRY_STANDARD(BeginGroup),
        ENTRY_STANDARD(EndGroup),
        ENTRY_STANDARD(GetCursorPos),
        ENTRY_STANDARD(SetCursorPos),
        ENTRY_STANDARD(SetCursorPosX),
        ENTRY_STANDARD(SetCursorPosY),
        ENTRY_STANDARD(GetCursorStartPos),
        ENTRY_STANDARD(GetCursorScreenPos),
        ENTRY_STANDARD(SetCursorScreenPos),
        ENTRY_STANDARD(AlignTextToFramePadding),
        ENTRY_STANDARD(GetTextLineHeight),
        ENTRY_STANDARD(GetTextLineHeightWithSpacing),
        ENTRY_STANDARD(GetFrameHeight),
        ENTRY_STANDARD(GetFrameHeightWithSpacing),
        
        // ID stack/scopes
        // missing various overloads of PushID, GetID
        // no guarantee GetID's return value can be represented
        ENTRY_OVERLOAD(
            PushID, PushID,
            void(*)(const char*)),
        ENTRY_STANDARD(PopID),
        
        // Widgets: Text
        {"ImGuiText", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char *text, std::optional<ImVec4> color) -> void
                {
                    if (color)
                        ImGui::TextColored(*color, "%s", text);
                    else
                        ImGui::Text("%s", text);
                }
            );
        }},
        {"ImGuiTextWrapped", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char *text) -> void
                {
                    ImGui::TextWrapped("%s", text);
                }
            );
        }},
        {"ImGuiLabelText", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char *label, const char *text) -> void
                {
                    ImGui::LabelText(label, "%s", text);
                }
            );
        }},
        {"ImGuiBulletText", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char *text) -> void
                {
                    ImGui::BulletText("%s", text);
                }
            );
        }},
        
        // Widgets: Main
        // missing Image, ImageButton (missing texture implementation)
        ENTRY_STANDARD(Button, Vec2Const::zero),
        ENTRY_STANDARD(SmallButton),
        ENTRY_STANDARD(InvisibleButton, ImGuiButtonFlags_None),
        ENTRY_STANDARD(ArrowButton),
        ENTRY_OVERLOAD(
            RadioButton, RadioButton,
            bool(*)(const char*, bool)),
        ENTRY_STANDARD(ProgressBar, Vec2Const::negative_one_zero, nullptr),
        ENTRY_STANDARD(Bullet),
        
        // Widgets: Combo Box
        ENTRY_STANDARD(BeginCombo, ImGuiComboFlags_None),
        ENTRY_STANDARD(EndCombo),
        
        // Widgets: Drags
        
        // Widgets: Sliders
        
        // Widgets: Input with Keyboard
        // Need to associate with each text box a buffer.
        // The buffer is returned each call, which becomes problematic as 
        // this may invoke an allocation/lookup within lua.
        // There is not really a great way of implementing this.
        // If we return a buffer object to lua, we need need to stringify that
        // buffer object, which will still be just as expensive.
        // Just need to give a big warning in the lua documentation.
        
        // Widgets: Color Editor/Picker
        // ColorEdit and ColorPicker use a float array as an output parameter
        // The signature decays these to float*, so we need to write a custom
        // interface for lua.
        {"ImGuiColorEdit3", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* label, 
                     std::array<float, 3> col, 
                     std::optional<ImGuiColorEditFlags> flags)
                    -> embedded_return<std::pair<bool, std::array<float, 3>>>
                {
                    auto result = ImGui::ColorEdit3(
                        label, 
                        col.data(), 
                        flags.value_or(ImGuiColorEditFlags_None));
                    return {result, col};
                }         
            );
        }},
        {"ImGuiColorEdit4", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* label, 
                     std::array<float, 4> col, 
                     std::optional<ImGuiColorEditFlags> flags)
                    -> embedded_return<std::pair<bool, std::array<float, 4>>>
                {
                    auto result = ImGui::ColorEdit4(
                        label, 
                        col.data(), 
                        flags.value_or(ImGuiColorEditFlags_None));
                    return {result, col};
                }         
            );
        }},
        {"ImGuiColorPicker3", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* label, 
                     std::array<float, 3> col, 
                     std::optional<ImGuiColorEditFlags> flags)
                    -> embedded_return<std::pair<bool, std::array<float, 3>>>
                {
                    auto result = ImGui::ColorEdit3(
                        label, 
                        col.data(), 
                        flags.value_or(ImGuiColorEditFlags_None));
                    return {std::make_pair(result, col)};
                }         
            );
        }},
        ENTRY_STANDARD(ColorButton, ImGuiColorEditFlags_None, Vec2Const::zero),
        ENTRY_STANDARD(SetColorEditOptions),
        
        // Widgets: Trees
        // @TODO CollapsingHeader overloads
        ENTRY_OVERLOAD(
            TreeNode, TreeNode,
            bool(*)(const char* label)),
        ENTRY_OVERLOAD(
            TreePush, TreePush,
            void(*)(const char* str_id)),
        ENTRY_STANDARD(TreePop),
        ENTRY_STANDARD(GetTreeNodeToLabelSpacing),
        ENTRY_STANDARD(SetNextItemOpen, ImGuiCond_None),
        
        // Widgets: Selectables
        ENTRY_OVERLOAD(
            Selectable, Selectable,
            bool(*)(
                const char* label, 
                bool selected              /*= false*/, 
                ImGuiSelectableFlags flags /*= ImGuiSelectableFlags_None*/,
                const ImVec2& size         /*= ImVec2(0, 0)*/),
            false, ImGuiSelectableFlags_None, Vec2Const::zero),
        
        // Widgets: List Boxes
        // Functions renamed from ListBoxHeader and ListBoxFooter to 
        // BeginListBox and EndListBox, following a note in the documentation.
        {"ImGuiListBox", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* label,
                     int current_item, // 1-based index, internally translated
                     const array_view<const char*> items,
                     std::optional<int> height_in_items  /*= -1*/)
                    -> embedded_return<std::pair<bool /*value changed*/, int /*current_item*/>>
                {
                    current_item -= 1; // translate from lua index (1-based)
                    bool value_changed = ImGui::ListBox(
                        label, 
                        &current_item, 
                        +[] (void* data, int idx, const char** out_text) {
                            const array_view<const char*>& items = 
                                *reinterpret_cast<const array_view<const char*>*>(data);
                            const auto element = items.get(idx);
                            if (element)
                                *out_text = *element;
                            return element.has_value();
                        },
                        &items, items.count(),
                        height_in_items.value_or(-1));
                    current_item += 1; // translate to lua index (1-based)
                    return {std::make_pair(value_changed, current_item)};
                }
            );
        }}
        ENTRY_OVERLOAD(
            BeginListBox, ListBoxHeader,
            bool(*)(const char* label, int items_count, int height_in_items),
            -1),
        ENTRY_RENAME(EndListBox, ListBoxFooter),
        
        // Widgets: Data Plotting
        // requires container support
        
        // Widgets: Menus
        ENTRY_STANDARD(BeginMenuBar),
        ENTRY_STANDARD(EndMenuBar),
        ENTRY_STANDARD(BeginMainMenuBar),
        ENTRY_STANDARD(EndMainMenuBar),
        ENTRY_STANDARD(BeginMenu, true),
        ENTRY_STANDARD(EndMenu),
        ENTRY_OVERLOAD(
            MenuItem, MenuItem,
            bool(*)(
                const char* label, 
                const char* shortcut /*= nullptr */,
                bool selected        /*= false   */,
                bool enabled         /*= true    */),
            nullptr,
            false,
            true),
        
        // Tooltips
        ENTRY_STANDARD(BeginTooltip),
        ENTRY_STANDARD(EndTooltip),
        {"ImGuiSetTooltip", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* tooltip) -> void
                {
                    ImGui::SetTooltip("%s", tooltip);
                }
            );
        }},
        
        // Popups, Modals
        // @TODO BeginPopupModal
        ENTRY_STANDARD(BeginPopup, ImGuiWindowFlags_None),
        ENTRY_STANDARD(EndPopup),
        ENTRY_STANDARD(OpenPopup, ImGuiPopupFlags_None),
        ENTRY_STANDARD(OpenPopupContextItem, nullptr, ImGuiPopupFlags_MouseButtonDefault_),
        ENTRY_STANDARD(CloseCurrentPopup),
        ENTRY_STANDARD(BeginPopupContextItem, nullptr, ImGuiPopupFlags_MouseButtonDefault_),
        ENTRY_STANDARD(BeginPopupContextWindow, nullptr, ImGuiPopupFlags_MouseButtonDefault_),
        ENTRY_STANDARD(BeginPopupContextVoid, nullptr, ImGuiPopupFlags_MouseButtonDefault_),
        ENTRY_STANDARD(IsPopupOpen, ImGuiPopupFlags_None),
        
        // Columns
        ENTRY_STANDARD(Columns, 1, nullptr, true),
        ENTRY_STANDARD(NextColumn),
        ENTRY_STANDARD(GetColumnIndex),
        ENTRY_STANDARD(GetColumnWidth, -1),
        ENTRY_STANDARD(SetColumnWidth),
        ENTRY_STANDARD(GetColumnOffset, -1),
        ENTRY_STANDARD(SetColumnOffset),
        ENTRY_STANDARD(GetColumnsCount),
        
        // Tab Bars, Tabs
        // @TODO BeginTabItem
        ENTRY_STANDARD(BeginTabBar, ImGuiTabBarFlags_None),
        ENTRY_STANDARD(EndTabBar),
        ENTRY_STANDARD(EndTabItem),
        ENTRY_STANDARD(SetTabItemClosed),
        
        // Logging/Capture
        // NO BINDINGS; NOT FOR USER APPLICATION
        
        // Drag and Drop (beta feature, not implementing)
        // NO BINDINGS; BETA FEATURE
        
        // Clipping
        ENTRY_STANDARD(PushClipRect),
        ENTRY_STANDARD(PopClipRect),
        
        // Focus, Activation
        ENTRY_STANDARD(SetItemDefaultFocus),
        ENTRY_STANDARD(SetKeyboardFocusHere, 0),
        
        // Item/Widgets Utilities
        ENTRY_STANDARD(IsItemHovered, ImGuiHoveredFlags_None),
        ENTRY_STANDARD(IsItemActive),
        ENTRY_STANDARD(IsItemFocused),
        ENTRY_STANDARD(IsItemClicked, ImGuiMouseButton_Left),
        ENTRY_STANDARD(IsItemVisible),
        ENTRY_STANDARD(IsItemEdited),
        ENTRY_STANDARD(IsItemActivated),
        ENTRY_STANDARD(IsItemDeactivated),
        ENTRY_STANDARD(IsItemDeactivatedAfterEdit),
        ENTRY_STANDARD(IsItemToggledOpen),
        ENTRY_STANDARD(IsAnyItemHovered),
        ENTRY_STANDARD(IsAnyItemActive),
        ENTRY_STANDARD(IsAnyItemFocused),
        ENTRY_STANDARD(GetItemRectMin),
        ENTRY_STANDARD(GetItemRectMax),
        ENTRY_STANDARD(GetItemRectSize),
        ENTRY_STANDARD(SetItemAllowOverlap),
        
        // Miscellaneous Utilities
        // missing: GetBackgroundDrawList, GetForegroundDrawList, 
        // GetDrawListSharedData, SetStateStorage, GetStateStorage,
        // CalcListClipping, BeginChildFrame, EndChildFrame
        {"ImGuiIsRectVisible", +[] (lua_State *L) -> int {
            // invokes IsRectVisible(const ImVec2&) or 
            //         IsRectVisible(const ImVec2&, cosnt ImVec2&) 
            // depending on the number of arguments provided
            return invoke_native(
                L,
                +[] (ImVec2 a, std::optional<ImVec2> b) -> bool
                {
                    return b ? IsRectVisible(a, *b) : IsRectVisible(a);
                };
            );
        }}
        ENTRY_STANDARD(GetTime),
        ENTRY_STANDARD(GetFrameCount),
        ENTRY_STANDARD(GetStyleColorName),
        
        // Text Utilities
        {"ImGuiCalcTextSize", +[] (lua_State *L) -> int {
            return invoke_native(
                L,
                +[] (const char* text, 
                     std::optional<bool> hide_text_after_double_hash,
                     std::optional<float> wrap_width)
                    -> ImVec2
                {
                    return CalcTextSize(
                        text, NULL, 
                        hide_text_after_double_hash.value_or(false),
                        wrap_width.value_or(-1.0f));
                }
            );
        }}
        
        // Color Utilities
        // NO BINDINGS: U32 not guaranteed representable in lua
        // may consider RGB/HSV conversions at a later date
        
        // Input Utilities: Keyboard
        // NO BINDINGS: backend/implementation utilities
        
        // Input Utilities: Mouse
        // NO BINDINGS: backend/implementation utilities
        
        // Clipboard Utilities
        // NO BINDINGS: may not want a script peaking into the clipboard
        // if desired, just uncomment the next two entries
        ENTRY_STANDARD(GetClipboardText),
        ENTRY_STANDARD(SetClipboardText),
        
        // Settings/.Ini Utilities
        // NO BINDINGS: backend/implementation utilities
        
        // Debug Utilities
        // NO BINDINGS: backend/implementation utilities
        
        // Memory Allocators
        // NO BINDINGS: implementation details
    };

#undef ENTRY_OVERLOAD
#undef ENTRY_RENAME
#undef ENTRY_STANDARD
}

namespace Chimera {
    void set_up_imgui_functions(lua_State *state, unsigned int /*api*/) noexcept {
        // prefer data-oriented approach for minimal code size
        for (auto [name, fn] : functions)
            lua_register(state, name, fn);
    }
}