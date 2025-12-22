#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ProtobufUtils {

struct ParsedFolder {
    std::vector<std::string> guildIds;
    std::string name;
    uint32_t color{0};
    bool hasColor{false};
    int64_t id{-1};
};

bool parseGuildFoldersProto(const std::string &base64, std::vector<ParsedFolder> &foldersOut,
                            std::vector<uint64_t> &positionsOut);

} // namespace ProtobufUtils
