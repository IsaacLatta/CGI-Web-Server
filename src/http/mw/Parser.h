#pragma once

#include "http/mw/Middleware.h"
#include "http/forward.h"

namespace mw {
class Parser: public Middleware {
public:
    Parser(const http::Router& router) : router_(router) {}

    asio::awaitable<void> Process(http::Transaction& txn, Next next) override;

private:
    const http::Router& router_;
};

}