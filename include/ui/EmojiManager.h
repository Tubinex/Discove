#pragma once

#include <FL/Fl_Image.H>

#include <string>
#include <unordered_set>

namespace EmojiManager {
bool initialize(const std::string &rootDir);
void initializeFromDefaultLocations();
bool isAvailable();
bool tryMatch(const std::string &text, size_t pos, size_t &outLength, std::string &outEmoji);
bool hasEmoji(const std::string &emoji);
std::string makeCacheKey(const std::string &emoji, int size);
Fl_Image *loadEmoji(const std::string &emoji, int size);
void pruneCache(const std::unordered_set<std::string> &keepKeys);
void clearCache();
} // namespace EmojiManager
