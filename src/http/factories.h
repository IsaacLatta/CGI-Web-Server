#pragma once

#include "http/Session.h"

namespace http {

    inline SessionPtr create_session(io::SocketPtr&& socket) {
        return std::make_shared<DefaultSession>(std::move(socket));
    }

}
