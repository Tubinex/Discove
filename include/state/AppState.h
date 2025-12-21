#pragma once

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

struct AppState {
    int counter = 0;
    RouteState route;
};
