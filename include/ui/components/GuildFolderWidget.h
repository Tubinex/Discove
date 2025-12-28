#pragma once

#include <FL/Fl_Group.H>
#include <string>
#include <vector>

class GuildIcon;

class GuildFolderWidget : public Fl_Group {
  public:
    GuildFolderWidget(int x, int y, int size);
    ~GuildFolderWidget() override = default;

    void addGuild(GuildIcon *icon);

    void setColor(unsigned int color) {
        color_ = color;
        redraw();
    }
    void setName(const std::string &name) { name_ = name; }
    void setExpanded(bool expanded);

    void toggle();
    bool isExpanded() const { return expanded_; }

    void layoutIcons();

  private:
    void draw() override;
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    std::vector<GuildIcon *> guildIcons_;
    std::string name_;
    unsigned int color_{0};
    int iconSize_{48};
    int cornerRadius_{12};
    bool expanded_{false};
    bool hovered_{false};
};
