#include "http/mw/ErrorHandler.h"
#include "http/Exception.h"
#include "http/Transaction.h"

namespace mw {

asio::awaitable<void> ErrorHandler::Process(http::Transaction& txn, Next next) {
    http::Handler error_handler;
    try {

        co_await next();
        if (txn.Endpoint) {
            co_await txn.Endpoint->Handle(txn);
        }

        co_return;
    } catch (const http::Exception& http_error) {
        DEBUG("MW Error Handler", "status=%d %s", static_cast<int>(http_error.GetResponse().Status), http_error.what());
        txn.SetResponse(http_error.GetResponse());
        error_handler = router_.GetErrorPage(txn.GetResponse().Status).Handler;
    } catch (const std::exception& error) {
        DEBUG("MW Error Handler", "status=500, std exception: %s", error.what());
        txn.SetResponse(http::Response(http::Internal_Server_Error));
        error_handler = router_.GetErrorPage(txn.GetResponse().Status).Handler;
    }

    if(error_handler) {
        TRACE("MW Error Handler", "Serving error page for status=%d", static_cast<int>(txn.GetResponse().Status));
        co_await error_handler(&txn);
    }

    co_return;
}

}