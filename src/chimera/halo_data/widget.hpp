// SPDX-License-Identifier: GPL-3.0-only

#ifndef CHIMERA_WIDGET_HPP
#define CHIMERA_WIDGET_HPP

#include <cstdint>
#include <limits>
#include <utility>
#include "pad.hpp"

namespace Chimera {
    struct AnalogStickWidgetEvent {
        using Count = std::int16_t;
        static constexpr Count max_count = std::numeric_limits<Count>::max();
        static constexpr Count min_count = std::numeric_limits<Count>::min();

        // If an axis count reaches max_count or min_count, then the widget receives
        // the (left/right)_analog_stick_(up/down/left/right) events.

        Count vertical;   ///< The measure of the analog stick along the vertical axis.
        Count horizontal; ///< The measure of the analog stick along the horizontal axis.

        /** 
         * Tests if the analog stick is fully up.
         * @return `true` if the analog stick is fully up, otherwise `false`.
         */
        bool is_fully_up()    const { return vertical == max_count; }
        
        /** 
         * Tests if the analog stick is fully down.
         * @return `true` if the analog stick is fully down, otherwise `false`.
         */
        bool is_fully_down()  const { return vertical == min_count; }
        
        /** 
         * Tests if the analog stick is fully left.
         * @return `true` if the analog stick is fully left, otherwise `false`.
         */
        bool is_fully_left()  const { return horizontal == min_count; }
        
        /** 
         * Tests if the analog stick is fully right.
         * @return `true` if the analog stick is fully right, otherwise `false`.
         */
        bool is_fully_right() const { return horizontal == max_count; }
    };
    
    struct GamepadButtonWidgetEvent {
        using Duration = std::uint8_t;

        enum Button : std::int8_t {
            button_a = 0,
            button_b,
            button_x,
            button_y,
            button_black,
            button_white,
            button_left_trigger,
            button_right_trigger,
            button_dpad_up,
            button_dpad_down,
            button_dpad_left,
            button_dpad_right,
            button_start,
            button_back,
            button_left_thumb,
            button_right_thumb
        };

        Button   button;   ///< The gamepad button pressed.
        Duration duration; ///< The duration #button has been pressed for.
                           ///< This must be `1`, otherwise the event gets dropped during processing.
    };
    
    struct MouseButtonWidgetEvent {
        using Duration = std::uint8_t;
        static constexpr Duration duration_max = std::numeric_limits<Duration>::max();

        enum Button : std::int8_t {
            button_left_mouse = 0,
            button_middle_mouse,
            buttom_right_mouse,
            button_double_click
        };

        Button   button;   ///< The mouse button pressed.
        Duration duration; ///< The duration #button was held for, up to #duration_max.
    };
    
    struct WidgetEvent {
        enum EventType : std::int16_t {
            type_none = 0,
            type_left_analog_stick,  ///< An update from the left analog stick; use `event.analog`.
            type_right_analog_stick, ///< An update from the right analog stick, use `event.analog`.
            type_gamepad_button,     ///< A gamepad button has been pressed; use `event.gamepad`.
            type_mouse_button,       ///< A mouse button has been pressed or held; use `event.mouse`.
            type_custom_activation,  ///< Indicates that Halo should process event handlers with event type `custom_activation`.
        };

        union Event {
            std::int32_t             lparam;  ///< For compatibility with how Halo.
            AnalogStickWidgetEvent   analog;  ///< Parameters for an analog stick event.
            GamepadButtonWidgetEvent gamepad; ///< Parameters for a gamepad button event.
            MouseButtonWidgetEvent   mouse;   ///< Parameters for a mouse button event.
        };

        EventType    event_type;         ///< Indicates the variant #event.
        std::int16_t local_player_index; ///< The player the event is for, or `-1` for any player.
        Event        event;              ///< The event descriptor. The variant is determined by #event_type.
    }; 
    static_assert(sizeof(WidgetEvent) == 0x08);
    
    /**
     * Contains data necessary to store and process widget events.
     */
    struct WidgetEventGlobals {
        /**
         * A FIFO queue where the front of the queue is the last element in the array that has 
         * an `event_type` not equal to `type_none`.
         *
         * Pushing onto the queue involves a `memmove`, but Halo does not call it correctly and swaps
         * the destination and source operands.
         * As a result, when a widget event is pushed, Halo evicts the first element in the array.
         * Then it writes over the second-now-first element in the array.
         * Halo drops two events per push because of this bug.
         */
        using EventQueue = WidgetEvent[8];

        bool         initialized;
        bool         drop_events;
        std::int32_t input_time;  ///< The time of the last input, in milliseconds.
        std::int32_t update_time; ///< The time of the last update, in milliseconds.
        EventQueue   queues[4];   ///< The widget event queues, for each player.
        
        /**
         * Get the widget event globals
         * @return reference to the widget event globals
         */
        static WidgetEventGlobals &get_widget_event_globals();
    };
    static_assert(sizeof(WidgetEventGlobals) == 0x10C);
    
    /**
     * Mostly values pertaining to the widget cursor's positioning and movement.
     *
     * Widgets in vanilla Halo work in a 640 by 480 grid.
     * Chimera upgrades this with the widescreen fix.
     * #get_client_normalized_position() and #get_framebuffer_position() are provided, to ease translation.
     */
    struct WidgetCursorGlobals {
        bool unknown_lock;     ///< Set and checked to prevents recursion.
        bool use_get_cursor;   ///< Set to `true` to calculate cursor position using `GetCursor`.
        bool position_changed; ///< Set to `true` if the positon of the cursor changed this update.

        struct {
            std::int32_t x; ///< The horizontal coordinate of the cursor, in widget coordinates.
            std::int32_t y; ///< The vertical coordinate of the cursor, in widget coordinates.
        } position; ///< The position of the cursor, in widget coordinates.
        
        /**
         * Gets the position of the cursor in window client space, with each
         * coordinate normalized to the range `[0,1]`.
         *
         * The top-left corner of the client window is `(0, 0)`.
         * The bottom-right corner of the client window is `(1, 1)`.
         * 
         * @return The horizonal and vertical normalized coordinates, in that order.
         */
        std::pair<float, float> get_client_normalized_position();
        
        /**
         * Gets the position of the cursor with respect to the screen coordinates of the framebuffer.
         *
         * Unlike #get_client_relative_position(), this function respects the dimension of the framebuffer.
         * For a 1024 by 720 framebuffer, the bottom right corner will be `(1024, 720)`.
         *
         * @return The horizontal and vertical screen coordinates, in that order.
         */
        std::pair<float, float> get_framebuffer_position();
        
        /**
         * Get the widget cursor globals
         * @return reference to the widget cursor globals
         */
        static WidgetCursorGlobals &get_widget_cursor_globals();
    };
    static_assert(sizeof(WidgetCursorGlobals) == 0x0C);
    
    struct WidgetGlobals {
        struct EnqueuedErrorDescriptor {
            std::int16_t error_string; ///< Index of the error in the error strings tag.
            std::int16_t local_player; ///< Index of the local player the error is for.
            bool         display_modal;
            bool         display_paused;
        };
        static_assert(sizeof(EnqueuedErrorDescriptor) == 0x06);

        struct DeferredErrorDescriptor {
            std::int16_t error_string; ///< Index of the error in the error strings tag.
            bool         display_modal;
            bool         display_paused;
        };
        static_assert(sizeof(DeferredErrorDescriptor) == 0x04);

        void *top_widget_instance[1]; ///< The top-level widget instance for each local player.
        PAD(0x4); // probably another widget instance array of size 1

        std::int32_t current_time; // in milliseconds
        std::int32_t popup_display_time; // ticks remaining for popup (i think)
        std::int16_t error_message_index;
        std::int16_t widget_pause_counter;

        PAD(0x4); // float

        EnqueuedErrorDescriptor enqueued_errors[1]; // for each local player
        DeferredErrorDescriptor priority_warning; // takes precedence over enqueued_errors, always displays modal, non-paused
        DeferredErrorDescriptor deferred_for_cinematic_errors[1]; // for each local player

        void* initialization_thread; // no path sets this, real type is HANDLE*
        std::int16_t demo_error; // 1 = all progress will be lost, 2 = insert another quarter
                                 // only used on the widget update after initialization_thread exits
                                 // does anyone know if an arcade version of Halo 1 was planned?

        bool initialized;
        PAD(0x01); // bool
        PAD(0x01); // bool
        PAD(0x01); // bool
        PAD(0x01); // bool
        PAD(0x01); // bool
        
        /**
         * Get the widget globals
         * @return reference to the widget globals
         */
        static WidgetGlobals &get_widget_globals();
    };
    static_assert(sizeof(WidgetGlobals) == 0x34);
}

#endif