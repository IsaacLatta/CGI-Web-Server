#pragma once

#include <unordered_map>
#include <any>
#include <functional>
#include <stdexcept>
#include <typeindex>
#include <format>

#include "concepts.h"
#include "core/concepts.h"

namespace core {

class Resolver {
public:
    Resolver() = default;

    template<typename FactoryAlias, typename FactoryImpl>
    requires ConstructibleFrom<FactoryAlias, FactoryImpl>
    void Set(FactoryImpl&& factory) {
        _factories[std::type_index(typeid(FactoryAlias))] = FactoryAlias(std::forward<FactoryImpl>(factory));
    }

    template<typename FactoryAlias, typename Fallback>
    requires CastableTo<Fallback, FactoryAlias>
    FactoryAlias GetOr(Fallback&& fallback) const {
        try {
            return TryResolve<FactoryAlias>();
        } catch (const std::runtime_error&) {
            return FactoryAlias(std::forward<Fallback>(fallback));
        }
    }

    template<typename FactoryAlias, typename Fallback, typename... Args>
    requires IsFactory<FactoryAlias, Resolver, Args&&...> && IsFactory<Fallback, Resolver, Args&&...>
    auto GetOrAndInvoke(Fallback&& fallback, Args&&... args) const {
        auto factory = GetOr<FactoryAlias>(std::forward<Fallback>(fallback));
        if constexpr (std::is_invocable_v<decltype(factory)&, const Resolver&, Args&& ...>) {
            return std::invoke(factory, *this, std::forward<Args>(args)...);
        } else {
            return std::invoke(factory, std::forward<Args>(args)...);
        }
    }

    template<typename FactoryAlias, typename... Args>
    requires IsFactory<FactoryAlias, Resolver, Args&&...>
    auto GetAndInvoke(Args&&... args) const {
        auto factory = TryResolve<FactoryAlias>();
        if constexpr (std::is_invocable_v<decltype(factory)&, const Resolver&, Args&& ...>) {
            return std::invoke(factory, *this, std::forward<Args>(args)...);
        } else {
            return std::invoke(factory, std::forward<Args>(args)...);
        }
    }

private:
    template<typename FactoryAlias>
    FactoryAlias TryResolve() const {
        try {
            const auto it = _factories.find(std::type_index(typeid(FactoryAlias)));
            if (it == _factories.end()) {
                throw std::runtime_error(
                    std::format("factory for type={} not found", typeid(FactoryAlias).name()));
            }
            return std::any_cast<FactoryAlias>(it->second);
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error(
                    std::format("factory for type={} stored under wrong type: {}",
                        typeid(FactoryAlias).name(), e.what()));
        }
    }

private:
    std::unordered_map<std::type_index, std::any> _factories;
};

}