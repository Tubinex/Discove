#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "utils/Protobuf.h"

/**
 * @brief Represents a user's guild folder organization
 * Wraps the ParsedFolder from protobuf parsing
 */
class GuildFolder {
  public:
    /**
     * @brief Create GuildFolder from protobuf ParsedFolder
     * @param proto Parsed folder from user_settings_proto
     * @return GuildFolder instance
     */
    static GuildFolder fromProtobuf(const ProtobufUtils::ParsedFolder &proto);

    /**
     * @brief Check if folder is empty
     * @return true if folder has no guilds
     */
    bool isEmpty() const;

    int64_t id = -1;
    std::string name;
    std::vector<std::string> guildIds;
    std::optional<uint32_t> color;
};
