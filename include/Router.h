#ifndef ROUTER_H
#define ROUTER_H

#include <unordered_map>
#include <string>
#include <functional>
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

// #include "MethodHandler.h"
namespace http {
    class Request;
    enum class method : int;
}

namespace cfg {
    class Config;
}

class Transaction;

namespace http {

    using Handler = std::function<asio::awaitable<void>(Transaction*)>;

    struct EndpointMethod {
        method m;
        std::string access_role; // role required to access the endpoint
        std::string auth_role;
        bool is_protected{false};
        bool is_authenticator{false};
        std::string script{""};
        Handler handler;
    };

    class Endpoint {
        public:
        Endpoint();
        bool isMethodProtected(method m) const;
        bool isMethodAuthenticator(method m) const;
        // method getAuthMethod() const; // need to think about the possibility here of multiple auth methods on the same endpoint
        std::string getAccessRole(method m) const;
        std::string getScript(method m) const;
        Handler getHandler(method m) const;
        void addMethod(EndpointMethod&& method);

        private:
        std::unordered_map<method, EndpointMethod> methods;
        std::string endpoint{""};
    };

    class Router 
    {
        friend class cfg::Config; // only the Configuration object can modify—at startup

        public:
        static const Router* getInstance();
        const Endpoint* getEndpoint(const std::string& endpoint) const;

        private:
        static Router INSTANCE;
        std::unordered_map<std::string, Endpoint> endpoints;

        private:
        void updateEndpoint(const std::string& endpoint_url, EndpointMethod&& method);
        Router();
        Router(const Router&) = delete;
        Router& operator=(const Router&) = delete;
    };
};

// namespace std {
//     template <>
//     struct hash<http::method> {
//         std::size_t operator()(const http::method& m) const noexcept {
//             return std::hash<int>()(static_cast<int>(m));
//         }
//     };
// }

#endif
