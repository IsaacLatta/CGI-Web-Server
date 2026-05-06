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

        std::optional<Request> WorkingRequest;
        std::optional<Response> WorkingResponse;

        std::optional<Route> ResolvedRoute;
        std::optional<Endpoint> ResolvedEndpoint;
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
            state_.WorkingRequest = std::move(request);
        }

        void SetRoute(const Route& route) {
            state_.ResolvedRoute = route;
        }

        void SetEndpoint(const Endpoint& endpoint) {
            state_.ResolvedEndpoint = endpoint;
        }

        bool IsRouted() const {
            return state_.WorkingRequest.has_value()
                && state_.ResolvedRoute.has_value()
                && state_.ResolvedEndpoint.has_value();
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
            if (!state_.WorkingRequest || !state_.ResolvedRoute || !state_.ResolvedEndpoint) {
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
            return *state_.WorkingRequest;
        }

        const Route& GetRoute() {
            return *state_.ResolvedRoute;
        }

        const Endpoint& GetEndpoint() {
            return *state_.ResolvedEndpoint;
        }

        Response& GetResponse() {
            if (!state_.WorkingResponse) {
                state_.WorkingResponse.emplace();
            }

            return *state_.WorkingResponse;
        }

        void SetResponse(Response response) {
            state_.WorkingResponse = std::move(response);
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
            return state_.WorkingRequest ? &*state_.WorkingRequest : nullptr;
        }

        [[nodiscard]] bool IsRouted() const {
            return state_.WorkingRequest.has_value()
                && state_.ResolvedRoute.has_value()
                && state_.ResolvedEndpoint.has_value();
        }

        const Route& GetRoute() const {
            return state_.ResolvedRoute.value();
        }

        const Endpoint& GetEndpoint() const {
            return state_.ResolvedEndpoint.value();
        }

    private:
        TransactionState& state_;
        Response& response_;
    };

}
