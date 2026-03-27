#pragma once

#include <concepts>

namespace core {

    template<typename T, typename... Args>
    concept ConstructibleFrom = std::constructible_from<std::remove_cvref_t<T>, std::decay_t<Args>...>;

    template<typename From, typename To>
    concept CastableTo = requires(From from) {
        static_cast<To>(from);
    };

    template<typename Callable, typename... Args>
    concept InvocableWith = std::invocable<std::remove_cvref_t<Callable>&, Args...>;

    template<typename Callable, typename Container, typename... Args>
    concept IsFactory = InvocableWith<Callable, Args...> || InvocableWith<Callable, Container, Args...>;

}
