#ifndef ROUTER_H
#define ROUTER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include "http/forward.h"

namespace cfg {
    class Config;
    struct TokenBucketRateSetting;
}

namespace mw {
    class Middleware;
}

struct Transaction;

namespace http { 

    enum class arg_type {
        None, Any, Body_Any, Body_JSON, Body_URL, Query_String
    };
    arg_type arg_str_to_enum(const std::string& args_str) noexcept;

    using Handler = std::function<asio::awaitable<void>(Transaction*)>;
    using Limiter = std::function<asio::awaitable<void>(Transaction*)>;

    struct ErrorPage {
        Code status;
        Handler handler;
    };

    struct EndpointMethod {
        Method m;
        std::string access_role; // role required to access the endpoint
        std::string auth_role;
        bool is_protected{false};
        bool is_authenticator{false};
        std::string resource{""};
        bool has_script{false};
        http::arg_type args{arg_type::None};
        Handler handler;
        std::shared_ptr<mw::Middleware> rate_limiter;
    };

    class Endpoint {
        public:
        Endpoint();
        const EndpointMethod* getMethod(Method m) const;
        bool isMethodProtected(Method m) const;
        bool isMethodAuthenticator(Method m) const;
        bool hasScript(Method m) const;
        std::string getAuthRole(Method m) const;
        std::string getAccessRole(Method m) const;
        std::string getResource(Method m) const;
        arg_type getArgType(Method m) const;
        Handler getHandler(Method m) const;
        std::vector<Method> getMethods() const;
        void addMethod(EndpointMethod&& Method);
        void setEndpointURL(const std::string& url);

        private:
        std::unordered_map<Method, EndpointMethod> methods;
        std::string endpoint{""};
    };

    class Router 
    {
        friend class cfg::Config; // only the Configuration object can modify—at startup

        public:
        static Router* getInstance();
        const Endpoint* getEndpoint(const std::string& endpoint);
        const EndpointMethod* getEndpointMethod(const std::string& endpoint_url, http::Method m);
        const ErrorPage* getErrorPage(Code status) const;

        private:
        static Router INSTANCE;
        std::unordered_map<std::string, Endpoint> endpoints;
        std::unordered_map<Code, ErrorPage> error_pages;

        private:
        void updateEndpoint(const std::string& endpoint_url, EndpointMethod&& Method);
        void addErrorPage(ErrorPage&& error_page, std::string&& file);
        Router();
        Router(const Router&) = delete;
        Router& operator=(const Router&) = delete;
    };
};

#endif
