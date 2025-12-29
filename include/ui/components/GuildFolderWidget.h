#pragma once

#include <FL/Fl_Group.H>
#include <string>
#include <vector>

class GuildIcon;

class GuildFolderWidget : public Fl_Group {
  public:
    GuildFolderWidget(int x, int y, int size);
    ~GuildFolderWidget() override;

    void addGuild(GuildIcon *icon);

    void setColor(unsigned int color) {
        color_ = color;
        redraw();
    }
    void setName(const std::string &name) { name_ = name; }
    void setFolderIconSize(int size) {
        folderIconSize_ = size;
        redraw();
    }
    void setExpanded(bool expanded);

    void toggle();
    bool isExpanded() const { return expanded_; }

    void layoutIcons();

  protected:
    void draw() override;

  private:
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    static void animationTimerCallback(void *data);
    void updateAnimation();
    int getCollapsedHeight() const { return iconSize_; }
    int getExpandedHeight() const {
        const int folderIconSize = static_cast<int>(iconSize_ * 0.85);
        return iconSize_ + (static_cast<int>(guildIcons_.size()) * (folderIconSize + 8)) + 4;
    }

    std::vector<GuildIcon *> guildIcons_;
    std::string name_;
    unsigned int color_{0};
    int iconSize_{48};
    int cornerRadius_{12};
    int folderIconSize_{20};  // Size of the folder SVG icon
    bool expanded_{false};
    bool hovered_{false};

    bool animating_{false};
    float animationProgress_{0.0f};
    bool targetExpanded_{false};
};
