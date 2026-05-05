#include "http/mw/Authenticator.h"

#include <cassert>
#include <format>

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include "logger/macros.h"

#include "core/time.h"

#include "http/Exception.h"
#include "http/mw/Context.h"
#include "http/routing/Route.h"

namespace mw {

    asio::awaitable<void> Authenticator::Process(http::PostRouteContext& ctx, Next next, Finish finish) {
        const auto& request = ctx.GetRequest();

        if(!ctx.GetEndpoint().IsProtected) {
            co_return;
        }

        const auto token = http::extract_jwt_from_cookie(request.GetHeader("Cookie"));
        if (!token) {
            throw http::Exception(http::Unauthorized, "missing or invalid authentication token");
        }

        try {
            const auto decoded_token = jwt::decode(*token);
            const auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{config_.Secret}).with_issuer(config_.Issuer);

            verifier.verify(decoded_token);

            if(decoded_token.has_expires_at() && core::WallClock::now() > decoded_token.get_expires_at()) {
                throw http::Exception(http::Unauthorized, "expired token");
            }

            auto role_claim = decoded_token.get_payload_claim("role");
            if (!config_.IncludesRole(ctx.GetEndpoint().AccessRole, role_claim.as_string())) {
                throw http::Exception(http::Unauthorized, "insufficient permissions");
            }

            co_return co_await next(ctx);
        } catch (const std::exception& e) {
            ERROR("MW Authenticator", "client failed to authenticate %s", e.what());
        }
        co_return co_await finish(ctx, http::Response{http::Unauthorized}, std::nullopt);
    }

}