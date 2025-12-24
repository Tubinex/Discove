#pragma once

#include <FL/Fl_Widget.H>
#include <functional>
#include <string>
#include <vector>

class GuildBar : public Fl_Widget {
  public:
    GuildBar(int x, int y, int w, int h, const char *label = nullptr);

    void draw() override;
    int handle(int event) override;

    /**
     * @brief Add a guild to the bar
     * @param guildId Guild snowflake ID
     * @param iconUrl Guild icon URL (can be empty for default)
     * @param name Guild name
     */
    void addGuild(const std::string &guildId, const std::string &iconUrl, const std::string &name);

    /**
     * @brief Set the selected guild
     * @param guildId Guild ID to select
     */
    void setSelectedGuild(const std::string &guildId);

    /**
     * @brief Get currently selected guild ID
     * @return Selected guild ID or empty string if none
     */
    std::string getSelectedGuild() const { return m_selectedGuildId; }

    /**
     * @brief Set callback for guild selection changes
     * @param cb Callback function(guildId)
     */
    void setOnGuildSelected(std::function<void(const std::string &)> cb) { m_onGuildSelected = cb; }

  private:
    struct GuildItem {
        std::string id;
        std::string iconUrl;
        std::string name;
        int y;
        bool hasUnread = false;
    };

    std::vector<GuildItem> m_guilds;
    std::string m_selectedGuildId;
    int m_hoveredIndex = -1;
    int m_scrollOffset = 0;
    std::function<void(const std::string &)> m_onGuildSelected;

    static constexpr int GUILD_ICON_SIZE = 48;
    static constexpr int GUILD_SPACING = 8;

    void drawGuildIcon(const GuildItem &guild, int yPos, bool selected, bool hovered);
    int getGuildIndexAt(int mx, int my) const;
};
