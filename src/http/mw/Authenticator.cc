#include "http/mw/Authenticator.h"

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include "core/time.h"

#include "http/Exception.h"

namespace {
    core::WallTimePoint default_expiry() {
        return core::WallClock::now() + core::Hrs { 1 };
    }
}

namespace mw {

    void Authenticator::validate(Transaction& txn, const http::EndpointMethod* route) const {
        auto request = txn.GetRequest();

        if(!route->is_protected) {
            return;
        }

        std::string cookie, token;
        if ((cookie = request.GetHeader("Cookie")).empty() || (token = http::extract_jwt_from_cookie(cookie)).empty()) {
            throw http::HTTPException(http::Unauthorized, "missing or invalid authentication token");
        }

        try {
            auto decoded_token = jwt::decode(token);
            auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{config->getSecret()}).with_issuer(config->getServerName());
            verifier.verify(decoded_token);

            if(decoded_token.has_expires_at() && std::chrono::system_clock::now() > decoded_token.get_expires_at()) {
                throw http::HTTPException(http::Unauthorized, "expired token");
            }

            auto role_claim = decoded_token.get_payload_claim("role");
            const cfg::Role* role;
            if(!((role = config->findRole(role_claim.as_string())) && role->includesRole(route->access_role))) {
                throw http::HTTPException(http::Unauthorized, "insufficient permissions");
            }
        } catch (const std::exception& error) {
            throw http::HTTPException(http::Unauthorized,
            std::format("[client {}] invalid token [error {}]", txn.GetSocket()->IpStr(), error.what()));
        }
    }

    asio::awaitable<void> Authenticator::Process(Transaction& txn, Next next) {
        auto& request = txn.GetRequest();
        const cfg::Config* config = cfg::Config::getInstance();

        validate(txn, request.route);
        co_await next();

        if(!request.route->is_authenticator) {
            co_return;
        }

        auto response = txn.GetResponse();
        if(!http::is_success_code(response.Status)) {
            throw http::HTTPException(response.Status, std::format("Failed to authorize client: {} [status={}]", txn.GetSocket()->IpStr(), static_cast<int>(response.Status)));
        }

        auto token_builder = jwt::create();
        std::string token = token_builder.set_issuer(config->getServerName()).set_subject("auth-token").set_expires_at(default_expiry())
                            .set_payload_claim("role",
                                jwt::claim(cfg::get_role_hash(request.endpoint->getAuthRole(request.method))))
                                    .sign(jwt::algorithm::hs256{config->getSecret()});

        response.AddHeader("Set-Cookie", std::format("jwt={}; HttpOnly; Secure; SameSite=Strict;", token));
        co_return;
    }


}