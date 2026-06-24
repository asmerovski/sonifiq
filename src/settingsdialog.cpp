#include "settingsdialog.h"
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

// ── Static format registry ────────────────────────────────────────────────────

QList<FormatEntry> SettingsDialog::allFormats() {
    return {
        { "mp3",  "MP3",                "libmp3lame",  false, true },
        { "m4a",  "AAC (.m4a)",         "aac",         false, true },
        { "ogg",  "Ogg Vorbis (.ogg)",  "libvorbis",   false, true },
        { "opus", "Opus (.opus)",        "libopus",     false, true },
        { "flac", "FLAC",               "flac",        false, true },
        { "wav",  "WAV (PCM)",          "pcm_s16le",   false, true },
        { "aiff", "AIFF",               "pcm_s16be",   false, true },
    };
}

// ── Constructor ───────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("SonifiQ — Settings");
    setMinimumWidth(520);
    m_formats = allFormats();
    loadSettings();
    setupUI();
}

// ── Load / Save ───────────────────────────────────────────────────────────────

void SettingsDialog::loadSettings() {
    QSettings s("SonifiQ", "SonifiQ");
    for (auto &f : m_formats) {
        f.enabled = s.value("format_enabled/" + f.id, true).toBool();
        f.available = s.value("format_available/" + f.id, false).toBool();
    }
}

void SettingsDialog::saveSettings() {
    QSettings s("SonifiQ", "SonifiQ");
    for (auto &f : m_formats) {
        if (m_checkboxes.contains(f.id))
            f.enabled = m_checkboxes[f.id]->isChecked();
        s.setValue("format_enabled/"   + f.id, f.enabled);
        s.setValue("format_available/" + f.id, f.available);
    }
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void SettingsDialog::setupUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 12);

    m_tabs = new QTabWidget();

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

void SettingsDialog::runPrecheck() {
    m_btnCheck->setEnabled(false);
    m_checkStatus->setText("Checking…");
    QApplication::processEvents();

    // Ask ffmpeg to list all encoders, then grep for each codec
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("ffmpeg", {"-encoders", "-v", "quiet"});
    proc.waitForFinished(8000);
    QString encoderList = QString::fromLocal8Bit(proc.readAllStandardOutput());

    int found = 0;
    for (auto &f : m_formats) {
        // A codec line looks like: " A..... libmp3lame ..."
        bool avail = encoderList.contains(f.codec);
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
            // Force stylesheet refresh
            lbl->update();
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

