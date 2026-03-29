#include "http/mw/ErrorHandler.h"
#include "http/Exception.h"
#include "logger_macros.h"

namespace mw {

asio::awaitable<void> ErrorHandler::Process(Transaction& txn, Next next) {
    http::Handler error_handler = nullptr;
    try {
        co_await next();
        const auto& request = txn.GetRequest();
        if(auto finisher = request.endpoint->getHandler(request.method)) {
            co_await finisher(&txn);
        }
    }
    catch (const http::HTTPException& http_error) {
        DEBUG("MW Error Handler", "status=%d %s", static_cast<int>(http_error.getResponse()->Status), http_error.what());
        txn.response = std::move(*http_error.getResponse());
        error_handler = http::Router::getInstance()->getErrorPage(txn.response.Status)->handler;
    }
    catch (const std::exception& error) {
        DEBUG("MW Error Handler", "status=500, std exception: %s", error.what());
        txn.response = std::move(http::Response(http::Internal_Server_Error));
        error_handler = http::Router::getInstance()->getErrorPage(txn.response.Status)->handler;
    }

    if(error_handler) {
        TRACE("MW Error Handler", "Serving error page for status=%d", static_cast<int>(txn.response.Status));
        co_await error_handler(&txn);
    }
    co_return;
}

}