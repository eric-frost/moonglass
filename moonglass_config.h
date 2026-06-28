#ifndef MOONGLASS_CONFIG_H
#define MOONGLASS_CONFIG_H

#include <KCModule>
#include "ui_config.h"

#include <QHash>
#include <QString>
#include <QStringList>

class QTimer;
class QTextEdit;

namespace KWin
{

class MoonglassEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit MoonglassEffectConfig(QObject *parent, const KPluginMetaData &data);
    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void selectWindow();
    void pollSelectedWindow();
    void addSelectedToDefaultOff();
    void addSelectedToDefaultOn();

private:
    QStringList rulesFromText(const QString &text) const;
    QString rulesToText(const QStringList &rules) const;
    void loadRuleTexts();
    void appendRuleToTextEdit(QTextEdit *edit, const QString &rule);
    QString effectDebug(const QString &parameter) const;
    QHash<QString, QString> parseWindowInfo(const QString &info) const;
    void setSelectedWindowInfo(const QHash<QString, QString> &info);

    Ui::MoonglassConfig m_ui;
    QTimer *m_selectionPollTimer = nullptr;
    QString m_selectedRule;
};

} // namespace KWin

#endif // MOONGLASS_CONFIG_H
