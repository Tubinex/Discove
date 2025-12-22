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
     * @brief Check if folder is empty (has no guilds)
     * @return true if folder has no guilds
     */
    bool isEmpty() const;

    int64_t id = -1;                   ///< Folder ID (-1 for unnamed)
    std::string name;                  ///< Folder name (empty for unnamed)
    std::vector<std::string> guildIds; ///< Guild IDs in this folder
    std::optional<uint32_t> color;     ///< Folder color (RGB)
};
