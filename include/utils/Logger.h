#pragma once

#include <mutex>
#include <string>

#ifdef ERROR
#undef ERROR
#endif

namespace Logger {
enum class Level { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, NONE = 4 };

void setLevel(Level level);
Level getLevel();

void debug(const std::string &message);
void info(const std::string &message);
void warn(const std::string &message);
void error(const std::string &message);

void log(Level level, const std::string &prefix, const std::string &message);
} // namespace Logger
