#include "ui/VirtualScroll.h"
#include <algorithm>

namespace VirtualScroll {

ViewportRange calculateRange(int scrollOffset, int viewportHeight, const std::vector<int> &itemOffsets,
                             const std::vector<int> &itemHeights, int renderPadding) {
    ViewportRange range;
    if (itemOffsets.empty() || itemHeights.empty()) {
        range.firstVisible = 0;
        range.lastVisible = -1;
        range.renderFirst = 0;
        range.renderLast = -1;
        return range;
    }

    int viewportTop = scrollOffset;
    int viewportBottom = scrollOffset + viewportHeight;
    int renderTop = viewportTop - renderPadding;
    int renderBottom = viewportBottom + renderPadding;

    renderTop = std::max(0, renderTop);
    int numItems = static_cast<int>(itemOffsets.size());

    range.firstVisible = numItems - 1;
    for (int i = 0; i < numItems; ++i) {
        if (itemOffsets[i] + itemHeights[i] > viewportTop) {
            range.firstVisible = i;
            break;
        }
    }

    range.lastVisible = 0;
    for (int i = numItems - 1; i >= 0; --i) {
        if (itemOffsets[i] < viewportBottom) {
            range.lastVisible = i;
            break;
        }
    }

    range.renderFirst = numItems - 1;
    for (int i = 0; i < numItems; ++i) {
        if (itemOffsets[i] + itemHeights[i] > renderTop) {
            range.renderFirst = i;
            break;
        }
    }

    range.renderLast = 0;
    for (int i = numItems - 1; i >= 0; --i) {
        if (itemOffsets[i] < renderBottom) {
            range.renderLast = i;
            break;
        }
    }

    return range;
}

void HeightCache::setHeight(size_t index, int height) {
    if (index >= m_heights.size()) {
        resize(index + 1);
    }

    if (m_heights[index] != height) {
        m_heights[index] = height;
        m_offsetsDirty = true;
    }
}

int HeightCache::getHeight(size_t index) const {
    if (index >= m_heights.size()) {
        return 0;
    }
    return m_heights[index];
}

bool HeightCache::hasHeight(size_t index) const { return index < m_heights.size() && m_heights[index] > 0; }

int HeightCache::getTotalHeight() const {
    if (m_heights.empty()) {
        return 0;
    }

    if (m_offsetsDirty) {
        rebuildOffsets();
    }

    return m_offsets.back() + m_heights.back();
}

int HeightCache::getOffsetAt(size_t index) const {
    if (index >= m_offsets.size()) {
        return 0;
    }

    if (m_offsetsDirty) {
        rebuildOffsets();
    }

    return m_offsets[index];
}

const std::vector<int> &HeightCache::getOffsets() const {
    if (m_offsetsDirty) {
        rebuildOffsets();
    }
    return m_offsets;
}

const std::vector<int> &HeightCache::getHeights() const { return m_heights; }

void HeightCache::resize(size_t newSize) {
    m_heights.resize(newSize, 0);
    m_offsets.resize(newSize, 0);
    m_offsetsDirty = true;
}

void HeightCache::clear() {
    m_heights.clear();
    m_offsets.clear();
    m_offsetsDirty = false;
}

void HeightCache::rebuildOffsets() const {
    if (m_heights.empty()) {
        m_offsetsDirty = false;
        return;
    }

    if (m_offsets.size() != m_heights.size()) {
        m_offsets.resize(m_heights.size());
    }

    int cumulativeOffset = 0;
    for (size_t i = 0; i < m_heights.size(); ++i) {
        m_offsets[i] = cumulativeOffset;
        cumulativeOffset += m_heights[i];
    }

    m_offsetsDirty = false;
}

} // namespace VirtualScroll
