#pragma once

#include "http/mw/Middleware.h"

namespace mw {

class Authenticator: public Middleware {
public:
    asio::awaitable<void> Process(Transaction& txn, Next next) override;

private:
    void validate(Transaction& txn, const http::EndpointMethod* route);

private:


};

} // namespace mw
