#pragma once

#include <FL/Fl_Group.H>
#include <memory>
#include <string>
#include <unordered_map>

class Screen : public Fl_Group {
  public:
    struct Context {
        std::unordered_map<std::string, std::string> params;
        std::unordered_map<std::string, std::string> query;
        std::string path;
    };

    Screen(int x, int y, int w, int h, const char *label = nullptr);
    virtual ~Screen() = default;

    virtual void onCreate(const Context &ctx) {}
    virtual void onEnter(const Context &ctx) {}
    virtual void onExit() {}
    virtual void onDestroy() {}

    virtual void onTransitionIn(float progress) {}
    virtual void onTransitionOut(float progress) {}

    const Context &context() const { return m_context; }

  protected:
    friend class Router;
    void setContext(const Context &ctx) { m_context = ctx; }

  private:
    Context m_context;
};
