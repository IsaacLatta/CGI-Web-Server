#pragma once

#include "http/mw/Middleware.h"

namespace mw {
    class Parser: public Middleware {
    public:
        asio::awaitable<void> Process(Transaction& txn, Next next) override;
    };

}