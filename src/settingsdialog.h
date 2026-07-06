#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
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
    // Persisted parallel-thread count (used by MainWindow when starting conversions)
    static int savedThreadCount();
    // Headless codec probe — runs ffmpeg -encoders and saves availability to
    // QSettings without needing a dialog instance. Called on app startup.
    static void probeAndSaveCodecs();

public slots:
    // Overridden so Cancel / Escape / the window's close button all revert
    // the live theme preview back to whatever was active when the dialog
    // opened — not just clicking the Cancel button specifically.
    void reject() override;

private slots:
    void runPrecheck();
    void restoreDefaultThemes();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QTabWidget              *m_tabs;
    QList<FormatEntry>       m_formats;
    QMap<QString, QCheckBox*> m_checkboxes;
    QMap<QString, QLabel*>    m_statusLabels;
    QPushButton             *m_btnCheck;
    QLabel                  *m_checkStatus;
    QSpinBox                *m_threadsSpin = nullptr;
    QCheckBox                *m_themingEnabledCheck = nullptr;
    QComboBox               *m_themeCombo  = nullptr;
    QString                  m_originalThemeId;
    bool                     m_originalThemingEnabled = true;
};