#pragma once

#include <cstdint>
#include <string>

/**
 * Discord permission bit constants
 * @see https://discord.com/developers/docs/topics/permissions
 */
namespace Permissions {

constexpr uint64_t CREATE_INSTANT_INVITE = 1ULL << 0;
constexpr uint64_t KICK_MEMBERS = 1ULL << 1;
constexpr uint64_t BAN_MEMBERS = 1ULL << 2;
constexpr uint64_t ADMINISTRATOR = 1ULL << 3;
constexpr uint64_t MANAGE_CHANNELS = 1ULL << 4;
constexpr uint64_t MANAGE_GUILD = 1ULL << 5;
constexpr uint64_t ADD_REACTIONS = 1ULL << 6;
constexpr uint64_t VIEW_AUDIT_LOG = 1ULL << 7;
constexpr uint64_t PRIORITY_SPEAKER = 1ULL << 8;
constexpr uint64_t STREAM = 1ULL << 9;
constexpr uint64_t VIEW_CHANNEL = 1ULL << 10;
constexpr uint64_t SEND_MESSAGES = 1ULL << 11;
constexpr uint64_t SEND_TTS_MESSAGES = 1ULL << 12;
constexpr uint64_t MANAGE_MESSAGES = 1ULL << 13;
constexpr uint64_t EMBED_LINKS = 1ULL << 14;
constexpr uint64_t ATTACH_FILES = 1ULL << 15;
constexpr uint64_t READ_MESSAGE_HISTORY = 1ULL << 16;
constexpr uint64_t MENTION_EVERYONE = 1ULL << 17;
constexpr uint64_t USE_EXTERNAL_EMOJIS = 1ULL << 18;
constexpr uint64_t VIEW_GUILD_INSIGHTS = 1ULL << 19;
constexpr uint64_t CONNECT = 1ULL << 20;
constexpr uint64_t SPEAK = 1ULL << 21;
constexpr uint64_t MUTE_MEMBERS = 1ULL << 22;
constexpr uint64_t DEAFEN_MEMBERS = 1ULL << 23;
constexpr uint64_t MOVE_MEMBERS = 1ULL << 24;
constexpr uint64_t USE_VAD = 1ULL << 25;
constexpr uint64_t CHANGE_NICKNAME = 1ULL << 26;
constexpr uint64_t MANAGE_NICKNAMES = 1ULL << 27;
constexpr uint64_t MANAGE_ROLES = 1ULL << 28;
constexpr uint64_t MANAGE_WEBHOOKS = 1ULL << 29;
constexpr uint64_t MANAGE_GUILD_EXPRESSIONS = 1ULL << 30;
constexpr uint64_t USE_APPLICATION_COMMANDS = 1ULL << 31;
constexpr uint64_t REQUEST_TO_SPEAK = 1ULL << 32;
constexpr uint64_t MANAGE_EVENTS = 1ULL << 33;
constexpr uint64_t MANAGE_THREADS = 1ULL << 34;
constexpr uint64_t CREATE_PUBLIC_THREADS = 1ULL << 35;
constexpr uint64_t CREATE_PRIVATE_THREADS = 1ULL << 36;
constexpr uint64_t USE_EXTERNAL_STICKERS = 1ULL << 37;
constexpr uint64_t SEND_MESSAGES_IN_THREADS = 1ULL << 38;
constexpr uint64_t USE_EMBEDDED_ACTIVITIES = 1ULL << 39;
constexpr uint64_t MODERATE_MEMBERS = 1ULL << 40;
constexpr uint64_t VIEW_CREATOR_MONETIZATION_ANALYTICS = 1ULL << 41;
constexpr uint64_t USE_SOUNDBOARD = 1ULL << 42;
constexpr uint64_t CREATE_GUILD_EXPRESSIONS = 1ULL << 43;
constexpr uint64_t CREATE_EVENTS = 1ULL << 44;
constexpr uint64_t USE_EXTERNAL_SOUNDS = 1ULL << 45;
constexpr uint64_t SEND_VOICE_MESSAGES = 1ULL << 46;
constexpr uint64_t SEND_POLLS = 1ULL << 48;
constexpr uint64_t USE_EXTERNAL_APPS = 1ULL << 50;

} // namespace Permissions

namespace PermissionUtils {

/**
 * @brief Check if a permission bit is set
 * @param permissions Permission bitfield as uint64_t
 * @param permission The permission bit to check
 * @return true if permission is set
 */
bool hasPermission(uint64_t permissions, uint64_t permission);

/**
 * @brief Check if a permission bit is set in a string-encoded permission
 * @param permissionsStr Permission bitfield as string (from Discord API)
 * @param permission The permission bit to check
 * @return true if permission is set
 */
bool hasPermission(const std::string &permissionsStr, uint64_t permission);

/**
 * @brief Parse permissions string to uint64_t
 * @param permissionsStr Permission bitfield as string
 * @return Permission bits as uint64_t
 */
uint64_t parsePermissions(const std::string &permissionsStr);

/**
 * @brief Convert permissions uint64_t to string
 * @param permissions Permission bitfield as uint64_t
 * @return Permission bits as string
 */
std::string permissionsToString(uint64_t permissions);

} // namespace PermissionUtils
