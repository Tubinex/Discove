#include "models/StickerItem.h"

StickerItem StickerItem::fromJson(const nlohmann::json &j) {
    StickerItem item;

    if (j.contains("id") && !j["id"].is_null()) {
        item.id = j["id"].get<std::string>();
    }
    if (j.contains("name") && !j["name"].is_null()) {
        item.name = j["name"].get<std::string>();
    }
    if (j.contains("format_type") && !j["format_type"].is_null()) {
        item.formatType = j["format_type"].get<int>();
    }

    return item;
}

