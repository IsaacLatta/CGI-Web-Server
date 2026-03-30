#pragma once

#include <filesystem>

#include "Exception.h"
#include "http/Session.h"
#include "http/routing/Endpoint.h"

namespace http {

    inline EndpointMethod create_default_endpoint_method(const std::string& endpoint, http::Method m) {
        return EndpointMethod{
            m,
            cfg::VIEWER_ROLE_HASH,
            "",
            false,
            false,
            endpoint,
            false,
            ArgumentType::None,
            detail::assign_handler(m),
            {}};
    }

    inline Route create_default_endpoint(std::string path) {
        std::error_code ec;
        const std::filesystem::path file_path("public" + path);
        const bool file_exists = std::filesystem::exists(file_path, ec) &&
                                !std::filesystem::is_directory(file_path, ec) && !ec;

        if (!file_exists) {
            throw HTTPException(http::Not_Found, std::format("file for path={} does not exist", path));
        }

        Route ep;
        ep.AddMethod(create_default_endpoint_method("public" + path, Get));
        ep.AddMethod(create_default_endpoint_method("public" + path, Head));
        return ep;
    }


}
