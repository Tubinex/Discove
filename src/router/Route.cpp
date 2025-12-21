#include "router/Route.h"
#include "router/Screen.h"
#include "utils/Logger.h"

Route::Route(std::string path, ScreenFactory factory) : m_path(std::move(path)), m_factory(std::move(factory)) {
    compilePath();
}

void Route::compilePath() {
    std::string regexPattern = "^";
    std::string::size_type pos = 0;
    std::string::size_type lastPos = 0;

    while ((pos = m_path.find(':', lastPos)) != std::string::npos) {
        regexPattern += m_path.substr(lastPos, pos - lastPos);
        std::string::size_type paramEnd = m_path.find('/', pos);
        if (paramEnd == std::string::npos) {
            paramEnd = m_path.length();
        }

        std::string paramName = m_path.substr(pos + 1, paramEnd - pos - 1);
        m_paramNames.push_back(paramName);

        regexPattern += "([a-zA-Z0-9_-]+)";
        lastPos = paramEnd;
    }

    regexPattern += m_path.substr(lastPos);
    regexPattern += "$";

    try {
        m_regex = std::regex(regexPattern);
    } catch (const std::regex_error &e) {
        Logger::error("Failed to compile route pattern: " + m_path + " - " + std::string(e.what()));
    }
}

RouteMatch Route::match(const std::string &path) const {
    RouteMatch result;
    std::smatch matches;

    if (std::regex_match(path, matches, m_regex)) {
        result.matched = true;

        for (size_t i = 1; i < matches.size() && i - 1 < m_paramNames.size(); ++i) {
            result.params[m_paramNames[i - 1]] = matches[i].str();
        }
    }

    return result;
}

std::unique_ptr<Screen> Route::createScreen(int x, int y, int w, int h) const {
    if (!m_factory) {
        Logger::error("No factory function for route: " + m_path);
        return nullptr;
    }
    return m_factory(x, y, w, h);
}
