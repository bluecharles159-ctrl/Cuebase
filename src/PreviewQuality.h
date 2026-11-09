#pragma once

// Use consecutive render times so a single expensive frame never changes detail.
class AdaptivePreviewQuality {
public:
    double scale() const { return m_quarter ? 0.25 : 0.5; }

    void reset() {
        m_quarter = false;
        m_streak = 0;
    }

    void record(double renderMs, double frameBudgetMs) {
        // Returning to half resolution quadruples the number of pixels to paint.
        const bool change = m_quarter ? renderMs < frameBudgetMs * 0.18
                                      : renderMs > frameBudgetMs * 0.85;
        m_streak = change ? m_streak + 1 : 0;
        if (m_streak >= (m_quarter ? 8 : 3)) {
            m_quarter = !m_quarter;
            m_streak = 0;
        }
    }

private:
    bool m_quarter = false;
    int m_streak = 0;
};
