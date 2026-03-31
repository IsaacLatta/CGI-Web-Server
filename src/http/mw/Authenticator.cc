#include "http/mw/Authenticator.h"

#include <cassert>

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include "core/time.h"

#include "http/Exception.h"
#include "http/Transaction.h"

namespace mw {

    void Authenticator::Validate(http::Transaction& txn, const http::Endpoint& endpoint) const {
        const auto& request = txn.GetRequest();

        if(!endpoint.IsProtected) {
            return;
        }

        const auto token = http::extract_jwt_from_cookie(request.GetHeader("Cookie"));
        if (!token) {
            throw http::Exception(http::Unauthorized, "missing or invalid authentication token");
        }

        try {
            const auto decoded_token = jwt::decode(*token);
            const auto verifier = jwt::verify().allow_algorithm(
                jwt::algorithm::hs256{config_.Secret}).with_issuer(config_.Issuer);

            verifier.verify(decoded_token);

            if(decoded_token.has_expires_at() && core::WallClock::now() > decoded_token.get_expires_at()) {
                throw http::Exception(http::Unauthorized, "expired token");
            }

            auto role_claim = decoded_token.get_payload_claim("role");
            if (!config_.IncludesRole(endpoint.AccessRole, role_claim.as_string())) {
                throw http::Exception(http::Unauthorized, "insufficient permissions");
            }

        } catch (const std::exception& e) {
            throw http::Exception(http::Unauthorized, std::format("authorization failed: {}", e.what()));
        }
    }

    asio::awaitable<void> Authenticator::Process(http::Transaction& txn, Next next) {
        if (!txn.ResolvedEndpoint) {
            throw http::Exception(http::Internal_Server_Error, "mw::Authenticator, routing failure!");
        }

        Validate(txn, *txn.ResolvedEndpoint);
        co_await next();
        co_return;
    }

}