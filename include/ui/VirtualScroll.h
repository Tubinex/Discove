#ifndef VIRTUAL_SCROLL_H
#define VIRTUAL_SCROLL_H

#include <algorithm>
#include <vector>

namespace VirtualScroll {

struct ViewportRange {
    int firstVisible;
    int lastVisible;
    int renderFirst;
    int renderLast;
};

ViewportRange calculateRange(int scrollOffset, int viewportHeight, const std::vector<int> &itemOffsets,
                             const std::vector<int> &itemHeights, int renderPadding = 200);

class HeightCache {
  public:
    HeightCache() = default;
    void setHeight(size_t index, int height);
    int getHeight(size_t index) const;
    bool hasHeight(size_t index) const;
    int getTotalHeight() const;
    int getOffsetAt(size_t index) const;
    const std::vector<int> &getOffsets() const;
    const std::vector<int> &getHeights() const;
    void resize(size_t newSize);
    void clear();
    size_t size() const { return m_heights.size(); }

  private:
    std::vector<int> m_heights;
    mutable std::vector<int> m_offsets;
    mutable bool m_offsetsDirty = false;
    void rebuildOffsets() const;
};

} // namespace VirtualScroll

#endif
