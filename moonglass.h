#pragma once

#include <kwin/effect/offscreeneffect.h>
#include <kwin/opengl/glshadermanager.h>
#include <memory>

namespace KWin {

class MoonglassEffect : public OffscreenEffect
{
    Q_OBJECT
public:
    MoonglassEffect();
    ~MoonglassEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;
    bool isActive() const override;
    QString debug(const QString &parameter) const override;

    // Window events
    void windowAdded(EffectWindow *w);
    void windowClosed(EffectWindow *w);

public Q_SLOTS:
    void toggleWindow();

private:
    bool loadShader();
    void applyEffectToWindow(EffectWindow *w);
    Region clippedPaintRegion(const RenderViewport &viewport, EffectWindow *w, const Region &deviceRegion) const;
    bool hasHigherTrackedWindow(EffectWindow *w) const;
    bool isMoveResizeInProgress() const;
    static bool isPopupLike(const EffectWindow *w);
    bool shouldDefaultOn(EffectWindow *w) const;
    void applyDefaultRulesToExistingWindows();
    bool ruleListMatches(const QStringList &rules, EffectWindow *w) const;
    QStringList normalizedRules(const QStringList &rules) const;
    QString suggestedWindowRule(EffectWindow *w) const;
    QString windowInfo(EffectWindow *w) const;
    void startInteractiveRuleSelection();
    void repaintTrackedWindows();

    std::unique_ptr<GLShader> m_shader;
    bool m_valid;
    bool m_inited;

    float m_opaqueThreshold;
    float m_transparentThreshold;
    float m_backgroundOpacity;
    QColor m_backgroundColor;
    bool m_clipOverlaps;
    bool m_applyToNewWindows;
    QStringList m_defaultOnRules;
    QStringList m_defaultOffRules;
    QString m_lastSelectedWindowInfo;
    QList<EffectWindow*> m_windows;
};

} // namespace KWin
