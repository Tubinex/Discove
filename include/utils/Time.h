#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace TimeUtils {

/**
 * @brief Parse an ISO 8601 timestamp string to a time_point
 * @param iso ISO 8601 timestamp string (e.g., "2024-12-22T15:30:45.123+00:00")
 * @return time_point if parsing succeeds, std::nullopt otherwise
 */
std::optional<std::chrono::system_clock::time_point> parseISO8601(const std::string &iso);

/**
 * @brief Format a time_point to an ISO 8601 timestamp string
 * @param tp The time_point to format
 * @return ISO 8601 formatted string
 */
std::string formatISO8601(const std::chrono::system_clock::time_point &tp);

/**
 * @brief Extract timestamp from a Discord snowflake ID
 * Discord snowflakes encode creation timestamp in first 42 bits
 * @param snowflake Discord snowflake ID as string
 * @return time_point representing when the snowflake was created
 */
std::chrono::system_clock::time_point snowflakeToTimestamp(const std::string &snowflake);

/**
 * @brief Extract timestamp from a Discord snowflake ID
 * @param snowflake Discord snowflake ID as uint64_t
 * @return time_point representing when the snowflake was created
 */
std::chrono::system_clock::time_point snowflakeToTimestamp(uint64_t snowflake);

} // namespace TimeUtils
