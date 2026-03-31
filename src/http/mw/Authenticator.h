#pragma once

#include <string>

#include <asio.hpp>

#include "http/mw/Middleware.h"
#include "config/config.h"

namespace mw {

class Authenticator: public Middleware {
public:
    Authenticator(const cfg::AccessControl& config)
        : config_(config) {}

    asio::awaitable<void> Process(http::Transaction&, Next) override;

private:
    void Validate(http::Transaction&, const http::Endpoint&) const;

private:
    const cfg::AccessControl& config_;
};

} // namespace mw
