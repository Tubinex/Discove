#include "models/Reaction.h"

Reaction Reaction::fromJson(const nlohmann::json &j) {
    Reaction reaction;

    if (j.contains("count") && !j["count"].is_null()) {
        reaction.count = j["count"].get<int>();
    }
    if (j.contains("me") && !j["me"].is_null()) {
        reaction.me = j["me"].get<bool>();
    }

    if (j.contains("emoji") && j["emoji"].is_object()) {
        const auto &emoji = j["emoji"];
        if (emoji.contains("id") && !emoji["id"].is_null()) {
            reaction.emojiId = emoji["id"].get<std::string>();
        }
        if (emoji.contains("name") && !emoji["name"].is_null()) {
            reaction.emojiName = emoji["name"].get<std::string>();
        }
        if (emoji.contains("animated") && !emoji["animated"].is_null()) {
            reaction.emojiAnimated = emoji["animated"].get<bool>();
        }
    }

    return reaction;
}
