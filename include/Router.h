#ifndef ROUTER_H
#define ROUTER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

namespace http {
    class Request;
    enum class method : int;
    enum class code : int;
}

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
        http::code status;
        Handler handler;
    };

    struct EndpointMethod {
        method m;
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
        const EndpointMethod* getMethod(method m) const;
        bool isMethodProtected(method m) const;
        bool isMethodAuthenticator(method m) const;
        bool hasScript(method m) const;
        std::string getAuthRole(method m) const;
        std::string getAccessRole(method m) const;
        std::string getResource(method m) const;
        arg_type getArgType(method m) const;
        Handler getHandler(method m) const;
        std::vector<method> getMethods() const;
        void addMethod(EndpointMethod&& method);
        void setEndpointURL(const std::string& url);

        private:
        std::unordered_map<method, EndpointMethod> methods;
        std::string endpoint{""};
    };

    class Router 
    {
        friend class cfg::Config; // only the Configuration object can modify—at startup

        public:
        static Router* getInstance();
        const Endpoint* getEndpoint(const std::string& endpoint);
        const EndpointMethod* getEndpointMethod(const std::string& endpoint_url, http::method m);
        const ErrorPage* getErrorPage(http::code status) const;

        private:
        static Router INSTANCE;
        std::unordered_map<std::string, Endpoint> endpoints;
        std::unordered_map<http::code, ErrorPage> error_pages;

        private:
        void updateEndpoint(const std::string& endpoint_url, EndpointMethod&& method);
        void addErrorPage(ErrorPage&& error_page, std::string&& file);
        Router();
        Router(const Router&) = delete;
        Router& operator=(const Router&) = delete;
    };
};

#endif
