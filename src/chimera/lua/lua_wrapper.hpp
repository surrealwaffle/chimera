// SPDX-License-Identifier: GPL-3.0-only

#ifndef LUA_WRAPPERS_HPP
#define LUA_WRAPPERS_HPP

#include <functional>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <string>
#include <string_view>

#include <lauxlib.h>
#include <lualib.h>

/* FILE SUMMARY
 * This file provides some metaprogramming wrappers to leverage the compiler
 * into generating interfaces for C(++) functions for lua.
 * More interfacing may be added here as requested/needed.
 *
 * This wrapper targets C++17, so the metaprogramming involved may look 
 * terrifying for the uninitiated. Concepts and relaxed NTTP requirements would
 * handily improve readability and functionality.
 * To alleviate C++17 restrictions, ADL is employed as an expansion point so 
 * that if/when Chimera transitions to C++20, NTTP relaxations can be leveraged
 * without rewriting this header. See the documentation for wrap_with_defaults.
 *
 * A user of the facilities of this header should only concern themselves with:
 *  * wrap_function,
 *  * wrap_with_defaults, and
 *  * invoke_native.
 * wrap_function is composes wrap_with_defaults and invoke_native, but all
 * three are provided for reuse.
 */

namespace Chimera { namespace lua_wrapper {

    namespace detail {
        template<auto Function, auto... Defaults>
        struct default_wrapper;
        
        template<typename R, typename... Arg, std::size_t... ArgI>
        int invoke_native_impl(
            lua_State *L, 
            R(*function)(Arg...), 
            std::index_sequence<ArgI...>);
    } // namespace (Chimera::lua_wrapper::)detail

    /**
     * Wraps @a Function so that the last `sizeof...(Default)` arguments
     * are wrapped around `std::optional`s that take a value from @a Default
     * if supplied `std::nullopt`.
     *
     * This aliases to a function pointer to the wrapping function.
     *
     * The specific transformation of a parameter type goes as follows:
     *  * `std::optional`-related types remain unchanged;
     *  * `T&` becomes `std::optional<std::reference_wrapper<T>>`;
     *  * `T&&` becomes `std::optional<T>`;
     *  * `T` becomes `std::optional<T>`.
     * This wrapper is not exactly transparent, because reference wrappers 
     * cannot be constructed from rvalue references.
     * This is a deliberate decision to retain the ability to pass by reference.
     * 
     * An optional argument `arg` with default value `default` is supplied to
     * @a Function as if by 
     * `std::forward<decltype(arg)>(arg).value_or(wrap_lookup(default))`.
     * 
     * `wrap_lookup` can be customized using ADL to support types that cannot
     * be used as non-type template parameters, e.g. defining an `enum`
     * that is used to lookup to a non-`constexpr`able type.
     * By default, `wrap_lookup(d)` returns a reference to `d`.
     */
    template<auto Function, auto... Default>
    inline constexpr auto wrap_with_defaults =
        &detail::default_wrapper<Function, Default...>::invoke;
    
    /**
     * Invokes @a function on the top frame of the stack of @a L.
     *
     * An argument of type `T` is processed by the expression
     * `frame_interface<typename make_stored<T>::type>::assign(L, n, v)`,
     * where `n` is the Lua argument index and `v` is a reference to the value
     * to be assigned.
     *
     * The return value is processed by the expression
     * `frame_interface<typename make_stored<R>::type>::push(L, function(...))`,
     * where `...` are the arguments as processed above.
     *
     * @sa frame_interface
     * @sa embedded_return
     * @sa make_stored
     *
     * @return The number of return values pushed onto the stack.
     */
    template<typename R, typename... Arg>
    int invoke_native(lua_State *L, R(*function)(Arg...))
    {
        return detail::invoke_native_impl(
            L, 
            function, 
            std::make_index_sequence<sizeof...(Arg)>{});
    }
    
    /**
     * Statically binds `wrap_with_defaults<Function, Default...>` to the 
     * second parameter of `invoke_native`.
     *
     * @sa wrap_with_defaults
     * @sa invoke_native
     */
    template<auto Function, auto... Default>
    int wrap_function(lua_State *L)
    {
        return invoke_native(L, wrap_with_defaults<Function, Default...>);
    }
    
    /** 
     * Specifies how values of type `T` are to be pushed onto and processed from
     * a lua stack frame.
     *
     * Users wishing to extend type support for the wrappers may 
     * specialize this template, using the second type parameter for SFINAE
     * techniques including `std::enable_if` and `std::void_t`, or leave as 
     * `void` if SFINAE is not employed.
     *
     * Static functions `push` and `assign` may be marked `deleted` 
     * independently to indicate that the operation is invalid.
     *
     * Specializations for a number of types are already provided for:
     *  * integer and `char`-related types,
     *  * floating point types (`std::is_floating_point`),
     *  * `std::string` and `std::string_view`,
     *  * `std::optional`s of supported types,
     *  * tuple-like objects of supported types.
     * 
     * A type `T` is considered tuple-like if `std::tuple_size<T>::value` is 
     * not ill-formed as an unevaluated operand.
     * Tuples are popped and pushed as if they were array-like tables, using 
     * 1-based indices.
     * 
     * To return a `tuple` of type `T` as multiple results, return instead 
     * `embedded_return<T>{tuple}`. Nested tuples will not be embedded,
     * unless marked in a similar way.
     */
    template<typename T, typename = void>
    struct frame_interface {        
        /** 
         * Pushes @a value onto the top stack frame as a return value. 
         * 
         * @return The number of results returned to lua.
         */
        static int push(lua_State *L, const T& value) = delete;
        
        /**
         * Processes the argument at index @a arg into @a dest.
         *
         * @return `0` on success, and non-zero on error.
         */
        static int assign(lua_State *L, int arg, T& dest) = delete;
        
    };
    
    /**
     * As the function return type, indicates to @ref invoke_native that 
     * the wrapped @a Tuple should be returned as multiple results rather than 
     * as a table.
     */
    template<typename Tuple>
    struct embedded_return {
        Tuple value;
        
        Tuple& operator*() noexcept { return value; }
        const Tuple& operator*() const noexcept { return value; }
    };
    
    /** 
     * Provides member `type` which is @a T with references, reference wrappers,
     * optionals, and cv-qualifiers removed (recursively).
     */
    template<typename T>
    struct make_stored {
        using type = T;
    };
    
    /**
     * As a function argument to @ref invoke_native, delegates read-access 
     * to a table that is read like an array of @a T.
     *
     * Delegation translates between 0-based and 1-based indices.
     */
    template<typename T>
    struct array_view {
        using stored = typename make_stored<T>::type;
        
        lua_State *L;
        int        arg;
        
        /** 
         * Accesses the `i`-th element of the table, in 0-based indices.
         *
         * @return The `i`-th element, or `std::nullopt` if table access 
         *         returned `nil`.
         */
        std::optional<stored> get(lua_Integer i) const
        {
            std::optional<stored> result;
            
            // potential UB here
            lua_geti(L, arg, i + 1);
            if (frame_interface<decltype(result)>::assign(L, -1, result))
                luaL_error(L, "array_view access failed");
            lua_pop(L, 1);
            
            return result;
        }
        
        template<typename UnaryFunction>
        UnaryFunction for_each(UnaryFunction f)
        {
            int i = 0;
            while (auto r = get(i)) {
                f(*r);
                ++i;
            }
            
            return std::move(f);
        }
        
        int count() const
        {
            // potential UB here, same as in get
            int result = 0;
            for_each([&result] (auto&&) { ++result; });
            return result;
        }
    };

    template<typename T>
    struct make_stored<std::reference_wrapper<T>> {
        using type = typename make_stored<T>::type;
    };

    template<typename T>
    struct make_stored<std::optional<T>> {
        using type = std::optional<typename make_stored<T>::type>;
    };

    template<typename T>
    struct make_stored<const T> {
        using type = typename make_stored<T>::type;
    };

    template<typename T>
    struct make_stored<volatile T> {
        using type = typename make_stored<T>::type;
    };

    template<typename T>
    struct make_stored<const volatile T> {
        using type = typename make_stored<T>::type;
    };

    template<typename T>
    struct make_stored<T&> {
        using type = typename make_stored<T>::type;
    };
    
    // -------------------------------------------------------------------------
    // IMPLEMENTATION
    // ALL SANITY ABANDON
    // YE WHO ENTER HERE
    
    // wrap_with_defaults implementation
    namespace detail {
        template<typename T>
        constexpr T&& wrap_lookup(T&& t) noexcept
        { 
            return std::forward<T>(t); 
        }
        
        template<typename T>
        struct is_optional_related : std::false_type { };
        
        template<typename T>
        struct is_optional_related<std::optional<T>> : std::true_type { };
        
        template<typename T>
        struct is_optional_related<const T> : is_optional_related<T> { };
        
        template<typename T>
        struct is_optional_related<volatile T> : is_optional_related<T> { };
        
        template<typename T>
        struct is_optional_related<const volatile T> : is_optional_related<T> { };
        
        template<typename T>
        struct is_optional_related<T&> : is_optional_related<T> { };
        
        template<typename T>
        struct is_optional_related<T&&> : is_optional_related<T> { };
        
        template<
            bool DoOptional, 
            class T, 
            bool IsOptional = is_optional_related<T>::value>
        struct make_conditionally_optional { using type = T; };
        
        template<class OptionalType>
        struct make_conditionally_optional<true, OptionalType, true> {
            using type = OptionalType;
        };

        template<class T>
        struct make_conditionally_optional<true, T&, false> {
            using type = std::optional<std::reference_wrapper<T>>;
        };
        
        template<class T>
        struct make_conditionally_optional<true, T&&, false> {
            using type = std::optional<T>;
        };
        
        template<class T>
        struct make_conditionally_optional<true, T, false> {
            using type = std::optional<T>;
        };
        
        // supply ParamIndexSequence as void to deduce it
        template<
            auto Function, typename FunctionType, typename ParamIndexSequence,
            auto... Default>
        struct default_wrapper_impl;
        
        template<auto Function, auto... Default>
        struct default_wrapper
            : default_wrapper_impl<
                Function, decltype(Function), void, 
                Default...> { };
        
        template<
            auto Function, typename R, typename... Arg,
            auto... Default>
        struct default_wrapper_impl<Function, R(*)(Arg...), void, Default...>
            : default_wrapper_impl<
                Function, R(*)(Arg...), 
                std::make_index_sequence<sizeof...(Arg)>,
                Default...> { };
        
        template<
            auto Function, typename R, typename... Arg, std::size_t... I,
            auto... Default>
        struct default_wrapper_impl<
            Function, R(*)(Arg...), 
            std::index_sequence<I...>, 
            Default...>
        {
            static_assert(sizeof...(Default) <= sizeof...(Arg));
            
            static inline constexpr std::tuple default_arguments =
                std::make_tuple(Default...);
            static inline constexpr std::size_t first_optional = 
                sizeof...(Arg) - sizeof...(Default);
            
            template<std::size_t ArgI, typename ArgT>
            using transform_arg = typename make_conditionally_optional<
                (ArgI >= first_optional), 
                ArgT>::type;
            
            static R invoke(transform_arg<I, Arg>... arg)
            {
                auto get_arg = [] (auto index, auto&& arg) -> decltype(auto) {
                    if constexpr (index < first_optional) {
                        return std::forward<decltype(arg)>(arg);
                    } else {
                        constexpr auto default_index = index - first_optional;
                        constexpr auto& default_arg =
                            std::get<default_index>(default_arguments);
                        return std::forward<decltype(arg)>(arg)
                                .value_or(wrap_lookup(default_arg));
                        
                    }
                };
                return Function(
                    get_arg(
                        std::integral_constant<std::size_t, I>{},
                        std::forward<decltype(arg)>(arg)
                    )...
                );
            }
        };
    } // namespace (Chimera::lua_wrapper::)detail

    // invoke_native implementation
    namespace detail {
        // used to ensure integers remain representable in destination types
        // C++20 has these in <utility> under cmp_*
        template<typename A, typename B>
        constexpr auto cmp_equal(A a, B b) noexcept 
            -> std::enable_if_t<
                std::is_integral_v<A> && std::is_integral_v<B>, 
                bool>
        {
            if constexpr (std::is_signed_v<A> == std::is_signed_v<B>) {
                return a == b;
            } else if constexpr (std::is_signed_v<A>) {
                using UnsignedA = std::make_unsigned_t<A>;
                return a < 0 ? false : static_cast<A>(a) == b;
            } else {
                return cmp_equal(b, a);
            }
        }
        
        template<typename A, typename B>
        constexpr auto cmp_less(A a, B b)
            -> std::enable_if_t<
                std::is_integral_v<A> && std::is_integral_v<B>, 
                bool>
        {
            if constexpr (std::is_signed_v<A> == std::is_signed_v<B>) {
                return a == b;
            } else if constexpr (std::is_signed_v<A>) {
                using UnsignedA = std::make_unsigned_t<A>;
                return a < 0 ? true : static_cast<UnsignedA>(a) < b;
            } else {
                using UnsignedB = std::make_unsigned_t<B>;
                return b < 0 ? false : a < static_cast<UnsignedB>(b);
            }
        }
        
        template<typename A, typename B>
        constexpr auto cmp_less_equal(A a, B b)
            -> std::enable_if_t<
                std::is_integral_v<A> && std::is_integral_v<B>, 
                bool>
        {
            return !cmp_less(a, b);
        }
        
        template<typename T, typename = void>
        struct return_value_size : std::integral_constant<std::size_t, 1> { };
        
        template<typename Void>
        struct return_value_size<Void, std::enable_if_t<std::is_void_v<Void>>
            : std::integral_constant<std::size_t, 0>
        { };
        
        template<typename Tuple>
        struct return_value_size<
            Tuple, 
            std::void_t<decltype(std::tuple_size<Tuple>::value)>>
            : std::integral_constant<std::size_t, std::tuple_size<Tuple>::value>
        { };
        
        template<typename R, typename... Arg, std::size_t... ArgI>
        int invoke_native_impl(
            lua_State *L, 
            R(*function)(Arg...), 
            std::index_sequence<ArgI...>)
        {
            using stored_return_type = typename make_stored<R>::type;
            
            static constexpr auto arg_count = sizeof...(Arg);
            static constexpr auto ret_count = return_value_size<stored_return_type>::value;
            
            static_assert(cmp_less_equal(arg_count, std::numeric_limits<int>::max()));
            static_assert(cmp_less_equal(ret_count, std::numeric_limits<int>::max()));
            
            auto assign = [L] (int arg, auto& e) -> int
            {
                using element = std::remove_reference_t<decltype(e)>;
                return frame_interface<element>::assign(L, arg, e);
            };
            
            auto push = [L] (auto& e) -> int
            {
                using element = std::remove_reference_t<decltype(e)>;
                return frame_interface<element>::push(L, e);
            }
            
            std::tuple<typename make_stored<Arg>::type> args;
            if (!(assign(static_cast<int>(ArgI) + 1, get<I>(args)) == 0 && ...))
                return luaL_error("failed to process arguments");
            
            if constexpr (std::is_void_v<R>)
            {
                std::apply(function, args);
                return 0;
            } else {
                return push(std::apply(function, args));
            }
        }
    }

    // frame interfaces
    
    // ------------------------------------------
    // Unsupported: character types (except char/unsigned char)
    // lua APIs accept char (strings) but no other character type.
    // User of the wrappers should cast to an appropriate integer type.    
    template<>
    struct frame_interface<char8_t, void> {
        static int push(lua_State *L, const char8_t& value) = delete;
        
        static int assign(lua_State *L, int arg, char8_t& dest) = delete;
    };
    
    template<>
    struct frame_interface<wchar_t, void> {
        static int push(lua_State *L, const wchar_t& value) = delete;
        
        /** Processes the argument at index @a arg into @a dest. */
        static int assign(lua_State *L, int arg, wchar_t& dest) = delete;
    };
    
    template<>
    struct frame_interface<char16_t, void> {
        static int push(lua_State *L, const char16_t& value) = delete;
        
        static int assign(lua_State *L, int arg, char16_t& dest) = delete;
    };
    
    template<>
    struct frame_interface<char32_t, void> {
        static int push(lua_State *L, const char32_t& value) = delete;
        
        static int assign(lua_State *L, int arg, char32_t& dest) = delete;
    };
    
    // ------------------------------------------
    // Supported types:
    // bool
    // Integral types except char8_t, wchar_t, char16_t, char32_t
    //  exceptions are handled by the above specializations
    // floating point types
    // std::string, std::string_view, const char*
    // std::optional of supported types
    // tuple-like and embedded_return of tuple like
    
    template<>
    struct frame_interface<
        bool,
        void>
    {
        static int push(lua_State *L, bool value)
        {
            lua_pushboolean(L, value);
            return 1;
        }
        
        static int assign(lua_State *L, int arg, bool &dest)
        {
            // design choice: check lua_isboolean?
            
            dest = lua_toboolean(L, arg);
            return 0;
        }
    };
    
    template<typename Integral>
    struct frame_interface<
        Integral, 
        std::enable_if_t<std::is_integral_v<Integral>>>
    {
        static int push(lua_State *L, Integral value)
        {
            using detail::cmp_less_equal;
            
            static constexpr auto min = std::numeric_limits<lua_Integer>::min();
            static constexpr auto max = std::numeric_limits<lua_Integer>::max();
            
            if (!(cmp_less_equal(min, value) && cmp_less_equal(value, max)))
                return luaL_error(L, "value out of lua_Integer range");
            
            lua_pushinteger(L, static_cast<lua_Integer>(value));
            return 1;
        }
        
        static int assign(lua_State *L, int arg, Integral& dest)
        {
            using detail::cmp_less_equal;
            
            static constexpr auto min = std::numeric_limits<Integral>::min();
            static constexpr auto max = std::numeric_limits<Integral>::max();
            
            const auto v = luaL_checkinteger(L, arg);
            if (!(cmp_less_equal(min, v) && cmp_less_equal(v, max)))
                return luaL_error(L, "lua_Integer value out of range of dest");
            
            dest = static_cast<Integral>(v);
            return 0;
        }
    };
    
    template<typename Float>
    struct frame_interface<
        Float,
        std::enable_if_t<std::is_floating_point_v<Float>>
    {        
        static int push(lua_State *L, Float value)
        {
            lua_pushnumber(L, static_cast<lua_Number>(value));
            return 1;
        }
        
        static int assign(lua_State *L, int arg, Float& dest)
        {
            dest = static_cast<Float>(luaL_checknumber(L, arg));
            return 0;
        }
    };   
    
    template<class Traits>
    struct frame_interface<
        std::basic_string_view<char, Traits>,
        void> 
    {
        using string_view = std::basic_string_view<char, Traits>;
        
        static int push(lua_State *L, string_view value)
        {
            lua_pushlstring(L, value.data(), value.length());
            return 1;
        }
        
        static int assign(lua_State *L, int arg, string_view& dest)
        {
            std::size_t len = 0;
            const char* str = luaL_checklstring(L, arg, &len);
            dest = string_view(str, len);
            return 0;
        }
    };
    
    template<class Traits, class Allocator>
    struct frame_interface<
        std::basic_string<char, Traits, Allocator>,
        void> 
    {
        using string = std::basic_string<char, Traits, Allocator>;
        using string_view = std::basic_string_view<char, Traits>;
        
        static int push(lua_State *L, const string& value)
        {
            return frame_interface<string_view>::push(L, value);
        }
        
        static int assign(lua_State *L, int arg, string& dest) 
        {
            string_view view;
            const auto result = frame_interface<string_view>::assign(L, view);
            if (result)
                dest = view;
            return 0;
        }
    };
    
    template<>
    struct frame_interface<
        const char*,
        void>
    {
        static int push(lua_State *L, const char* value)
        {
            if (value)
                lua_pushstring(L, value);
            else
                lua_pushnil(L);
            return 1;
        }
        
        static int assign(lua_State *L, int arg, const char* &dest)
        {
            if (lua_isnil(L, arg))
                dest = nullptr;
            else
                dest = luaL_checkstring(L, arg);
            return 0;
        }
    };
    
    template<typename T>
    struct frame_interface<
        std::optional<T>, 
        void>
    {
        using stored = typename make_stored<T>::type;
        
        static int push(lua_State *L, const std::optional<T>& value)
        {
            if (value)
                return frame_interface<stored>::push(L, *value);
            else
                return (lua_pushnil(L), 1);
        }
        
        static int assign(lua_State *L, int arg, std::optional<T>& dest)
        {
            // consider changing this to lua_isnone?
            // depends on how we want to treat nil for optionals
            if (lua_isnoneornil(L, arg)) {
                dest = std::nullopt;
                return 0;
            } else {
                dest = T();
                return frame_interface<stored>::assign(L, arg, *dest);
            }
        }
    };
    
    template<typename Tuple>
    struct frame_interface<
        Tuple,
        std::void_t<decltype(std::tuple_size<Tuple>::value)>>
    {
        using index = lua_Integer;
        
        inline constexpr auto size = std::tuple_size<Tuple>::value;
        inline constexpr auto indices = std::make_index_sequence<size>{};
        static_assert(detail::cmp_less_equal(size, std::numeric_limits<index>::max()));
        
        template<std::size_t... I>
        static int push_embed(
            lua_State *L, 
            const Tuple& value, 
            std::index_sequence<I...>)
        {
            auto push_element = [L] (auto& e) -> int
            {
                using element = std::remove_reference_t<decltype(e)>;
                using stored = typename make_stored<element>::type;
                return frame_interface<stored>::push(L, e);
            };
            
            using std::get; // ADL
            // this can overflow and invoke UB
            // not likely to, but there is no good compiler-agnostic way of 
            // doing this that looks sane and is performant
            // impossible to check at compile time unless we forbid variants
            int retcount = 0;
            (retcount += push_element(get<I>(value)), ...);
            return retcount;
        }
        
        template<std::size_t... I>
        static int push(
            lua_State *L,
            const Tuple& value,
            std::index_sequence<I...>)
        {
            // this implementation is less than ideal as ALL elements of the 
            // destination table are pushed before populating the table
            // the only way to fix this is an intrusive visitor on element push
            // such a solution is error-prone
            
            lua_createtable(L, int(size), 0); // best guess
            auto push_count = push_embed(L, value, std::index_sequence<I...>{});
            
            // value at pos -1 on stack is the last unpopulated element in table
            for (; push_count > 0; --push_count) {
                const auto table_index = -push_count - 1;
                const auto element_index = push_count;
                lua_seti(L, table_index, element_index);
            }
            
            // all pushed elements from push_embed are popped
            // leaving the return table as the sole result
            return 1;
        }
        
        template<std::size_t... I>
        static int assign(
            lua_State *L, 
            int arg,
            Tuple& dest, 
            std::index_sequence<I...>)
        {
            auto assign_element = [L, arg] (index i, auto& e) -> int 
            {
                using element = std::remove_reference_t<decltype(e)>;
                using stored = typename make_stored<element>::type;
                lua_geti(L, arg, i);
                const int result = frame_interface<stored>::assign(L, -1, e);
                lua_pop(L, 1);
                return result;
            };
            
            using std::get; // ADL
            int result = 0;
            // index(I) + 1 to convert to 1-based indices
            ((result = assign_element(index(I) + 1, get<I>(dest))) == 0 && ...);
            return result;
        }
        
        static int push(lua_State *L, const Tuple& value)
        {
            return push(L, value, indices);
        }
        
        static int push_embed(lus_State *L, const Tuple& value)
        {
            return push_embed(L, value, indices);
        }
        
        static int assign(lua_State *L, int arg, Tuple& dest)
        {
            return assign(L, arg, dest, indices);
        }
    };
    
    // embed tuple as return, no assignment
    template<typename Tuple>
    struct frame_interface<
        embedded_return<Tuple>,
        std::void_t<decltype(std::tuple_size<Tuple>::value)>>
    {
        using type = embedded_return<Tuple>;
        
        static int push(lua_State *L, int arg, const type& value)
        {
            using stored = typename make_stored<Tuple>::type;
            
            return frame_interface<stored>::push_embed(L, *value);
        }
        
        static int assign(lua_State *L, int arg, type& dest) = delete;
    };
    
    template<typename T>
    struct frame_interface<
        array_view<T>,
        void>
    {
        static int push(lua_State *L, int arg, const array_view<T>&) = delete;
        
        static int assign(lua_State *L, int arg, array_view<T>& dest)
        {
            dest = array_view<T>{L, arg};
            return 0;
        }
    };
    
} } // namespace Chimera::lua_wrapper

#endif // LUA_WRAPPERS_HPP