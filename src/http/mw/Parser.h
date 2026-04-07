#pragma once

#include "Context.h"
#include "http/mw/Middleware.h"
#include "http/forward.h"
#include "http/mw/Context.h"

namespace mw {
class Parser: public Middleware<http::PreRouteContext> {
public:
    Parser(const http::Router& router) : router_(router) {}

    asio::awaitable<void> Process(http::PreRouteContext&, NextCallback, FinishCallback) override;

private:
    const http::Router& router_;
};

}