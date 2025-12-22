#include "utils/Time.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace TimeUtils {

std::optional<std::chrono::system_clock::time_point> parseISO8601(const std::string &iso) {
    if (iso.empty()) {
        return std::nullopt;
    }
    std::tm tm = {};
    std::istringstream ss(iso);

    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::nullopt;
    }

    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    if (ss.peek() == '.') {
        ss.ignore();
        std::string fractionalStr;
        while (std::isdigit(ss.peek())) {
            fractionalStr += static_cast<char>(ss.get());
        }

        if (!fractionalStr.empty()) {
            if (fractionalStr.length() < 6) {
                fractionalStr.append(6 - fractionalStr.length(), '0');
            } else if (fractionalStr.length() > 6) {
                fractionalStr = fractionalStr.substr(0, 6);
            }

            int64_t microseconds = std::stoll(fractionalStr);
            tp += std::chrono::microseconds(microseconds);
        }
    }

    return tp;
}

std::string formatISO8601(const std::chrono::system_clock::time_point &tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = {};

#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");

    auto since_epoch = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds);

    if (microseconds.count() > 0) {
        ss << '.' << std::setfill('0') << std::setw(6) << microseconds.count();
    }

    ss << "+00:00";

    return ss.str();
}

std::chrono::system_clock::time_point snowflakeToTimestamp(const std::string &snowflake) {
    try {
        uint64_t id = std::stoull(snowflake);
        return snowflakeToTimestamp(id);
    } catch (...) {
        return std::chrono::system_clock::from_time_t(0);
    }
}

std::chrono::system_clock::time_point snowflakeToTimestamp(uint64_t snowflake) {
    constexpr int64_t DISCORD_EPOCH = 1420070400000LL;

    uint64_t timestamp_ms = (snowflake >> 22) + DISCORD_EPOCH;
    auto duration = std::chrono::milliseconds(timestamp_ms);
    return std::chrono::system_clock::time_point(duration);
}

} // namespace TimeUtils
