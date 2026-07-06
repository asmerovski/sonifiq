#include "settingsdialog.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QProcess>
#include <QSettings>
#include <QDialogButtonBox>
#include <QApplication>
#include <QStyle>
#include <QLabel>
#include <QThread>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFont>
#include <QMessageBox>

// ── Static format registry ────────────────────────────────────────────────────

QList<FormatEntry> SettingsDialog::allFormats() {
    return {
             { "mp3",  "MP3",                      "libmp3lame",  false, true },
             { "m4a",  "AAC (.m4a)",               "aac",         false, true },
             { "ogg",  "Ogg Vorbis (.ogg)",        "libvorbis",   false, true },
             { "opus", "Opus (.opus)",              "libopus",     false, true },
             { "flac", "FLAC",                     "flac",        false, true },
             { "wav",  "WAV (PCM)",                "pcm_s16le",   false, true },
             { "aiff", "AIFF",                     "pcm_s16be",   false, true },
             { "wv",   "WavPack (.wv)",            "wavpack",     false, true },
             { "mp2",  "MP2",                      "mp2",         false, true },
             { "ac3",  "AC3 (Dolby Digital)",      "ac3",         false, true },
             { "mka",  "MKA (Matroska Audio)",     "copy",        false, true },
             { "caf",  "CAF (Apple Core Audio)",   "pcm_s16be",   false, true },
             { "alac", "ALAC (Apple Lossless)",    "alac",        false, true },
             };
}

// ── Constructor ───────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("SonifiQ — Settings");
    setMinimumWidth(520);
    m_formats = allFormats();
    m_originalThemeId = ThemeManager::currentThemeId();
    m_originalThemingEnabled = ThemeManager::themingEnabled();
    loadSettings();
    setupUI();
}

// ── Load / Save ───────────────────────────────────────────────────────────────

void SettingsDialog::reject() {
    // The theme picker (and the theming on/off switch) apply live as the
    // user browses them, so on Cancel / Escape / closing the window, put
    // back whatever was active before this dialog opened. Restore the theme
    // id first, then the enabled flag — that order means if theming ends up
    // enabled, it re-applies with the correct (already-restored) id in one
    // step instead of flashing the last-previewed theme first.
    ThemeManager::applyTheme(m_originalThemeId);
    ThemeManager::setThemingEnabled(m_originalThemingEnabled);
    QDialog::reject();
}

void SettingsDialog::loadSettings() {
    QSettings s("SonifiQ", "SonifiQ");
    for (auto &f : m_formats) {
        f.enabled = s.value("format_enabled/" + f.id, true).toBool();
        f.available = s.value("format_available/" + f.id, false).toBool();
    }
}

int SettingsDialog::savedThreadCount() {
    QSettings s("SonifiQ", "SonifiQ");
    int def = qMax(1, QThread::idealThreadCount() / 2);
    int v = s.value("threads/count", def).toInt();
    return qBound(1, v, QThread::idealThreadCount());
}

void SettingsDialog::saveSettings() {
    QSettings s("SonifiQ", "SonifiQ");
    for (auto &f : m_formats) {
        if (m_checkboxes.contains(f.id))
            f.enabled = m_checkboxes[f.id]->isChecked();
        s.setValue("format_enabled/"   + f.id, f.enabled);
        s.setValue("format_available/" + f.id, f.available);
    }
    if (m_threadsSpin)
        s.setValue("threads/count", m_threadsSpin->value());
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void SettingsDialog::setupUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 12);

    m_tabs = new QTabWidget();

    // ── Tab: Appearance ──────────────────────────────────────────────────────
    auto *appearanceWidget = new QWidget();
    auto *appearanceLayout = new QVBoxLayout(appearanceWidget);
    appearanceLayout->setContentsMargins(16, 16, 16, 8);
    appearanceLayout->setSpacing(10);

    auto *appearanceIntro = new QLabel(
        "Choose a colour theme. Only colours change — layout and icons stay the same.");
    appearanceIntro->setWordWrap(true);
    appearanceIntro->setObjectName("introLabel");
    appearanceLayout->addWidget(appearanceIntro);

    m_themingEnabledCheck = new QCheckBox("Use custom theme (beta)");
    m_themingEnabledCheck->setToolTip(
        "Unchecked: SonifiQ follows your system's default look "
        "(whatever light/dark appearance your desktop normally gives a Qt app), "
        "instead of the colour theme picked below.");
    m_themingEnabledCheck->setChecked(ThemeManager::themingEnabled());
    appearanceLayout->addWidget(m_themingEnabledCheck);

    auto *themeRow = new QHBoxLayout();
    auto *themeLabel = new QLabel("Theme:");
    m_themeCombo = new QComboBox();
    for (const Theme &t : ThemeManager::allThemes())
        m_themeCombo->addItem(t.name, t.id);
    int curIdx = m_themeCombo->findData(ThemeManager::currentThemeId());
    m_themeCombo->setCurrentIndex(curIdx >= 0 ? curIdx : 0);
    m_themeCombo->setEnabled(m_themingEnabledCheck->isChecked());
    themeRow->addWidget(themeLabel);
    themeRow->addWidget(m_themeCombo);
    themeRow->addStretch();
    appearanceLayout->addLayout(themeRow);

    // Themes are individual JSON files on disk — surface the folder so
    // users can edit a built-in theme or drop in their own.
    auto *btnOpenThemesDir = new QPushButton("Open themes folder");
    btnOpenThemesDir->setObjectName("linkButton");
    btnOpenThemesDir->setFlat(true);
    btnOpenThemesDir->setCursor(Qt::PointingHandCursor);
    QFont linkFont = btnOpenThemesDir->font();
    linkFont.setUnderline(true);
    btnOpenThemesDir->setFont(linkFont);
    btnOpenThemesDir->setToolTip(
        "Each theme is a .json file here. Edit one or add your own — "
        "new themes appear next time this dialog opens.");

    // Lets a user get back any built-in theme (Dark, Light, ...) whose .json
    // file was deleted from the themes folder, without touching anything
    // else in there (custom themes, or built-ins that were only edited).
    auto *btnRestoreThemes = new QPushButton("Restore Default Themes");
    btnRestoreThemes->setObjectName("btnSecondary");
    btnRestoreThemes->setToolTip(
        "Recreates the .json file for any built-in theme that's been "
        "deleted from the themes folder. Edited or custom themes are left untouched.");

    auto *themesDirRow = new QHBoxLayout();
    themesDirRow->addWidget(btnOpenThemesDir);
    themesDirRow->addStretch();
    themesDirRow->addWidget(btnRestoreThemes);
    appearanceLayout->addLayout(themesDirRow);
    appearanceLayout->addStretch();

    connect(btnOpenThemesDir, &QPushButton::clicked, this, [] {
        QDir().mkpath(ThemeManager::themesDirPath()); // no-op if it already exists
        QDesktopServices::openUrl(QUrl::fromLocalFile(ThemeManager::themesDirPath()));
    });

    connect(btnRestoreThemes, &QPushButton::clicked, this, &SettingsDialog::restoreDefaultThemes);

    connect(m_themingEnabledCheck, &QCheckBox::toggled, this, [this](bool checked) {
        ThemeManager::setThemingEnabled(checked);
        m_themeCombo->setEnabled(checked);
    });

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        ThemeManager::applyTheme(m_themeCombo->itemData(idx).toString());
    });

    m_tabs->addTab(appearanceWidget, "Appearance");

    // ── Tab: Output Formats ───────────────────────────────────────────────────
    auto *fmtWidget = new QWidget();
    auto *fmtLayout = new QVBoxLayout(fmtWidget);
    fmtLayout->setContentsMargins(16, 16, 16, 8);
    fmtLayout->setSpacing(10);

    auto *introLabel = new QLabel(
        "Enable or disable output formats. Use <b>Check Codec Availability</b> "
        "to verify which codecs are installed in your ffmpeg build.");
    introLabel->setWordWrap(true);
    introLabel->setObjectName("introLabel");
    fmtLayout->addWidget(introLabel);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    grid->setColumnMinimumWidth(0, 24);  // checkbox
    grid->setColumnMinimumWidth(1, 180); // name
    grid->setColumnMinimumWidth(2, 130); // codec
    grid->setColumnMinimumWidth(3, 100); // status

    // Header row
    auto makeHdr = [](const QString &t) {
        auto *l = new QLabel(t);
        l->setObjectName("tableHdr");
        return l;
    };
    grid->addWidget(makeHdr(""),       0, 0);
    grid->addWidget(makeHdr("Format"), 0, 1);
    grid->addWidget(makeHdr("Codec"),  0, 2);
    grid->addWidget(makeHdr("Status"), 0, 3);

    // Format rows
    for (int i = 0; i < m_formats.size(); ++i) {
        FormatEntry &f = m_formats[i];
        int row = i + 1;

        auto *cb = new QCheckBox();
        cb->setChecked(f.enabled);
        cb->setObjectName("fmtCheck");
        m_checkboxes[f.id] = cb;
        grid->addWidget(cb, row, 0);

        auto *nameLabel = new QLabel(f.label);
        grid->addWidget(nameLabel, row, 1);

        auto *codecLabel = new QLabel("<tt>" + f.codec + "</tt>");
        codecLabel->setObjectName("codecLabel");
        grid->addWidget(codecLabel, row, 2);

        auto *statusLabel = new QLabel(f.available ? "✓ Available" : "— Not checked");
        statusLabel->setObjectName(f.available ? "statusOk" : "statusUnknown");
        m_statusLabels[f.id] = statusLabel;
        grid->addWidget(statusLabel, row, 3);
    }

    fmtLayout->addLayout(grid);
    fmtLayout->addSpacing(8);

    // Precheck row
    auto *precheckRow = new QHBoxLayout();
    m_btnCheck = new QPushButton("Check Codec Availability");
    m_btnCheck->setObjectName("btnPrimary");
    m_checkStatus = new QLabel("");
    m_checkStatus->setObjectName("checkStatus");
    precheckRow->addWidget(m_btnCheck);
    precheckRow->addSpacing(12);
    precheckRow->addWidget(m_checkStatus);
    precheckRow->addStretch();
    fmtLayout->addLayout(precheckRow);
    fmtLayout->addStretch();

    m_tabs->addTab(fmtWidget, "Output Formats");

    // ── Tab: Performance ──────────────────────────────────────────────────────
    auto *perfWidget = new QWidget();
    auto *perfLayout = new QVBoxLayout(perfWidget);
    perfLayout->setContentsMargins(16, 16, 16, 8);
    perfLayout->setSpacing(10);

    auto *perfIntro = new QLabel(
        "Number of files converted in parallel. Higher values use more CPU "
        "and disk I/O at once.");
    perfIntro->setWordWrap(true);
    perfIntro->setObjectName("introLabel");
    perfLayout->addWidget(perfIntro);

    auto *threadsRow = new QHBoxLayout();
    auto *threadsLabel = new QLabel("Parallel Threads:");
    m_threadsSpin = new QSpinBox();
    m_threadsSpin->setRange(1, QThread::idealThreadCount());
    m_threadsSpin->setValue(SettingsDialog::savedThreadCount());
    m_threadsSpin->setSuffix(QString("  / %1").arg(QThread::idealThreadCount()));
    threadsRow->addWidget(threadsLabel);
    threadsRow->addWidget(m_threadsSpin);
    threadsRow->addStretch();
    perfLayout->addLayout(threadsRow);
    perfLayout->addStretch();

    m_tabs->addTab(perfWidget, "Performance");

    root->addWidget(m_tabs, 1);

    // Dialog buttons
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->setContentsMargins(16, 0, 16, 0);
    root->addWidget(btns);

    connect(m_btnCheck, &QPushButton::clicked, this, &SettingsDialog::runPrecheck);
    connect(btns, &QDialogButtonBox::accepted, this, [this]{ saveSettings(); accept(); });
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ── Codec Precheck ────────────────────────────────────────────────────────────

// Shared by the interactive "Check Codec Availability" button and the
// automatic startup check — runs `ffmpeg -encoders` once and reports which
// of the given formats' codecs are present in the encoder list.
static QMap<QString, bool> probeCodecAvailability(const QList<FormatEntry> &formats) {
    QMap<QString, bool> result;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("ffmpeg", {"-encoders", "-v", "quiet"});
    proc.waitForFinished(8000);
    QString encoderList = QString::fromLocal8Bit(proc.readAllStandardOutput());
    for (const auto &f : formats)
        result[f.id] = encoderList.contains(f.codec);
    return result;
}

// Headless equivalent of runPrecheck() — probes codecs and saves the
// results straight to QSettings, with no dialog/UI involved. Called once
// at app startup so availability data is always fresh.
void SettingsDialog::probeAndSaveCodecs() {
    const QList<FormatEntry> formats = allFormats();
    const QMap<QString, bool> availability = probeCodecAvailability(formats);
    QSettings s("SonifiQ", "SonifiQ");
    for (const auto &f : formats)
        s.setValue("format_available/" + f.id, availability.value(f.id, false));
}

void SettingsDialog::runPrecheck() {
    m_btnCheck->setEnabled(false);
    m_checkStatus->setText("Checking…");
    QApplication::processEvents();

    const QMap<QString, bool> availability = probeCodecAvailability(m_formats);

    int found = 0;
    for (auto &f : m_formats) {
        bool avail = availability.value(f.id, false);
        f.available = avail;

        if (m_statusLabels.contains(f.id)) {
            auto *lbl = m_statusLabels[f.id];
            if (avail) {
                lbl->setText("✓ Available");
                lbl->setObjectName("statusOk");
            } else {
                lbl->setText("✗ Not found");
                lbl->setObjectName("statusErr");
            }
            lbl->update();
        }

        // Auto-disable checkbox if codec is missing; re-enable if it's found
        if (m_checkboxes.contains(f.id)) {
            auto *cb = m_checkboxes[f.id];
            cb->setEnabled(avail);
            if (!avail) cb->setChecked(false);
        }

        if (avail) ++found;
    }

    m_checkStatus->setText(QString("%1/%2 codecs found").arg(found).arg(m_formats.size()));
    m_btnCheck->setEnabled(true);

    // Persist availability results immediately
    QSettings s("SonifiQ", "SonifiQ");
    for (const auto &f : m_formats)
        s.setValue("format_available/" + f.id, f.available);
}

// Recreates any built-in theme .json file that's missing from the themes
// folder (e.g. the user deleted "dark.json"). Never touches files that
// already exist, so edited built-ins and custom themes are left alone.
void SettingsDialog::restoreDefaultThemes() {
    if (ThemeManager::missingBuiltInThemeIds().isEmpty()) {
        QMessageBox::information(this, "Restore Default Themes",
                                 "All default themes are already present in the themes folder.");
        return;
    }

    const int restored = ThemeManager::restoreMissingBuiltInThemes();

    // Repopulate the picker so the restored theme(s) show up immediately,
    // keeping whatever's currently selected/previewed if it still exists.
    const QString keepId = m_themeCombo->currentData().toString();
    m_themeCombo->blockSignals(true);
    m_themeCombo->clear();
    for (const Theme &t : ThemeManager::allThemes())
        m_themeCombo->addItem(t.name, t.id);
    int idx = m_themeCombo->findData(keepId);
    m_themeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_themeCombo->blockSignals(false);

    QMessageBox::information(this, "Restore Default Themes",
                             restored == 1 ? "Restored 1 default theme."
                                           : QString("Restored %1 default themes.").arg(restored));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QStringList SettingsDialog::enabledFormats() const {
    QStringList r;
    for (const auto &f : m_formats)
        if (f.enabled) r << f.id;
    return r;
}

bool SettingsDialog::isFormatEnabled(const QString &id) const {
    for (const auto &f : m_formats)
        if (f.id == id) return f.enabled;
    return true;
}