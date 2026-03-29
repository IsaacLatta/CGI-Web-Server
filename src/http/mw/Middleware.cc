#include "Middleware.h"
#include "http/Exception.h"

namespace mw {

asio::awaitable<void> Pipeline::RunOne(Transaction& txn, size_t index) const {
    if (index >= components_.size()) {
        co_return;
    }

    Middleware& mw = *components_[index];
    Next next = [this, &txn, index]() mutable -> asio::awaitable<void> {
        co_await RunOne(txn, index + 1);
    };

    co_await mw.Process(txn, std::move(next));
}

asio::awaitable<void> Pipeline::Run(Transaction& txn) const {
    co_return co_await RunOne(txn, 0u);
}


}
 