#pragma once

#include <string>

#include <asio.hpp>

#include "http/mw/Middleware.h"
#include "config/config.h"

namespace mw {

class Authenticator: public Middleware<http::PostRouteContext> {
public:
    Authenticator(const cfg::AccessControl& config)
        : config_(config) {}

    asio::awaitable<void> Process(http::PostRouteContext&, Next, Finish) override;

private:
    void Validate(http::PostRouteContext&, const http::Endpoint&) const;

private:
    const cfg::AccessControl& config_;
};

} // namespace mw
