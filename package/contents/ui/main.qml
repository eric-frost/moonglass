import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kcmutils 2.0 as KCM

KCM.SimpleKCM {
    id: root

    Kirigami.FormLayout {
        anchors.fill: parent

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Brightness Threshold:")
            from: 0
            to: 100
            value: kcm.ruleThreshold * 100
            onValueChanged: kcm.ruleThreshold = value / 100.0
            
            property real realValue: value / 100.0
            
            validator: DoubleValidator {
                bottom: 0.0
                top: 1.0
            }
            
            textFromValue: function(value, locale) {
                return Number(value / 100.0).toLocaleString(locale, 'f', 2)
            }
            
            valueFromText: function(text, locale) {
                return Number.fromLocaleString(locale, text) * 100
            }
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Dark Text Opacity:")
            from: 0
            to: 100
            value: kcm.ruleDarkOpacity * 100
            onValueChanged: kcm.ruleDarkOpacity = value / 100.0
            
            property real realValue: value / 100.0
            
            validator: DoubleValidator {
                bottom: 0.0
                top: 1.0
            }
            
            textFromValue: function(value, locale) {
                return Number(value / 100.0).toLocaleString(locale, 'f', 2)
            }
            
            valueFromText: function(text, locale) {
                return Number.fromLocaleString(locale, text) * 100
            }
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Bright Background Opacity:")
            from: 0
            to: 100
            value: kcm.ruleBackgroundAlpha * 100
            onValueChanged: kcm.ruleBackgroundAlpha = value / 100.0
            
            property real realValue: value / 100.0
            
            validator: DoubleValidator {
                bottom: 0.0
                top: 1.0
            }
            
            textFromValue: function(value, locale) {
                return Number(value / 100.0).toLocaleString(locale, 'f', 2)
            }
            
            valueFromText: function(text, locale) {
                return Number.fromLocaleString(locale, text) * 100
            }
        }
    }
}
