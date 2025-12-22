#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "models/Attachment.h"
#include "models/Embed.h"

/**
 * @brief Discord message types
 * @see https://discord.com/developers/docs/resources/message#message-object-message-types
 */
enum class MessageType {
    DEFAULT = 0,
    RECIPIENT_ADD = 1,
    RECIPIENT_REMOVE = 2,
    CALL = 3,
    CHANNEL_NAME_CHANGE = 4,
    CHANNEL_ICON_CHANGE = 5,
    CHANNEL_PINNED_MESSAGE = 6,
    USER_JOIN = 7,
    GUILD_BOOST = 8,
    GUILD_BOOST_TIER_1 = 9,
    GUILD_BOOST_TIER_2 = 10,
    GUILD_BOOST_TIER_3 = 11,
    CHANNEL_FOLLOW_ADD = 12,
    GUILD_DISCOVERY_DISQUALIFIED = 14,
    GUILD_DISCOVERY_REQUALIFIED = 15,
    GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16,
    GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17,
    THREAD_CREATED = 18,
    REPLY = 19,
    CHAT_INPUT_COMMAND = 20,
    THREAD_STARTER_MESSAGE = 21,
    GUILD_INVITE_REMINDER = 22,
    CONTEXT_MENU_COMMAND = 23,
    AUTO_MODERATION_ACTION = 24,
    ROLE_SUBSCRIPTION_PURCHASE = 25,
    INTERACTION_PREMIUM_UPSELL = 26,
    STAGE_START = 27,
    STAGE_END = 28,
    STAGE_SPEAKER = 29,
    STAGE_TOPIC = 31,
    GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 32,
    POLL = 46
};

/**
 * @brief Represents a Discord message
 * @see https://discord.com/developers/docs/resources/message
 */
class Message {
  public:
    /**
     * @brief Deserialize message from JSON
     * @param j JSON object from Discord API
     * @return Message instance
     */
    static Message fromJson(const nlohmann::json &j);

    /**
     * @brief Check if message is a reply to another message
     * @return true if message is a reply
     */
    bool isReply() const;

    /**
     * @brief Check if message is a system message
     * @return true if message is system-generated
     */
    bool isSystemMessage() const;

    /**
     * @brief Get the jump URL for this message
     * @return Discord jump URL (discord.com/channels/...)
     */
    std::string getJumpUrl() const;

    /**
     * @brief Check if message was edited
     * @return true if message has been edited
     */
    bool wasEdited() const;

    std::string id;                                                       ///< Message ID (snowflake)
    std::string channelId;                                                ///< Channel ID where message was sent
    std::string authorId;                                                 ///< Author user ID
    std::string content;                                                  ///< Message text content
    std::chrono::system_clock::time_point timestamp;                      ///< Message creation timestamp
    std::optional<std::chrono::system_clock::time_point> editedTimestamp; ///< Last edit timestamp
    bool tts = false;                                                     ///< Whether message is text-to-speech
    bool mentionEveryone = false;                                         ///< Whether @everyone was used
    std::vector<std::string> mentionIds;                                  ///< Mentioned user IDs
    std::vector<std::string> mentionRoleIds;                              ///< Mentioned role IDs
    std::vector<Attachment> attachments;                                  ///< File attachments
    std::vector<Embed> embeds;                                            ///< Rich embeds
    std::optional<std::string> nonce;                                     ///< Nonce for verification
    bool pinned = false;                                                  ///< Whether message is pinned
    std::optional<std::string> webhookId;                                 ///< Webhook ID if from webhook
    MessageType type;                                                     ///< Message type
    std::optional<std::string> applicationId;                             ///< Application ID (for interactions)
    std::optional<std::string> referencedMessageId;                       ///< Replied-to message ID
    std::optional<std::string> guildId;                                   ///< Guild ID (if in guild)
};
