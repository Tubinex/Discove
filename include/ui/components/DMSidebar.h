#pragma once

#include <FL/Fl_Group.H>
#include <functional>
#include <string>
#include <vector>

class DMSidebar : public Fl_Group {
  public:
    DMSidebar(int x, int y, int w, int h, const char *label = nullptr);
    ~DMSidebar();

    void draw() override;
    int handle(int event) override;

    /**
     * @brief Set the selected DM
     * @param dmId DM channel ID to select
     */
    void setSelectedDM(const std::string &dmId);

    /**
     * @brief Set callback for DM selection
     * @param callback Callback function(dmId)
     */
    void setOnDMSelected(std::function<void(const std::string &)> callback) { m_onDMSelected = callback; }

  private:
    struct DMItem {
        std::string id;
        std::string username;
        std::string avatarUrl;
        std::string status = "offline";
        int yPos = 0;
    };

    void addPlaceholderDMs();

    std::vector<DMItem> m_dms;
    std::string m_selectedDMId;
    int m_hoveredDMIndex = -1;
    int m_scrollOffset = 0;
    std::function<void(const std::string &)> m_onDMSelected;

    static constexpr int HEADER_HEIGHT = 48;
    static constexpr int DM_HEIGHT = 44;
    static constexpr int INDENT = 8;

    void drawHeader();
    void drawDM(const DMItem &dm, int yPos, bool selected, bool hovered);
    int getDMAt(int mx, int my) const;
};
