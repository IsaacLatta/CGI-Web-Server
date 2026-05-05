#pragma once

#include "http/forward.h"
#include "http/mw/Context.h"

namespace mw {

class SessionBeginLogger: public Middleware<http::PreRouteContext> {
public:
    asio::awaitable<void> Process(http::PreRouteContext&, Next, Finish) override;
};

class SessionEndLogger: public Middleware<http::FinalContext> {
public:
    asio::awaitable<void> Process(http::FinalContext&, Next, Finish) override;
};

}
