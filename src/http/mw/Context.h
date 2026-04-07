#pragma once

#include <vector>
#include <span>

#include "io/forward.h"
#include "http/forward.h"

#include "http/Request.h"

namespace http {

    struct PreRouteContext {
        io::Socket& Socket;
        std::vector<char> Buffer;

        // Fields that need to be constructed in this FaZe.
        std::optional<Request> ParsedRequest;
        const Route* MatchedRoute { nullptr };
        const Endpoint* MatchedEndpoint { nullptr };

        PreRouteContext(io::Socket& sock) : Socket(sock) {
            Buffer.reserve(io::BUFFER_SIZE);
            Buffer.resize(io::BUFFER_SIZE);
        }
    };

    struct PostRouteContext {
        PostRouteContext(
            const Route&,
            const Endpoint&,
            io::Socket&,
            Request&,
            std::vector<char>&&);

        const Route& MatchedRoute;
        const Endpoint& MatchedEndpoint;
        io::Socket& Socket;
        Response WorkingResponse;
        Request& MatchedRequest;
        std::vector<char> Buffer;
    };

}
