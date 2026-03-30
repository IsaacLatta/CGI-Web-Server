#include "http/routing/Route.h"
#include "http/Exception.h"
#include "http.h"

namespace http {

const Endpoint& Route::GetEndpoint(Method m) const {
    const auto it = endpoints_.find(m);
    if (it == endpoints_.end()) {
        throw Exception(Method_Not_Allowed);
    }
    return it->second;
}

bool Route::IsMethodProtected(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.IsProtected;
    });
}

bool Route::HasScript(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.HasScript;
    });
}

ArgumentType Route::GetArgType(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.ArgType;
    });
}

std::string Route::GetAuthRole(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
       return ep.AuthRole;
    });
}

bool Route::IsMethodAuthenticator(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.IsAuthenticator;
    });
}

std::string Route::GetResource(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.ResourceName;
    });
}

std::string Route::GetAccessRole(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
            return ep.AccessRole;
    });
}

Handler Route::GetHandler(Method m) const {
    return TryVisitProperty(m, [](const Endpoint& ep) {
        return ep.Handler;
    });
}

std::vector<Method> Route::GetAvailableMethods() const {
    std::vector<Method> results;
    for(const auto& ep : endpoints_) {
        results.push_back(ep.first);
    }
    return results;
}

}