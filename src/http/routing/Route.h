#pragma once

#include "core/concepts.h"

#include "http/forward.h"
#include "http/mw/Middleware.h"
#include "http/Exception.h"

namespace http {

struct ErrorPage {
    Code Status;
    ResponseHandler Handler;
};

struct Endpoint {
    Method HttpMethod;

    std::string AccessRole;

    std::string AuthRole;

    bool IsProtected { false };

    bool IsAuthenticator{ false };

    std::string ResourceName;

    bool HasScript { false };

    ArgumentType ArgType { ArgumentType::None };

    mw::Pipeline<PostRouteContext> Pipeline;

    EndpointHandler Handler;
};

class Route {
public:
    Route() = default;

    explicit Route(std::unordered_map<Method, Endpoint> methods) : endpoints_(std::move(methods)) {}

    const Endpoint& GetEndpoint(Method) const;

    bool IsMethodProtected(Method) const;

    bool IsMethodAuthenticator(Method) const;

    bool HasScript(Method) const;

    std::string GetAuthRole(Method) const;

    std::string GetAccessRole(Method) const;

    std::string GetResource(Method) const;

    ArgumentType GetArgType(Method) const;

    EndpointHandler GetHandler(Method) const;

    std::vector<Method> GetAvailableMethods() const;

    const mw::Pipeline<PostRouteContext>& Pipeline() const;

private:
    template<typename Callable>
    requires core::InvocableWith<Callable, const Endpoint&>
    auto TryVisitProperty(Method method, Callable&& callable) const {
        const auto it = endpoints_.find(method);
        if (it == endpoints_.end()) {
            throw Exception(Method_Not_Allowed);
        }
        return std::invoke(std::forward<Callable>(callable), it->second);
    }

private:
    std::unordered_map<Method, Endpoint> endpoints_;
    mw::Pipeline<PostRouteContext> pipeline_;
};
}
