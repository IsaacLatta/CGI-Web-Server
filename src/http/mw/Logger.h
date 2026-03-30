#pragma once

#include "Middleware.h"

namespace mw {

class Logger: public Middleware {
public:
    asio::awaitable<void> Process(http::Transaction& txn, Next next) override;
};

}
