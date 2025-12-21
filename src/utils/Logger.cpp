#include "utils/Logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Logger {

static std::mutex g_mutex;
static Level g_level = Level::INFO;

static const char *getName(Level level) {
    switch (level) {
    case Level::DEBUG:
        return "DEBUG";
    case Level::INFO:
        return "INFO";
    case Level::WARN:
        return "WARN";
    case Level::ERROR:
        return "ERROR";
    case Level::NONE:
        return "NONE";
    default:
        return "INFO";
    }
}

static const char *getColor(Level level) {
    switch (level) {
    case Level::DEBUG:
        return "\033[36m";
    case Level::INFO:
        return "\033[37m";
    case Level::WARN:
        return "\033[33m";
    case Level::ERROR:
        return "\033[31m";
    default:
        return "\033[0m";
    }
}

static constexpr const char *DIM = "\033[90m";
static constexpr const char *RESET = "\033[0m";

static std::string timestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm.tm_hour << ":" << std::setw(2) << tm.tm_min << ":" << std::setw(2)
        << tm.tm_sec << "." << std::setw(3) << ms.count();
    return oss.str();
}

static bool shouldLog(Level level) {
    if (g_level == Level::NONE)
        return false;
    return static_cast<int>(level) >= static_cast<int>(g_level);
}

void setLevel(Level level) {
    std::scoped_lock lock(g_mutex);
    g_level = level;
}

Level getLevel() {
    std::scoped_lock lock(g_mutex);
    return g_level;
}

void debug(const std::string &message) { log(Level::DEBUG, "", message); }
void info(const std::string &message) { log(Level::INFO, "", message); }
void warn(const std::string &message) { log(Level::WARN, "", message); }
void error(const std::string &message) { log(Level::ERROR, "", message); }

void log(Level level, const std::string &prefix, const std::string &message) {
    {
        std::scoped_lock lock(g_mutex);
        if (!shouldLog(level))
            return;
    }

    const std::string ts = timestamp();
    std::string tag = getName(level);
    std::ostringstream out;
    out << DIM << ts << RESET << " ";

    if (!prefix.empty()) {
        out << DIM << "[" << prefix << "] " << RESET;
    }

    out << getColor(level) << "[" << tag << "]" << " " << message << RESET;

    std::scoped_lock lock(g_mutex);
    std::cout << out.str() << "\n";
    std::cout.flush();
}

} // namespace Logger
