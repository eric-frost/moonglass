#include "moonglass_config.h"
#include "moonglassconfig.h" // generated from kcfgc

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTimer>
#include <KPluginFactory>
#include <KLocalizedString>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

K_PLUGIN_CLASS_WITH_JSON(KWin::MoonglassEffectConfig, "kwin_moonglass_config.json")

namespace KWin
{

MoonglassEffectConfig::MoonglassEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    
    // Hardcode the config filename since KWIN_CONFIG macro might not be available
    MoonglassConfigKcfg::instance(QStringLiteral("kwinrc"));
    addConfig(MoonglassConfigKcfg::self(), widget());

    m_selectionPollTimer = new QTimer(this);
    m_selectionPollTimer->setInterval(250);
    connect(m_selectionPollTimer, &QTimer::timeout, this, &MoonglassEffectConfig::pollSelectedWindow);

    connect(m_ui.selectWindowButton, &QPushButton::clicked, this, &MoonglassEffectConfig::selectWindow);
    connect(m_ui.addSelectedToDefaultOffButton, &QPushButton::clicked, this, &MoonglassEffectConfig::addSelectedToDefaultOff);
    connect(m_ui.addSelectedToDefaultOnButton, &QPushButton::clicked, this, &MoonglassEffectConfig::addSelectedToDefaultOn);
    connect(m_ui.defaultOffRulesText, &QTextEdit::textChanged, this, &MoonglassEffectConfig::markAsChanged);
    connect(m_ui.defaultOnRulesText, &QTextEdit::textChanged, this, &MoonglassEffectConfig::markAsChanged);

    loadRuleTexts();
}

void MoonglassEffectConfig::load()
{
    KCModule::load();
    loadRuleTexts();
    m_selectedRule.clear();
    m_ui.selectedWindowInfoLabel->setText(i18n("No window selected."));
    m_ui.addSelectedToDefaultOffButton->setEnabled(false);
    m_ui.addSelectedToDefaultOnButton->setEnabled(false);
}

void MoonglassEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-Moonglass"));
    conf.writeEntry("DefaultOffRules", rulesFromText(m_ui.defaultOffRulesText->toPlainText()));
    conf.writeEntry("DefaultOnRules", rulesFromText(m_ui.defaultOnRulesText->toPlainText()));
    conf.sync();

    // Notify KWin to reload the effect configuration
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect")
    );
    message << QStringLiteral("kwin-effect-moonglass");
    QDBusConnection::sessionBus().send(message);
}

void MoonglassEffectConfig::defaults()
{
    KCModule::defaults();
    m_ui.defaultOffRulesText->clear();
    m_ui.defaultOnRulesText->clear();
}

QStringList MoonglassEffectConfig::rulesFromText(const QString &text) const
{
    QStringList rules;
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            rules.append(line);
        }
    }
    return rules;
}

QString MoonglassEffectConfig::rulesToText(const QStringList &rules) const
{
    return rules.join(QLatin1Char('\n'));
}

void MoonglassEffectConfig::loadRuleTexts()
{
    KConfigGroup conf = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-Moonglass"));
    const QSignalBlocker offBlocker(m_ui.defaultOffRulesText);
    const QSignalBlocker onBlocker(m_ui.defaultOnRulesText);
    m_ui.defaultOffRulesText->setPlainText(rulesToText(conf.readEntry("DefaultOffRules", QStringList{})));
    m_ui.defaultOnRulesText->setPlainText(rulesToText(conf.readEntry("DefaultOnRules", QStringList{})));
}

QString MoonglassEffectConfig::effectDebug(const QString &parameter) const
{
    QDBusInterface iface(QStringLiteral("org.kde.KWin"),
                         QStringLiteral("/Effects"),
                         QStringLiteral("org.kde.kwin.Effects"),
                         QDBusConnection::sessionBus());
    QDBusReply<QString> reply = iface.call(QStringLiteral("debug"), QStringLiteral("kwin-effect-moonglass"), parameter);
    if (!reply.isValid()) {
        return QString();
    }
    return reply.value();
}

QHash<QString, QString> MoonglassEffectConfig::parseWindowInfo(const QString &info) const
{
    QHash<QString, QString> parsed;
    for (const QString &line : info.split(QLatin1Char('\n'))) {
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        parsed.insert(line.left(equals), line.mid(equals + 1));
    }
    return parsed;
}

void MoonglassEffectConfig::setSelectedWindowInfo(const QHash<QString, QString> &info)
{
    m_selectedRule = info.value(QStringLiteral("rule")).trimmed();
    const QString status = info.value(QStringLiteral("status"));
    if (status == QStringLiteral("selected") && !m_selectedRule.isEmpty()) {
        m_ui.selectedWindowInfoLabel->setText(i18n("Suggested rule: %1\nClass: %2\nTitle: %3", m_selectedRule, info.value(QStringLiteral("class")), info.value(QStringLiteral("title"))));
        m_ui.addSelectedToDefaultOffButton->setEnabled(true);
        m_ui.addSelectedToDefaultOnButton->setEnabled(true);
    } else if (status == QStringLiteral("cancelled")) {
        m_ui.selectedWindowInfoLabel->setText(i18n("Window selection cancelled."));
        m_ui.addSelectedToDefaultOffButton->setEnabled(false);
        m_ui.addSelectedToDefaultOnButton->setEnabled(false);
    }
}

void MoonglassEffectConfig::selectWindow()
{
    const QString result = effectDebug(QStringLiteral("select-window"));
    if (result.isEmpty()) {
        m_ui.selectedWindowInfoLabel->setText(i18n("Could not contact KWin effect."));
        return;
    }
    m_selectedRule.clear();
    m_ui.addSelectedToDefaultOffButton->setEnabled(false);
    m_ui.addSelectedToDefaultOnButton->setEnabled(false);
    m_ui.selectedWindowInfoLabel->setText(i18n("Click a window to detect its rule…"));
    m_selectionPollTimer->start();
}

void MoonglassEffectConfig::pollSelectedWindow()
{
    const QString result = effectDebug(QStringLiteral("selected-window"));
    const QHash<QString, QString> info = parseWindowInfo(result);
    const QString status = info.value(QStringLiteral("status"));
    if (status == QStringLiteral("selected") || status == QStringLiteral("cancelled")) {
        m_selectionPollTimer->stop();
        setSelectedWindowInfo(info);
    }
}

void MoonglassEffectConfig::appendRuleToTextEdit(QTextEdit *edit, const QString &rule)
{
    if (rule.trimmed().isEmpty()) {
        return;
    }
    QStringList rules = rulesFromText(edit->toPlainText());
    for (const QString &existing : rules) {
        if (existing.compare(rule, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    rules.append(rule);
    edit->setPlainText(rulesToText(rules));
    markAsChanged();
}

void MoonglassEffectConfig::addSelectedToDefaultOff()
{
    appendRuleToTextEdit(m_ui.defaultOffRulesText, m_selectedRule);
}

void MoonglassEffectConfig::addSelectedToDefaultOn()
{
    appendRuleToTextEdit(m_ui.defaultOnRulesText, m_selectedRule);
}

} // namespace KWin

#include "moonglass_config.moc"
