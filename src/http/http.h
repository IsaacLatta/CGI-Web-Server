#pragma once

#include <asio.hpp>

#include "io/forward.h"
#include "http/forward.h"

namespace http {

    asio::awaitable<void> send_response(Response&, io::Socket&);

    asio::awaitable<io::Result> send_response_to(Response&, io::Socket&);

}