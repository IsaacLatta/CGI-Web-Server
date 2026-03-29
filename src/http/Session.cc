#include "http/Session.h"
#include "config.h"
#include "Transaction.h"
#include "mw/Middleware.h"

namespace http {

asio::awaitable<void> DefaultSession::Start() {
    Transaction txn(sock_.get());
    const auto pipeline = cfg::Config::getInstance()->getPipeline();
    co_await pipeline->Run(txn);
}

}
