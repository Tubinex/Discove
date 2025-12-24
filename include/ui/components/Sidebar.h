#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <string>
#include <vector>

class Sidebar : public Fl_Group {
  public:
    Sidebar(int x, int y, int w, int h, const char *label = nullptr);

    void draw() override;
    int handle(int event) override;

    /**
     * @brief Set the server/guild being displayed
     * @param guildId Guild ID
     * @param guildName Guild name
     */
    void setGuild(const std::string &guildId, const std::string &guildName);

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

  private:
    struct ChannelItem {
        std::string id;
        std::string name;
        bool isVoice;
        bool hasUnread = false;
        int yPos = 0;
    };

    std::string m_guildId;
    std::string m_guildName;
    std::vector<ChannelItem> m_textChannels;
    std::vector<ChannelItem> m_voiceChannels;
    std::string m_selectedChannelId;
    int m_hoveredChannelIndex = -1;
    bool m_hoveredIsVoice = false;
    bool m_textChannelsCollapsed = false;
    bool m_voiceChannelsCollapsed = false;
    int m_scrollOffset = 0;
    std::function<void(const std::string &)> m_onChannelSelected;

    static constexpr int HEADER_HEIGHT = 48;
    static constexpr int CHANNEL_HEIGHT = 32;
    static constexpr int CATEGORY_HEIGHT = 28;
    static constexpr int INDENT = 8;

    void drawServerHeader();
    void drawChannelCategory(const char *title, int &yPos, bool collapsed, int channelCount);
    void drawChannel(const ChannelItem &channel, int yPos, bool selected, bool hovered);
    int getChannelAt(int mx, int my, bool &isVoice) const;
};
