#include "http/Session.h"
#include "mw/Middleware.h"

namespace http {

asio::awaitable<void> DefaultSession::Start() {
    co_await pipeline_.Run(transaction_);
}

}
