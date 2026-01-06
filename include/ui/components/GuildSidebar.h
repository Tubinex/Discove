#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "state/Store.h"
#include "ui/AnimationManager.h"

class Fl_RGB_Image;
class GifAnimation;

class GuildSidebar : public Fl_Group {
  public:
    GuildSidebar(int x, int y, int w, int h, const char *label = nullptr);
    ~GuildSidebar();

    void draw() override;
    int handle(int event) override;

    /**
     * @brief Set the guild being displayed
     * @param guildId Guild ID
     * @param guildName Guild name
     */
    void setGuild(const std::string &guildId, const std::string &guildName);

    /**
     * @brief Set the guild banner URL
     * @param bannerUrl CDN URL for the guild banner
     */
    void setBannerUrl(const std::string &bannerUrl);

    /**
     * @brief Set the guild boost information
     * @param premiumTier Premium tier (0-3)
     * @param subscriptionCount Number of boosts
     */
    void setBoostInfo(int premiumTier, int subscriptionCount);

    /**
     * @brief Set the guild ID and load channels from Store
     * @param guildId Guild ID
     */
    void setGuildId(const std::string &guildId);

    /**
     * @brief Get the current guild ID
     * @return Guild ID
     */
    std::string getGuildId() const { return m_guildId; }

    /**
     * @brief Add a text channel to the sidebar
     * @param channelId Channel ID
     * @param channelName Channel name
     */
    void addTextChannel(const std::string &channelId, const std::string &channelName);

    /**
     * @brief Add a voice channel to the sidebar
     * @param channelId Channel ID
     * @param channelName Channel name
     */
    void addVoiceChannel(const std::string &channelId, const std::string &channelName);

    /**
     * @brief Clear all channels
     */
    void clearChannels();

    /**
     * @brief Set the selected channel
     * @param channelId Channel ID to select
     */
    void setSelectedChannel(const std::string &channelId);

    /**
     * @brief Set callback for channel selection
     * @param callback Callback function(channelId)
     */
    void setOnChannelSelected(std::function<void(const std::string &)> callback) { m_onChannelSelected = callback; }

    int getScrollOffset() const { return m_scrollOffset; }

    void setScrollOffset(int offset) {
        m_scrollOffset = offset;
        if (m_scrollOffset < 0)
            m_scrollOffset = 0;
        redraw();
    }

  private:
    struct ChannelItem {
        std::string id;
        std::string name;
        int type = 0;
        bool isVoice;
        bool hasUnread = false;
        int yPos = 0;
    };

    struct CategoryItem {
        std::string id;
        std::string name;
        std::vector<ChannelItem> channels;
        bool collapsed = false;
        int position = 0;
        int yPos = 0;
    };

    void loadChannelsFromStore();

    std::string m_guildId;
    std::string m_guildName;
    std::string m_rulesChannelId;
    std::string m_bannerUrl;
    std::string m_bannerHash;
    Fl_RGB_Image *m_bannerImage = nullptr;
    std::unique_ptr<GifAnimation> m_bannerGif;
    AnimationManager::AnimationId m_bannerAnimationId = 0;
    bool m_isAnimatedBanner = false;
    bool m_bannerHasPlayedOnce = false;
    bool m_bannerHovered = false;
    double m_frameTimeAccumulated = 0.0;
    int m_premiumTier = 0;
    int m_subscriptionCount = 0;
    std::vector<CategoryItem> m_categories;
    std::vector<ChannelItem> m_uncategorizedChannels;
    std::string m_selectedChannelId;

    static std::map<std::string, std::map<std::string, bool>> s_guildCategoryCollapsedState;
    int m_hoveredChannelIndex = -1;
    std::string m_hoveredCategoryId;
    int m_scrollOffset = 0;
    std::function<void(const std::string &)> m_onChannelSelected;
    Store::ListenerId m_storeListenerId = 0;
    std::shared_ptr<bool> m_isAlive;

    static constexpr int CHANNEL_HEIGHT = 36;
    static constexpr int CATEGORY_HEIGHT = 40;

    int getHeaderHeight() const;

    void drawServerHeader();
    void drawSimpleHeader();
    void drawBanner();
    void drawChannelCategory(const char *title, int &yPos, bool collapsed, int channelCount, bool hovered);
    void drawChannel(const ChannelItem &channel, int yPos, bool selected, bool hovered);
    int getChannelAt(int mx, int my, std::string &categoryId) const;
    int getCategoryAt(int mx, int my) const;
    int calculateContentHeight() const;
    void loadBannerImage();
    bool ensureBannerGifLoaded();
    bool updateBannerAnimation();
    void startBannerAnimation();
    void stopBannerAnimation();
};
