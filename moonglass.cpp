#include "moonglass.h"

#include <kwin/core/renderviewport.h>
#include <kwin/effect/effecthandler.h>
#include <kwin/opengl/glplatform.h>
#include <kwin/opengl/glutils.h>

#include <QMatrix4x4>
#include <QFile>
#include <QAction>
#include <KPluginFactory>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <algorithm>

KWIN_EFFECT_FACTORY_SUPPORTED(KWin::MoonglassEffect, "moonglass.json", return KWin::MoonglassEffect::supported();)

static void ensureResources()
{
    Q_INIT_RESOURCE(moonglass);
}

namespace KWin {

MoonglassEffect::MoonglassEffect()
    : m_valid(true)
    , m_inited(false)
    , m_opaqueThreshold(0.8f)
    , m_transparentThreshold(0.2f)
    , m_backgroundOpacity(0.0f)
    , m_backgroundColor(Qt::black)
    , m_clipOverlaps(true)
    , m_applyToNewWindows(false)
{
    // Only react to window close to clean up our list
    connect(effects, &EffectsHandler::windowAdded, this, &MoonglassEffect::windowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &MoonglassEffect::windowClosed);
    connect(effects, &EffectsHandler::stackingOrderChanged, this, [this]() {
        if (m_clipOverlaps) {
            repaintTrackedWindows();
        }
    });

    QAction *a = new QAction(this);
    // Use a versioned action id to avoid stale KGlobalAccel state from earlier
    // broken shortcut experiments being autoloaded for this effect.
    a->setObjectName(QStringLiteral("MoonglassToggle"));
    a->setText(i18n("Toggle Moonglass"));
    const QList<QKeySequence> shortcut{QKeySequence(Qt::META | Qt::Key_Z)};
    KGlobalAccel::self()->setDefaultShortcut(a, shortcut, KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(a, shortcut, KGlobalAccel::NoAutoloading);
    connect(a, &QAction::triggered, this, &MoonglassEffect::toggleWindow);

    reconfigure(ReconfigureAll);
    applyDefaultRulesToExistingWindows();
}

MoonglassEffect::~MoonglassEffect() = default;

bool MoonglassEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void MoonglassEffect::reconfigure(ReconfigureFlags)
{
    // The config KCM writes kwinrc in a different process. KWin keeps a cached
    // KSharedConfig, so explicitly reparse before reading or settings changes
    // appear to be ignored until KWin restarts/reloads the effect.
    effects->config()->reparseConfiguration();

    KConfigGroup conf = effects->config()->group(QStringLiteral("Effect-Moonglass"));
    m_opaqueThreshold = std::clamp(conf.readEntry("OpaqueThreshold", 0.8f), 0.0f, 1.0f);
    m_transparentThreshold = std::clamp(conf.readEntry("TransparentThreshold", 0.2f), 0.0f, 1.0f);
    m_backgroundColor = conf.readEntry("BackgroundColor", QColor(Qt::black));
    m_backgroundOpacity = std::clamp(conf.readEntry("BackgroundOpacity", 0.0f), 0.0f, 1.0f);
    const bool oldClipOverlaps = m_clipOverlaps;
    m_clipOverlaps = conf.readEntry("ClipOverlaps", true);
    m_applyToNewWindows = conf.readEntry("ApplyToNewWindows", false);
    m_defaultOnRules = normalizedRules(conf.readEntry("DefaultOnRules", QStringList{}));
    m_defaultOffRules = normalizedRules(conf.readEntry("DefaultOffRules", QStringList{}));
    
    if (m_valid && m_inited && m_shader) {
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform("opaqueThreshold", m_opaqueThreshold);
        m_shader->setUniform("transparentThreshold", m_transparentThreshold);
        m_shader->setUniform("backgroundOpacity", m_backgroundOpacity);
        m_shader->setUniform("backgroundColor", QVector3D(m_backgroundColor.redF(), m_backgroundColor.greenF(), m_backgroundColor.blueF()));

        for (EffectWindow *w : m_windows) {
            w->addRepaintFull();
        }
        effects->addRepaintFull();
    }
    if (oldClipOverlaps != m_clipOverlaps) {
        repaintTrackedWindows();
    }
}

bool MoonglassEffect::loadShader()
{
    m_inited = true;
    ensureResources();

    m_shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QStringLiteral(":/effects/moonglass/shaders/moonglass.frag"));
    if (!m_shader) {
        qWarning() << "Moonglass: The shader failed to load!";
        return false;
    }

    ShaderBinder binder(m_shader.get());
    m_shader->setUniform("opaqueThreshold", m_opaqueThreshold);
    m_shader->setUniform("transparentThreshold", m_transparentThreshold);
    m_shader->setUniform("backgroundOpacity", m_backgroundOpacity);
    m_shader->setUniform("backgroundColor", QVector3D(m_backgroundColor.redF(), m_backgroundColor.greenF(), m_backgroundColor.blueF()));

    return true;
}

void MoonglassEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if ((m_windows.contains(w) && m_valid) || (!isMoveResizeInProgress() && hasHigherTrackedWindow(w))) {
        data.setTranslucent();
    }
    effects->prePaintWindow(view, w, data, presentTime);
}

void MoonglassEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    // KWin's interactive move/resize path can mispaint when our overlap clipping
    // subtracts moving geometry from the rest of the scene. Keep the shader
    // active so dragged Moonglass windows stay translucent, but pause
    // clipping while any window is being dragged/resized.
    if (isMoveResizeInProgress()) {
        effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
        return;
    }

    if (m_windows.contains(w)) {
        effects->paintWindow(renderTarget, viewport, w, mask, clippedPaintRegion(viewport, w, deviceRegion), data);
        return;
    }
    effects->paintWindow(renderTarget, viewport, w, mask, clippedPaintRegion(viewport, w, deviceRegion), data);
}

Region MoonglassEffect::clippedPaintRegion(const RenderViewport &viewport, EffectWindow *w, const Region &deviceRegion) const
{
    if (!m_clipOverlaps || !m_valid || m_windows.isEmpty() || w->isDesktop() || w->isDock()) {
        return deviceRegion;
    }

    Region clippedRegion = (deviceRegion == Region::infinite()) ? Region(viewport.deviceRect()) : deviceRegion;
    const QList<EffectWindow *> stacking = effects->stackingOrder();
    const int windowIndex = stacking.indexOf(w);
    if (windowIndex < 0) {
        return clippedRegion;
    }

    for (int i = windowIndex + 1; i < stacking.size(); ++i) {
        EffectWindow *above = stacking.at(i);
        if (!m_windows.contains(above) || above->isDeleted() || above->isHidden() || above->isMinimized() || !above->isVisible()) {
            continue;
        }
        // Popup-like windows (menus, tooltips, combos) should not clip
        // what's behind them — only real app windows should clip.
        if (isPopupLike(above)) {
            continue;
        }
        clippedRegion -= viewport.mapToDeviceCoordinatesAligned(above->frameGeometry());
        if (clippedRegion.isEmpty()) {
            break;
        }
    }

    return clippedRegion;
}

bool MoonglassEffect::hasHigherTrackedWindow(EffectWindow *w) const
{
    if (!m_clipOverlaps || !m_valid || m_windows.isEmpty() || !w || w->isDeleted() || w->isDesktop() || w->isDock()) {
        return false;
    }

    const QList<EffectWindow *> stacking = effects->stackingOrder();
    const int windowIndex = stacking.indexOf(w);
    if (windowIndex < 0) {
        return false;
    }

    const QRectF windowGeometry = w->frameGeometry();
    for (int i = windowIndex + 1; i < stacking.size(); ++i) {
        EffectWindow *above = stacking.at(i);
        if (!m_windows.contains(above) || above->isDeleted() || above->isHidden() || above->isMinimized() || !above->isVisible()) {
            continue;
        }
        if (isPopupLike(above)) {
            continue;
        }
        if (above->frameGeometry().intersects(windowGeometry)) {
            return true;
        }
    }

    return false;
}

bool MoonglassEffect::isMoveResizeInProgress() const
{
    for (EffectWindow *w : effects->stackingOrder()) {
        if (w && !w->isDeleted() && (w->isUserMove() || w->isUserResize())) {
            return true;
        }
    }
    return false;
}

bool MoonglassEffect::isPopupLike(const EffectWindow *w)
{
    return w && (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()
              || w->isPopupWindow() || w->isTooltip() || w->isComboBox());
}

void MoonglassEffect::repaintTrackedWindows()
{
    for (EffectWindow *w : m_windows) {
        if (w && !w->isDeleted()) {
            w->addRepaintFull();
        }
    }
    effects->addRepaintFull();
}

void MoonglassEffect::windowAdded(EffectWindow *w)
{
    if (!shouldDefaultOn(w)) {
        return;
    }
    if (!w->isDesktop() && !w->isDock() && !w->isDeleted() && !m_windows.contains(w)) {
        applyEffectToWindow(w);
    }
}

void MoonglassEffect::applyDefaultRulesToExistingWindows()
{
    for (EffectWindow *w : effects->stackingOrder()) {
        if (shouldDefaultOn(w)) {
            applyEffectToWindow(w);
        }
    }
}

void MoonglassEffect::windowClosed(EffectWindow *w)
{
    if (m_windows.contains(w)) {
        unredirect(w);
        m_windows.removeOne(w);
        repaintTrackedWindows();
    }
}

void MoonglassEffect::toggleWindow()
{
    if (auto w = effects->activeWindow()) {
        if (m_windows.contains(w)) {
            unredirect(w);
            m_windows.removeOne(w);
            w->addRepaintFull();
            repaintTrackedWindows();
        } else {
            if (!w->isDesktop() && !w->isDock() && !w->isDeleted()) {
                applyEffectToWindow(w);
            }
        }
    }
}

void MoonglassEffect::applyEffectToWindow(EffectWindow *w)
{
    if (!w || w->isDesktop() || w->isDock() || w->isDeleted() || isPopupLike(w) || m_windows.contains(w)) {
        return;
    }
    if (m_valid && !m_inited) {
        m_valid = loadShader();
    }
    if (!m_valid) {
        return;
    }

    m_windows.append(w);
    connect(w, &EffectWindow::windowStartUserMovedResized, this, &MoonglassEffect::repaintTrackedWindows, Qt::UniqueConnection);
    connect(w, &EffectWindow::windowStepUserMovedResized, this, &MoonglassEffect::repaintTrackedWindows, Qt::UniqueConnection);
    connect(w, &EffectWindow::windowFinishUserMovedResized, this, &MoonglassEffect::repaintTrackedWindows, Qt::UniqueConnection);
    redirect(w);
    setShader(w, m_shader.get());
    w->addRepaintFull();
    repaintTrackedWindows();
}

bool MoonglassEffect::shouldDefaultOn(EffectWindow *w) const
{
    if (!w || w->isDesktop() || w->isDock() || w->isDeleted() || isPopupLike(w)) {
        return false;
    }
    if (ruleListMatches(m_defaultOffRules, w)) {
        return false;
    }
    if (ruleListMatches(m_defaultOnRules, w)) {
        return true;
    }
    return m_applyToNewWindows;
}

QStringList MoonglassEffect::normalizedRules(const QStringList &rules) const
{
    QStringList normalized;
    for (const QString &entry : rules) {
        const QStringList lines = entry.split(QLatin1Char('\n'));
        for (QString line : lines) {
            line = line.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            normalized.append(line.toLower());
        }
    }
    normalized.removeDuplicates();
    return normalized;
}

bool MoonglassEffect::ruleListMatches(const QStringList &rules, EffectWindow *w) const
{
    if (!w || rules.isEmpty()) {
        return false;
    }

    const QString windowClass = w->windowClass().trimmed().toLower();
    const QString windowRole = w->windowRole().trimmed().toLower();
    const QString tag = w->tag().trimmed().toLower();
    const QString caption = w->caption().trimmed().toLower();
    QStringList ids{windowClass, windowRole, tag};

    QString classParts = windowClass;
    classParts.replace(QLatin1Char('.'), QLatin1Char(' '));
    classParts.replace(QLatin1Char('/'), QLatin1Char(' '));
    for (const QString &part : classParts.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        ids.append(part);
    }
    ids.removeAll(QString());
    ids.removeDuplicates();

    for (const QString &rule : rules) {
        if (rule.startsWith(QStringLiteral("class:"))) {
            const QString value = rule.mid(6).trimmed();
            if (ids.contains(value)) {
                return true;
            }
        } else if (rule.startsWith(QStringLiteral("role:"))) {
            if (windowRole == rule.mid(5).trimmed()) {
                return true;
            }
        } else if (rule.startsWith(QStringLiteral("tag:"))) {
            if (tag == rule.mid(4).trimmed()) {
                return true;
            }
        } else if (rule.startsWith(QStringLiteral("title:"))) {
            if (caption.contains(rule.mid(6).trimmed())) {
                return true;
            }
        } else if (rule.startsWith(QStringLiteral("caption:"))) {
            if (caption.contains(rule.mid(8).trimmed())) {
                return true;
            }
        } else if (rule.startsWith(QStringLiteral("pid:"))) {
            if (QString::number(w->pid()) == rule.mid(4).trimmed()) {
                return true;
            }
        } else if (ids.contains(rule)) {
            return true;
        }
    }

    return false;
}

QString MoonglassEffect::suggestedWindowRule(EffectWindow *w) const
{
    if (!w) {
        return QString();
    }
    const QString windowClass = w->windowClass().trimmed();
    if (!windowClass.isEmpty()) {
        const QStringList parts = windowClass.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        return parts.isEmpty() ? windowClass : parts.last();
    }
    const QString tag = w->tag().trimmed();
    if (!tag.isEmpty()) {
        return QStringLiteral("tag:%1").arg(tag);
    }
    const QString role = w->windowRole().trimmed();
    if (!role.isEmpty()) {
        return QStringLiteral("role:%1").arg(role);
    }
    return QStringLiteral("title:%1").arg(w->caption().trimmed());
}

QString MoonglassEffect::windowInfo(EffectWindow *w) const
{
    if (!w) {
        return QStringLiteral("status=cancelled\nrule=\nclass=\nrole=\ntag=\ntitle=\npid=");
    }

    auto oneLine = [](QString value) {
        value.replace(QLatin1Char('\n'), QLatin1Char(' '));
        value.replace(QLatin1Char('\r'), QLatin1Char(' '));
        return value.trimmed();
    };

    return QStringLiteral("status=selected\nrule=%1\nclass=%2\nrole=%3\ntag=%4\ntitle=%5\npid=%6")
        .arg(oneLine(suggestedWindowRule(w)),
             oneLine(w->windowClass()),
             oneLine(w->windowRole()),
             oneLine(w->tag()),
             oneLine(w->caption()),
             QString::number(w->pid()));
}

void MoonglassEffect::startInteractiveRuleSelection()
{
    m_lastSelectedWindowInfo = QStringLiteral("status=pending\nrule=\nclass=\nrole=\ntag=\ntitle=\npid=");
    effects->startInteractiveWindowSelection([this](EffectWindow *w) {
        m_lastSelectedWindowInfo = windowInfo(w);
    });
}

bool MoonglassEffect::isActive() const
{
    return m_valid && !m_windows.isEmpty();
}

QString MoonglassEffect::debug(const QString &parameter) const
{
    if (parameter == QStringLiteral("select-window")) {
        const_cast<MoonglassEffect *>(this)->startInteractiveRuleSelection();
        return QStringLiteral("status=pending");
    }
    if (parameter == QStringLiteral("selected-window")) {
        return m_lastSelectedWindowInfo.isEmpty()
            ? QStringLiteral("status=none\nrule=\nclass=\nrole=\ntag=\ntitle=\npid=")
            : m_lastSelectedWindowInfo;
    }
    if (parameter == QStringLiteral("active-window")) {
        return windowInfo(effects->activeWindow());
    }

    return QStringLiteral("opaqueThreshold=%1 transparentThreshold=%2 backgroundOpacity=%3 backgroundColor=%4,%5,%6 clipOverlaps=%7 applyToNewWindows=%8 windows=%9 shaderLoaded=%10 defaultOnRules=%11 defaultOffRules=%12 moveResizeInProgress=%13")
        .arg(m_opaqueThreshold, 0, 'f', 3)
        .arg(m_transparentThreshold, 0, 'f', 3)
        .arg(m_backgroundOpacity, 0, 'f', 3)
        .arg(m_backgroundColor.red())
        .arg(m_backgroundColor.green())
        .arg(m_backgroundColor.blue())
        .arg(m_clipOverlaps ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_applyToNewWindows ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_windows.size())
        .arg(m_shader ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_defaultOnRules.size())
        .arg(m_defaultOffRules.size())
        .arg(isMoveResizeInProgress() ? QStringLiteral("true") : QStringLiteral("false"));
}

} // namespace KWin

#include "moonglass.moc"
