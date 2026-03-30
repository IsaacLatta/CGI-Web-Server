#pragma once

#include "Middleware.h"

namespace mw {

class ErrorHandler: public Middleware {
public:
    ErrorHandler(const http::Router& router) : router_(router) {}

    asio::awaitable<void> Process(http::Transaction& txn, Next next) override;

private:
    const http::Router& router_;
};

}
