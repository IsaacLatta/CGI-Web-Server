#pragma once

#include <asio.hpp>
#include <string>
#include <format>
#include <functional>
#include <algorithm>

namespace cfg {

constexpr int DEFAULT_WINDOW_SECONDS = 60;
constexpr int DEFAULT_MAX_REQUESTS = 3000;
constexpr int DEFAULT_TOKEN_CAPACITY = 60;
constexpr int DEFAULT_REFILL_RATE = 2; /* in tokens/s, i.e. 1 token/s */

/* Returns the sockets ip address */
inline std::string default_make_key(http::Transaction& txn) {
    return txn.GetSocket().IpStr();
}

struct Role {
    std::string Title;
    std::vector<std::string> Includes;

    [[nodiscard]] bool IncludesRole(const std::string& role) const {
        return (Title == role) || (std::ranges::find(Includes, role) != Includes.end());
    }
};

struct AccessControl {
    std::string Secret;
    std::string Issuer;
    std::vector<Role> Roles;

    [[nodiscard]] std::optional<Role> GetRole(const std::string& title) const {
        for (const auto& role : Roles) {
            if (role.Title == title) {
                return role;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool IncludesRole(const std::string& target_role, const std::string& queried_role) const {
        const auto role = GetRole(target_role);
        if (!role) {
            return false;
        }
        return role->IncludesRole(queried_role);
    }

};

struct RateSetting {
    enum class KeyType { IP, Header };
    KeyType key_type{KeyType::IP};
    std::function<std::string(http::Transaction&)> make_key{default_make_key};
};

struct TokenBucketSetting: public RateSetting {
    int capacity{DEFAULT_TOKEN_CAPACITY};
    int refill_rate{DEFAULT_REFILL_RATE}; // tokens per second
};

struct FixedWindowSetting: public RateSetting {
    int window_seconds{DEFAULT_WINDOW_SECONDS};
    int max_requests{DEFAULT_MAX_REQUESTS};
};

inline std::string get_role_hash(std::string role_title) {
    return role_title;
}

const std::string VIEWER_ROLE_HASH = get_role_hash("viewer");
const std::string USER_ROLE_HASH = get_role_hash("user");
const std::string ADMIN_ROLE_HASH = get_role_hash("admin");
const Role VIEWER = {VIEWER_ROLE_HASH, {}};
const Role USER = {USER_ROLE_HASH, {VIEWER_ROLE_HASH}};
const Role ADMIN = {ADMIN_ROLE_HASH, {USER_ROLE_HASH, VIEWER_ROLE_HASH}};

const std::string NO_HOST_NAME  = "server";
const size_t DEFAULT_JWT_SECRET_SIZE = 64;

struct SSLConfig {
    bool Active { false };
    std::string KeyPath;
    std::string CertificatePath;
};

struct Config {
    uint16_t Threads { 0u };
    SSLConfig Ssl;
    AccessControl AccessCtrl;
    std::string ServerName;
    std::string ContentPath;
};

};
