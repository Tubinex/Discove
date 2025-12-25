#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct RouteState {
    std::string currentPath = "";
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query;
    std::vector<std::string> history;
    int historyIndex = 0;
    bool isTransitioning = false;

    bool operator==(const RouteState &other) const {
        return currentPath == other.currentPath && params == other.params && query == other.query &&
               historyIndex == other.historyIndex && isTransitioning == other.isTransitioning;
    }

    bool operator!=(const RouteState &other) const { return !(*this == other); }
};

struct UserProfile {
    std::string id;
    std::string username;
    std::string discriminator = "0";
    std::string globalName;
    std::string avatarHash;
    std::string avatarUrl;
    std::string status = "online";

    bool operator==(const UserProfile &other) const {
        return id == other.id && username == other.username && discriminator == other.discriminator &&
               globalName == other.globalName && avatarHash == other.avatarHash && avatarUrl == other.avatarUrl &&
               status == other.status;
    }

    bool operator!=(const UserProfile &other) const { return !(*this == other); }
};

struct AppState {
    int counter = 0;
    RouteState route;
    std::optional<UserProfile> currentUser;
};
