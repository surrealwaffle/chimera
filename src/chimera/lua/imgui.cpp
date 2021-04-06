#include "imgui.hpp"
#include "lua_wrapper.hpp"
#include <cstddef>
#include <algorithm>
#include <functional>
#incldue <mutex>
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

#define IMGUI_ENTRY_NAME(name) "ImGui" #name

#define ENTRY_CUSTOM(name, ...) {IMGUI_ENTRY_NAME(name), +[] (lua_State *L) -> int { return invoke_native(L, __VA_ARGS__); }}

// Entry point with no overloads
#define ENTRY_STANDARD(name, ...) \
    {IMGUI_ENTRY_NAME(name), lua_wrapper::wrap_function<&ImGui::name __VA_OPT__(,) __VA_ARGS__>}

// Entry point with no overloads, renaming
#define ENTRY_RENAME(rename, name, ...) \
    {IMGUI_ENTRY_NAME(rename), lua_wrapper::wrap_function<&ImGui::name __VA_OPT__(,) __VA_ARGS__>}

// Entry point with overloads, renaming
#define ENTRY_OVERLOAD(rename, name, type, ...) \
    {IMGUI_ENTRY_NAME(rename), lua_wrapper::wrap_function<static_cast<type>(&ImGui::name) __VA_OPT__(,) __VA_ARGS__>}
    
    using lua_wrapper::embedded_return;
    using lua_wrapper::invoke_native
    using lua_wrapper::array_view;

    // Note on return values:
    //  Some arguments are used as output.
    //  The bindings simply return them back to lua as multiple results.
    //  When these bindings return to lua, the return value of the API
    //  is ordered first followed by the output parameters from left to right.
    //
    //  For instance: bool ImGui::Begin(const char *name, bool *open, ImGuiWindowFlags flags = 0)
    //      In C++, modifies *open to indicate that the window is open.
    //      The return value indicates whether or not the window is to be drawn.
    //  lua will receive the parameters as draw, open in that order.
    constexpr std::pair<const char *, lua_CFunction> functions[] {
        ENTRY_STANDARD(GetVersion),
        
        // Windows
        ENTRY_CUSTOM(Begin, +[] (
            const char* name,
            std::optional<bool> open,
            std::optional<ImGuiWindowFlags> flags /*= 0*/)
            -> embedded_return<std::pair<
                bool                /*draw*/, 
                std::optional<bool> /*open*/>>
        {
            const bool draw = ImGui::Begin(
                name,
                open ? &*open : nullptr, 
                flags.value_or(0));
            return {std::make_pair(draw, open)};
        }),
        ENTRY_STANDARD(End),
        
        // Child Windows
        ENTRY_OVERLOAD(
            BeginChild, BeginChild
            bool(*)(const char*, const ImVec2&, bool, ImGuiWindowFlags),
            Vec2Const::zero, false, 0),
        ENTRY_STANDARD(EndChild),
        
        // Windows Utilities
        // Missing: SetNextWindowSizeConstraints (has callback)
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
        //          GetFontTexUvWhitePixel
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
        // printf-like formatting is not supported by this binding.
        // The user should format strings within lua instead.
        ENTRY_CUSTOM(Text, +[] (const char* text) -> void
        { 
            ImGui::TextUnformatted(text);
        }),
        ENTRY_CUSTOM(TextColored, +[] (ImVec4 col, const char* text) -> void
        { 
            ImGui::TextColored(col, "%s", text);
        }),
        ENTRY_CUSTOM(TextDisabled, +[] (const char* text) -> void
        {
            ImGui::TextDisabled("%s", text);
        }),
        ENTRY_CUSTOM(TextWrapped, +[] (const char* text) -> void
        {
            ImGui::TextWrapped("%s", text);
        }),
        ENTRY_CUSTOM(LabelText, +[] (const char* label, const char* text) -> void
        {
            ImGui::LabelText(label, "%s", text);
        }),
        ENTRY_CUSTOM(BulletText, +[] (const char* text) -> void
        {
            ImGui::BulletText("%s", text);
        }),
        
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
        // Missing: Multi-drags (DragFloat#, DragInt#) and DragScalar family
        //          DragIntRange2 and DragFloatRange2
        ENTRY_CUSTOM(DragFloat, +[] (
            const char* label,
            float v,
            std::optional<float> v_speed      /*= 1.0f*/,
            std::optional<float> v_min        /*= 0.0f*/,
            std::optional<float> v_max        /*= 0.0f*/,
            std::optional<const char*> format /*= "%.3f"*/,
            std::optional<float> power        /*= 1.0f*/)
            -> embedded_return<std::pair<
                bool  /*value_changed*/, 
                float /*v*/>>
        {
            bool value_changed = ImGui::DragFloat(
                label,
                &v,
                v_speed.value_or(1.0f),
                v_min.value_or(0.0f), v_max.value_or(0.0f),
                format.value_or("%.3f"),
                power.value_or(1.0f));
            return {std::make_pair(value_changed, v)};
        }),
        ENTRY_CUSTOM(DragInt, +[] (
            const char* label,
            float v,
            std::optional<float> v_speed      /*= 1.0f*/,
            std::optional<int> v_min          /*= 0.0f*/,
            std::optional<int> v_max          /*= 0.0f*/,
            std::optional<const char*> format /*= "%.3f"*/,
            std::optional<float> power        /*= 1.0f*/)
            -> embedded_return<std::pair<
                bool  /*value_changed*/, 
                float /*v*/>>
        {
            bool value_changed = ImGui::DragInt(
                label,
                &v,
                v_speed.value_or(1.0f),
                v_min.value_or(0.0f), v_max.value_or(0.0f),
                format.value_or("%.3f"),
                power.value_or(1.0f));
            return {std::make_pair(value_changed, v)};
        }),
        
        // Widgets: Sliders
        // Missing: SliderFloat#, SliderInt#, VSlider and SliderScalar families
        ENTRY_CUSTOM(SliderFloat, +[] (
            const char* label,
            float v,
            float v_min,
            float v_max,
            std::optional<const char*> format /*= "%.3f"*/,
            std::optional<float> power        /*= 1.0f*/)
            -> embedded_return<std::pair<
                bool  /*value_changed*/,
                float /*v*/>>
        {
            bool value_changed = ImGui::SliderFloat(
                label,
                &v,
                v_min, v_max,
                format.value_or("%.3f"),
                power.value_or(1.0f));
            return {std::make_pair(value_changed, v)};
        }),
        ENTRY_CUSTOM(SliderInt, +[] (
            const char* label,
            int v,
            int v_min,
            int v_max,
            std::optional<const char*> format /*= "%.3f"*/,
            std::optional<float> power        /*= 1.0f*/)
            -> embedded_return<std::pair<
                bool /*value_changed*/,
                int  /*v*/>>
        {
            bool value_changed = ImGui::SliderInt(
                label,
                &v,
                v_min, v_max,
                format.value_or("%.3f"),
                power.value_or(1.0f));
            return {std::make_pair(value_changed, v)};
        }),
            
        
        // Widgets: Input with Keyboard
        // Need to associate with each text box a buffer.
        // A string is returned each call, which becomes problematic as 
        // this may invoke an allocation/lookup within lua.
        // If we return a buffer object to lua, we need need to stringify that
        // buffer object, which will still be just as expensive.
        // Just need to give a big warning in the lua documentation.
        
        // Widgets: Color Editor/Picker
        // ColorEdit and ColorPicker use a float array as an output parameter.
        // In the function signature, these decay to float*, so we need a custom
        // interface for them.
        ENTRY_CUSTOM(ColorEdit3, +[] (
            const char* label,
            std::array<float, 3> col,
            std::optional<ImGuiColorEditFlags> flags /*= 0*/)
            -> embedded_return<std::pair<
                bool                 /*value_changed*/, 
                std::array<float, 3> /*col*/>>
        {
            auto val = ImGui::ColorEdit3(label, col.data(), flags.value_or(0));
            return {std::make_pair(val, col)};
        }),
        ENTRY_CUSTOM(ColorEdit4, +[] (
            const char* label,
            std::array<float, 4> col,
            std::optional<ImGuiColorEditFlags> flags /*= 0*/)
            -> embedded_return<std::pair<
                bool                 /*value_changed*/, 
                std::array<float, 4> /*col*/>>
        {
            auto val = ImGui::ColorEdit4(label, col.data(), flags.value_or(0));
            return {std::make_pair(val, col)};
        }),
        ENTRY_CUSTOM(ColorPicker3, +[] (
            const char* label,
            std::array<float, 3> col,
            std::optional<ImGuiColorEditFlags> flags /*= 0*/)
            -> embedded_return<std::pair<
                bool                 /*value_changed*/, 
                std::array<float, 3> /*col*/>>
        {
            auto val = ImGui::ColorPicker3(label, col.data(), flags.value_or(0));
            return {std::make_pair(val, col)};
        }),
        ENTRY_CUSTOM(ColorPicker4, +[] (
            const char* label,
            std::array<float, 4> col,
            std::optional<ImGuiColorEditFlags> flags /*= 0*/)
            -> embedded_return<std::pair<
                bool                 /*value_changed*/, 
                std::array<float, 4> /*col*/>>
        {
            auto val = ImGui::ColorPicker4(label, col.data(), flags.value_or(0));
            return {std::make_pair(val, col)};
        }),
        ENTRY_STANDARD(ColorButton, ImGuiColorEditFlags_None, Vec2Const::zero),
        ENTRY_STANDARD(SetColorEditOptions),
        
        // Widgets: Trees
        ENTRY_OVERLOAD(
            TreeNode, TreeNode,
            bool(*)(const char* label)),
        ENTRY_OVERLOAD(
            TreePush, TreePush,
            void(*)(const char* str_id)),
        ENTRY_STANDARD(TreePop),
        ENTRY_STANDARD(GetTreeNodeToLabelSpacing),
        {IMGUI_ENTRY_NAME(CollapsingHeader), +[] (lua_State *L) -> int
        {
            const bool has_three_args = lua_isnone(L, 3);
            
            static constexpr auto overload2 = +[] ( // (2-arg overload)
                const char* label,
                std::optional<ImGuiTreeNodeFlags> flags /*= 0*/)
                -> bool /*open*/
            {
                return ImGui::CollapsingHeader(label, flags.value_or(0));
            }

            static constexpr auto overload3 = +[] ( // (3-arg overload)
                const char* label,
                std::optional<bool> open,
                std::optional<ImGuiTreeNodeFlags> flags /*= 0*/)
                -> bool /*open*/
            {
                return ImGui::CollapsingHeader(
                    label,
                    open ? &*open : nullptr,
                    flags.value_or(0));
            };
            
            return has_three_args ? invoke_native(L, overload3)
                                  : invoke_native(L, overload2);
        }},
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
        ENTRY_CUSTOM(ListBox, +[] (
            const char*             label,
            int                     current_item,
            array_view<const char*> items,
            std::optional<int>      height_in_items /*= -1*/)
            -> embedded_return<std::pair<
                bool /*value_changed*/, 
                int current_item>>
        {
            auto getter = +[] (void* data, int idx, const char** out_text) {
                auto items =  *reinterpret_cast<array_view<const char*>*>(data);
                const auto element = items.get(idx);
                if (element)
                    *out_text = *element;
                return element.has_value();
            };
            
            --current_item; // translate from lua index (1-based to 0-based)
            bool value_changed = ImGui::ListBox(
                label,
                &current_item,
                getter,
                &items,
                height_in_items.value_or(-1));
            ++current_item; // translate to lua index (0-based to 1-based)
            return {std::make_pair(value_changed, current_item)};
        }),
        ENTRY_OVERLOAD(
            BeginListBox, ListBoxHeader,
            bool(*)(const char* label, int items_count, int height_in_items),
            -1),
        ENTRY_RENAME(EndListBox, ListBoxFooter),
        
        // Widgets: Data Plotting
        ENTRY_CUSTOM(PlotLines, +[] (
            const char* label,
            array_view<float> values,
            std::optional<int> values_offset        /*= 0*/,
            std::optional<const char*> overlay_text /*= nullptr*/,
            std::optional<float> scale_min          /*= FLT_MAX*/,
            std::optional<float> scale_max          /*= FLT_MAX*/,
            std::optional<ImVec2> graph_size        /*= ImVec2(0, 0)*/)
            -> void
        {
            auto getter = +[] (void* data, int idx) -> float {
                auto values = *reinterpret_cast<array_view<float>*>(data);
                return values.get(idx);
            };
            
            ImGui::PlotLines(
                label,
                getter,
                values.count(),
                values_offset.value_or(0),
                overlay_text.value_or(nullptr),
                scale_min.value_or(FLT_MAX),
                scale_max.value_or(FLT_MAX),
                graph_size.value_or(ImVec2(0, 0)));
        }),
        ENTRY_CUSTOM(PlotHistogram, +[] (
            const char* label,
            array_view<float> values,
            std::optional<int> values_offset        /*= 0*/,
            std::optional<const char*> overlay_text /*= nullptr*/,
            std::optional<float> scale_min          /*= FLT_MAX*/,
            std::optional<float> scale_max          /*= FLT_MAX*/,
            std::optional<ImVec2> graph_size        /*= ImVec2(0, 0)*/)
            -> void
        {
            auto getter = +[] (void* data, int idx) -> float {
                auto values = *reinterpret_cast<array_view<float>*>(data);
                return values.get(idx);
            };
            
            ImGui::PlotHistogram(
                label,
                getter,
                values.count(),
                values_offset.value_or(0),
                overlay_text.value_or(nullptr),
                scale_min.value_or(FLT_MAX),
                scale_max.value_or(FLT_MAX),
                graph_size.value_or(ImVec2(0, 0)));
        }),
        
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
        // missing: BeginPopupModal
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
        ENTRY_STANDARD(BeginTabBar, ImGuiTabBarFlags_None),
        ENTRY_STANDARD(EndTabBar),
        ENTRY_CUSTOM(BeginTabItem, +[] (
            const char* label,
            std::optional<bool> open               /*if unsupplied, tab has no close button*//,
            std::optional<ImGuiTabItemFlags> flags /* = 0*/)
            -> embedded_return<std::pair<
                bool                /*selected*/, 
                std::optional<bool> /*open*/>>
        {
            bool selected = ImGui::BeginTabItem(
                label, 
                open ? &*open : nullptr,
                flags.value_or(0));
            return {std::make_pair(selected, open)};
        }
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
        ENTRY_CUSTOM(IsRectVisible, +[] (ImVec2 a, std::optional<ImVec2> b) -> bool
        {
            return b ? ImGui::IsRectVisible(a, *b)
                     : ImGui::IsRectVisible(a);
        }),
        ENTRY_STANDARD(GetTime),
        ENTRY_STANDARD(GetFrameCount),
        ENTRY_STANDARD(GetStyleColorName),
        
        // Text Utilities
        ENTRY_CUSTOM(CalcTextSize, +[] (
            const char* text,
            std::optional<bool> hide_text_after_double_hash /*= false*/,
            std::optional<float> wrap_width                 /*= -1.0f*/)
            -> ImVec2
        {
            return ImGui::CalcTextSize(
                text, NULL,
                hide_text_after_double_hash.value_or(false),
                wrap_width.value_or(-1.0f));
        }),
        
        // Color Utilities
        // NO BINDINGS: U32 not guaranteed representable in lua
        
        // Input Utilities: Keyboard
        // NO BINDINGS: backend/implementation utilities
        
        // Input Utilities: Mouse
        // NO BINDINGS: backend/implementation utilities
        
        // Clipboard Utilities
        // NO BINDINGS: may not want a script peaking into the clipboard
        // If these functions are deemed acceptable, just uncomment the next
        // two lines.
        // ENTRY_STANDARD(GetClipboardText),
        // ENTRY_STANDARD(SetClipboardText),
        
        // Settings/.Ini Utilities
        // NO BINDINGS: backend/implementation utilities
        
        // Debug Utilities
        // NO BINDINGS: backend/implementation utilities
        
        // Memory Allocators
        // NO BINDINGS: backend/implementation utilities
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