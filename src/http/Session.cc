#include "http/Session.h"
#include "config.h"
#include "Transaction.h"
#include "Middleware.h"

namespace http {

asio::awaitable<void> DefaultSession::Start() {
    Transaction txn(sock_.get());
    const auto pipeline = cfg::Config::getInstance()->getPipeline();
    co_await pipeline->run(&txn);
}

}
