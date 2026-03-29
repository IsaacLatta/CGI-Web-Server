#pragma once

#include "Middleware.h"

namespace mw {

class Logger: public Middleware {
public:
    asio::awaitable<void> Process(Transaction& txn, Next next) override;
};

}
