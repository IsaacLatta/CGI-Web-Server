#pragma once

#include <vector>
#include <span>
#include <optional>

#include "logger/Entry.h"

#include "io/forward.h"

#include "http/forward.h"
#include "http/Request.h"
#include "http/Response.h"
#include "http/routing/Route.h"

namespace http {

    struct TransactionState {
        io::SocketPtr Socket;
        std::vector<char> Buffer{};
        logger::SessionEntry LogEntry{};

        std::optional<Request> Request;
        std::optional<Response> Response;

        std::optional<Route> Route;
        std::optional<Endpoint> Endpoint;

        Handler FinalHandler { nullptr };
    };

    class PreRouteContext {
    public:

        explicit PreRouteContext(TransactionState& state) : state_(state) {}

        io::Socket& GetSocket() {
            return *state_.Socket;
        }

        std::vector<char>& GetBuffer() {
            return state_.Buffer;
        }

        logger::SessionEntry& GetLogEntry() {
            return state_.LogEntry;
        }

        void SetRequest(Request request) {
            state_.Request = std::move(request);
        }

        void SetRoute(const Route& route) {
            state_.Route = route;
        }

        void SetEndpoint(const Endpoint& endpoint) {
            state_.Endpoint = endpoint;
        }

        bool IsRouted() const {
            return state_.Request.has_value()
                && state_.Route.has_value()
                && state_.Endpoint.has_value();
        }

        TransactionState& GetState() {
            return state_;
        }

    private:
        TransactionState& state_;
    };

    class PostRouteContext {
    public:

        explicit PostRouteContext(TransactionState& state): state_(state) {
            if (!state_.Request || !state_.Route || !state_.Endpoint) {
                throw Exception(Internal_Server_Error);
            }
        }

        io::Socket& GetSocket() {
            return *state_.Socket;
        }

        std::vector<char>& GetBuffer() {
            return state_.Buffer;
        }

        logger::SessionEntry& GetLogEntry() {
            return state_.LogEntry;
        }

        Request& GetRequest() {
            return *state_.Request;
        }

        const Route& GetRoute() {
            return *state_.Route;
        }

        const Endpoint& GetEndpoint() {
            return *state_.Endpoint;
        }

        Response& GetResponse() {
            if (!state_.Response) {
                state_.Response.emplace();
            }

            return *state_.Response;
        }

        TransactionState& GetState() {
            return state_;
        }

    private:
        TransactionState& state_;
    };

    class FinalContext {
    public:
        FinalContext(TransactionState& state, Response& response): state_(state), response_(response) {}

        io::Socket& GetSocket() {
            return *state_.Socket;
        }

        std::vector<char>& GetBuffer() {
            return state_.Buffer;
        }

        logger::SessionEntry& GetLogEntry() {
            return state_.LogEntry;
        }

        Response& GetResponse() {
            return response_;
        }

        Request* GetRequest() {
            return state_.Request ? &*state_.Request : nullptr;
        }

        bool IsRouted() const {
            return state_.Request.has_value()
                && state_.Route.has_value()
                && state_.Endpoint.has_value();
        }

        const Route& GetRoute() const {
            return state_.Route.value();
        }

        const Endpoint& GetEndpoint() const {
            return state_.Endpoint.value();
        }

    private:
        TransactionState& state_;
        Response& response_;
    };

}
