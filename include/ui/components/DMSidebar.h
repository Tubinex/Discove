#pragma once

#include <FL/Fl_Group.H>
#include <functional>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "state/Store.h"
#include "ui/AnimationManager.h"
#include "ui/GifAnimation.h"

class Fl_RGB_Image;
class GifAnimation;

class DMSidebar : public Fl_Group {
  public:
    DMSidebar(int x, int y, int w, int h, const char *label = nullptr);
    ~DMSidebar();

    void draw() override;
    int handle(int event) override;

    void setSelectedDM(const std::string &dmId);

    void setOnDMSelected(std::function<void(const std::string &)> callback) { m_onDMSelected = callback; }

    int getScrollOffset() const { return m_scrollOffset; }

    void setScrollOffset(int offset);

  private:
    struct AnimatedAvatarState {
        std::unique_ptr<GifAnimation> animation;
        std::vector<std::unique_ptr<Fl_RGB_Image>> frames;
        AnimationManager::AnimationId animationId = 0;
        double frameTimeAccumulated = 0.0;
        bool running = false;
    };

    struct DMItem {
        std::string id;
        std::string displayName;
        std::string recipientId;
        std::string avatarUrl;
        std::string animatedAvatarUrl;
        std::string avatarLabel;
        std::string secondaryAvatarUrl;
        std::string secondaryAnimatedAvatarUrl;
        std::string secondaryAvatarLabel;
        std::string status = "offline";
        bool showStatus = false;
        int yPos = 0;
    };

    void loadDMsFromState();
    void updateStatuses(const std::unordered_map<std::string, std::string> &statuses);

    std::vector<DMItem> m_dms;
    std::string m_selectedDMId;
    int m_hoveredDMIndex = -1;
    int m_scrollOffset = 0;
    std::function<void(const std::string &)> m_onDMSelected;
    Store::ListenerId m_storeListenerId = 0;
    Store::ListenerId m_statusListenerId = 0;
    Store::ListenerId m_userListenerId = 0;
    std::shared_ptr<bool> m_isAlive;

    std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> m_avatarCache;
    std::unordered_set<std::string> m_avatarPending;
    std::unordered_map<std::string, AnimatedAvatarState> m_avatarGifCache;
    std::unordered_set<std::string> m_avatarGifPending;
    std::string m_hoveredAvatarKey;

    std::deque<std::string> m_userResolveQueue;
    std::unordered_set<std::string> m_userResolveQueuedUserIds;
    int m_userResolveInFlight = 0;

    static constexpr int HEADER_HEIGHT = 48;
    static constexpr int DM_HEIGHT = 44;
    static constexpr int LIST_PADDING = 16;
    static constexpr int INDENT = 8;

    void drawHeader();
    void drawDM(const DMItem &dm, int yPos, bool selected, bool hovered);
    int getDMAt(int mx, int my) const;
    int calculateContentHeight() const;

    Fl_RGB_Image *getCircularAvatar(const std::string &url, int size);
    void requestCircularAvatar(const std::string &url, int size);

    Fl_RGB_Image *getAnimatedAvatarFrame(const std::string &gifUrl, int size);
    void ensureAnimatedAvatar(const std::string &gifUrl, int size);
    void setHoveredAvatarKey(const std::string &key);
    bool updateAvatarAnimation(const std::string &key);
    void startAvatarAnimation(const std::string &key);
    void stopAvatarAnimation(const std::string &key);

    void enqueueUserResolve(const std::string &userId);
    void pumpUserResolveQueue();
};
