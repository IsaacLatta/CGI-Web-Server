#pragma once

#include <vector>
#include <span>

#include "io/forward.h"
#include "http/forward.h"

#include "http/Request.h"

namespace http {

    struct PreRouteContext {
        io::Socket& Socket;
        std::vector<char>& Buffer;

        // Fields that need to be constructed in this FaZe.
        std::optional<Request> ParsedRequest;
        const Route* MatchedRoute { nullptr };
        const Endpoint* MatchedEndpoint { nullptr };

        PreRouteContext(io::Socket& sock, std::vector<char>& buffer)
            : Socket(sock), Buffer(buffer) {}
    };

    struct PostRouteContext {
        PostRouteContext(
            const Route& route,
            const Endpoint& endpoint,
            io::Socket& sock,
            Request& request,
            std::vector<char>& buffer) :
        MatchedRoute(route),
        MatchedEndpoint(endpoint),
        Socket(sock),
        MatchedRequest(request),
        Buffer(buffer) {}

        Handler FinalHandler { nullptr };

        const Route& MatchedRoute;
        const Endpoint& MatchedEndpoint;
        io::Socket& Socket;
        Response WorkingResponse;
        Request& MatchedRequest;
        std::vector<char>& Buffer;
    };

    using PostEndpointContext = PostRouteContext;

}
