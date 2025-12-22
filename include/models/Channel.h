#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Discord channel types
 * @see https://discord.com/developers/docs/resources/channel#channel-object-channel-types
 */
enum class ChannelType {
    GUILD_TEXT = 0,
    DM = 1,
    GUILD_VOICE = 2,
    GROUP_DM = 3,
    GUILD_CATEGORY = 4,
    GUILD_ANNOUNCEMENT = 5,
    ANNOUNCEMENT_THREAD = 10,
    PUBLIC_THREAD = 11,
    PRIVATE_THREAD = 12,
    GUILD_STAGE_VOICE = 13,
    GUILD_DIRECTORY = 14,
    GUILD_FORUM = 15,
    GUILD_MEDIA = 16
};

/**
 * @brief Represents a Discord channel
 * @see https://discord.com/developers/docs/resources/channel
 */
class Channel {
  public:
    /**
     * @brief Deserialize channel from JSON
     * @param j JSON object from Discord API
     * @return Channel instance
     */
    static Channel fromJson(const nlohmann::json &j);

    /**
     * @brief Check if this is a guild channel
     * @return true if channel is in a guild
     */
    bool isGuildChannel() const;

    /**
     * @brief Check if this is a DM channel
     * @return true if channel is a DM or group DM
     */
    bool isDM() const;

    /**
     * @brief Check if this is a thread
     * @return true if channel is a thread
     */
    bool isThread() const;

    /**
     * @brief Check if this is a voice channel
     * @return true if channel is voice/stage
     */
    bool isVoice() const;

    /**
     * @brief Get channel icon URL (for group DMs)
     * @param size Desired size (default 256)
     * @return CDN URL for icon, or empty if none
     */
    std::string getIconUrl(int size = 256) const;

    std::string id;                           ///< Channel ID (snowflake)
    ChannelType type;                         ///< Channel type
    std::optional<std::string> guildId;       ///< Guild ID (null for DMs)
    std::optional<int> position;              ///< Sorting position in guild
    std::optional<std::string> name;          ///< Channel name
    std::optional<std::string> topic;         ///< Channel topic/description
    bool nsfw = false;                        ///< Whether channel is NSFW
    std::optional<std::string> lastMessageId; ///< ID of last message
    std::optional<int> bitrate;               ///< Voice bitrate (voice channels)
    std::optional<int> userLimit;             ///< User limit (voice channels)
    std::optional<int> rateLimitPerUser;      ///< Slowmode seconds
    std::vector<std::string> recipientIds;    ///< DM recipient user IDs
    std::optional<std::string> icon;          ///< Channel icon hash (group DMs)
    std::optional<std::string> ownerId;       ///< DM/thread creator ID
    std::optional<std::string> parentId;      ///< Parent channel/category ID
    std::optional<int> messageCount;          ///< Thread message count
    std::optional<int> memberCount;           ///< Thread member count
};
