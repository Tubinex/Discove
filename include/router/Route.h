#pragma once

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

class Screen;

struct RouteMatch {
    bool matched = false;
    std::unordered_map<std::string, std::string> params;

    explicit operator bool() const { return matched; }
};

class Route {
  public:
    using ScreenFactory = std::function<std::unique_ptr<Screen>(int x, int y, int w, int h)>;

    Route(std::string path, ScreenFactory factory);

    RouteMatch match(const std::string &path) const;
    std::unique_ptr<Screen> createScreen(int x, int y, int w, int h) const;

    const std::string &path() const { return m_path; }

  private:
    void compilePath();

    std::string m_path;
    std::regex m_regex;
    std::vector<std::string> m_paramNames;
    ScreenFactory m_factory;
};
