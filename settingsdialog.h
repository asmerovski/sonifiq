#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include <QSettings>

// Describes one output format and its required ffmpeg codec
struct FormatEntry {
    QString id;        // internal key, e.g. "mp3"
    QString label;     // display name
    QString codec;     // ffmpeg codec name to probe, e.g. "libmp3lame"
    bool    available; // set after precheck
    bool    enabled;   // user toggle
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Returns list of enabled format IDs
    QStringList enabledFormats() const;
    bool isFormatEnabled(const QString &id) const;

    static QList<FormatEntry> allFormats();

private slots:
    void runPrecheck();

private:
    void setupUI();
    void applyStylesheet();
    void loadSettings();
    void saveSettings();

    QTabWidget              *m_tabs;
    QList<FormatEntry>       m_formats;
    QMap<QString, QCheckBox*> m_checkboxes;
    QMap<QString, QLabel*>    m_statusLabels;
    QPushButton             *m_btnCheck;
    QLabel                  *m_checkStatus;
};
