#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>

#include "http/forward.h"
#include "http/mw/Context.h"

namespace http {

class Session {
public:
    virtual ~Session() = default;
    virtual asio::awaitable<void> Start() = 0;
};

class DefaultSession : public Session {
public:
    explicit DefaultSession(
        const Router& router,
        const mw::Pipeline<PreRouteContext>& preroute_pipeline,
        const mw::Pipeline<FinalContext>& final_pipeline,
        io::SocketPtr&& sock) : router_(router), final_pipeline_(final_pipeline), preroute_pipeline_(preroute_pipeline) {

        state_.Socket = std::move(sock);
        state_.Buffer.reserve(io::BUFFER_SIZE);
        state_.Buffer.resize(io::BUFFER_SIZE);
    }

    asio::awaitable<void> Start() override;

private:
    asio::awaitable<void> DoPeRoute();
    asio::awaitable<void> DoPostRoute();
    asio::awaitable<void> OnFinish(Response, std::optional<ResponseHandler> = std::nullopt);

private:
    TransactionState state_{};
    const Router& router_;
    const mw::Pipeline<FinalContext>& final_pipeline_;
    const mw::Pipeline<PreRouteContext>& preroute_pipeline_;
    bool done_ { false };
};

} // namespace http

