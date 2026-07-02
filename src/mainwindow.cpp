#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QMimeData>
#include <QUrl>
#include <QProcess>
#include <QThread>
#include <QSettings>
#include <QTimer>
#include <QScrollBar>
#include <QDialog>
#include <QMenu>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QToolTip>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QPainter>
#include <QPainterPath>
#include <utility>

// ── Audio extensions accepted as input ───────────────────────────────────────
static const QStringList &audioExtensions() {
    static const QStringList list = {
        "flac","mp3","mp2","ogg","opus","wav","aiff","aif","m4a","aac",
        "wma","ape","wv","mka","tta","ac3","caf","dts"
    };
    return list;
}

// ── Segmented total-progress bar ──────────────────────────────────────────────
// Shows successful / failed / skipped counts as distinct coloured segments
// (rather than a single uniform fill) plus an "x/y" label overlay.
class SegmentedProgressBar : public QWidget {
public:
    explicit SegmentedProgressBar(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(22);
    }

    // Sets the total job count and clears all segment counts (blank bar).
    void reset(int total) {
        m_total = total;
        m_success = m_failed = m_skipped = 0;
        update();
    }

    // Updates the coloured segments. `total` is the denominator (usually the
    // number of jobs in the current run).
    void setCounts(int total, int success, int failed, int skipped) {
        m_total   = total;
        m_success = success;
        m_failed  = failed;
        m_skipped = skipped;
        update();
    }

    QSize sizeHint() const override { return QSize(280, 22); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QRectF r = rect().adjusted(0, 0, -1, -1);
        const double radius = 4.0;

        QPainterPath clipPath;
        clipPath.addRoundedRect(r, radius, radius);

        // Background track
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60, 64, 76));
        p.drawPath(clipPath);

        if (m_total > 0) {
            p.setClipPath(clipPath);
            double w = r.width();
            double x = r.left();
            auto drawSeg = [&](int count, const QColor &color) {
                if (count <= 0) return;
                double segW = w * (double(count) / double(m_total));
                p.fillRect(QRectF(x, r.top(), segW, r.height()), color);
                x += segW;
            };
            drawSeg(m_success, QColor( 76, 175, 125));
            drawSeg(m_failed,  QColor(224,  92,  92));
            drawSeg(m_skipped, QColor(200, 150,  60));
            p.setClipping(false);
        }

        p.setPen(QColor(235, 235, 235));
        QString text = m_total > 0
                           ? QString("%1/%2").arg(m_success + m_failed + m_skipped).arg(m_total)
                           : QString("0/0");
        p.drawText(rect(), Qt::AlignCenter, text);
    }

private:
    int m_total = 0, m_success = 0, m_failed = 0, m_skipped = 0;
};

// ── Output format definitions (mirrors SettingsDialog::allFormats order) ─────
struct FormatDef {
    QString     id;
    QString     label;
    QString     ext;
    QStringList qualities;
    QStringList qualityArgs;  // each entry is a single ffmpeg arg token
    QString     codecArg;     // -codec:a value
    bool        supportsCover;// can this container carry cover art?
};

static const QList<FormatDef> &allFormatDefs() {
    static const QList<FormatDef> list = {
                                           { "mp3",  "MP3",           "mp3",
                                            {"320 kbps CBR","256 kbps CBR","192 kbps CBR","128 kbps CBR",
                                             "V0 (VBR ~245)","V2 (VBR ~190)","V4 (VBR ~165)"},
                                            {"-b:a\n320k", "-b:a\n256k", "-b:a\n192k", "-b:a\n128k",
                                             "-q:a\n0",    "-q:a\n2",    "-q:a\n4"},
                                            "libmp3lame", true },
                                           { "m4a",  "AAC (.m4a)",    "m4a",
                                            {"320 kbps","256 kbps","192 kbps","128 kbps"},
                                            {"-b:a\n320k","-b:a\n256k","-b:a\n192k","-b:a\n128k"},
                                            "aac", false },   // m4a: cover art requires -codec:v copy; omit for reliability
                                           { "ogg",  "Ogg Vorbis",    "ogg",
                                            {"Quality 10 (~500k)","Quality 8 (~256k)","Quality 6 (~192k)",
                                             "Quality 4 (~128k)","Quality 2 (~96k)"},
                                            {"-q:a\n10","-q:a\n8","-q:a\n6","-q:a\n4","-q:a\n2"},
                                            "libvorbis", false },
                                           { "opus", "Opus",          "opus",
                                            {"320 kbps","256 kbps","192 kbps","128 kbps","96 kbps","64 kbps"},
                                            {"-b:a\n320k","-b:a\n256k","-b:a\n192k","-b:a\n128k","-b:a\n96k","-b:a\n64k"},
                                            "libopus", false },
                                           { "flac", "FLAC",          "flac",
                                            {"Level 8 (best)","Level 5 (default)","Level 0 (fast)"},
                                            {"-compression_level\n8","-compression_level\n5","-compression_level\n0"},
                                            "flac", true },
                                           { "wav",  "WAV (PCM)",     "wav",
                                            {"16-bit","24-bit","32-bit float"},
                                            {"-acodec\npcm_s16le","-acodec\npcm_s24le","-acodec\npcm_f32le"},
                                            "", false },          // WAV: codec is embedded in qualityArg itself
                                           { "aiff", "AIFF",          "aiff",
                                            {"16-bit","24-bit"},
                                            {"-acodec\npcm_s16be","-acodec\npcm_s24be"},
                                            "", false },
                                           { "wv",   "WavPack",       "wv",
                                            {"High (q=4)","Default (q=3)","Fast (q=1)"},
                                            {"-compression_level\n4","-compression_level\n3","-compression_level\n1"},
                                            "wavpack", false },
                                           { "mp2",  "MP2",           "mp2",
                                            {"384 kbps","320 kbps","256 kbps","192 kbps"},
                                            {"-b:a\n384k","-b:a\n320k","-b:a\n256k","-b:a\n192k"},
                                            "mp2", false },
                                           { "ac3",  "AC3 (Dolby Digital)", "ac3",
                                            {"640 kbps","448 kbps","384 kbps","256 kbps"},
                                            {"-b:a\n640k","-b:a\n448k","-b:a\n384k","-b:a\n256k"},
                                            "ac3", false },
                                           { "mka",  "MKA (Matroska)", "mka",
                                            {"Copy (remux only)"},
                                            {"-codec:a\ncopy"},
                                            "", false },
                                           { "caf",  "CAF (Apple Core Audio)", "caf",
                                            {"16-bit","24-bit","32-bit float"},
                                            {"-acodec\npcm_s16be","-acodec\npcm_s24be","-acodec\npcm_f32le"},
                                            "", false },
                                           { "alac", "ALAC (Apple Lossless)", "m4a",
                                            {"Default"},
                                            {""},
                                            "alac", true },
                                           };
    return list;
}

// Returns only the formats the user has enabled in Settings
static QList<FormatDef> enabledFormats() {
    QSettings s("SonifiQ", "SonifiQ");
    QList<FormatDef> r;
    for (const auto &f : allFormatDefs()) {
        if (s.value("format_enabled/" + f.id, true).toBool())
            r << f;
    }
    if (r.isEmpty()) return allFormatDefs(); // safety fallback
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ConversionWorker::run
// ═══════════════════════════════════════════════════════════════════════════════
void ConversionWorker::run() {
    if (m_cancel->loadAcquire()) {
        emit m_relay->jobFinished(m_job.row, false, "Cancelled");
        return;
    }

    const QString fileName = QFileInfo(m_job.inputPath).fileName();

    // Source and destination resolve to the same file — converting would
    // overwrite the source, so skip it entirely instead of running ffmpeg.
    // This check always applies, regardless of the overwrite setting.
    if (QFileInfo(m_job.inputPath).absoluteFilePath() ==
        QFileInfo(m_job.outputPath).absoluteFilePath()) {
        QString reason = "Source and destination file are identical — conversion skipped.";
        emit m_relay->logLine(m_job.row, fileName, "Skipped: " + reason);
        emit m_relay->jobFinished(m_job.row, false, "Skipped:" + reason);
        return;
    }

    // Destination file already exists and overwrite is disabled — skip it
    // rather than silently clobbering an existing file.
    if (!m_job.overwrite && QFileInfo::exists(m_job.outputPath)) {
        QString reason = "Destination file already exists and overwrite is disabled.";
        emit m_relay->logLine(m_job.row, fileName, "Skipped: " + reason);
        emit m_relay->jobFinished(m_job.row, false, "Skipped:" + reason);
        return;
    }

    // ── Step 1: probe duration (microseconds) ────────────────────────────────
    qint64 durationUs = 0;
    {
        QProcess probe;
        probe.setProcessChannelMode(QProcess::SeparateChannels);
        probe.start("ffprobe", {
                                   "-v", "error",
                                   "-show_entries", "format=duration",
                                   "-of", "default=noprint_wrappers=1:nokey=1",
                                   m_job.inputPath
                               });
        if (probe.waitForFinished(10000)) {
            bool ok;
            double secs = QString::fromLocal8Bit(
                              probe.readAllStandardOutput()).trimmed().toDouble(&ok);
            if (ok && secs > 0.0)
                durationUs = static_cast<qint64>(secs * 1000000.0);
        }
    }

    emit m_relay->progressChanged(m_job.row, 0);

    m_stderrBuf.clear();

    // ── Step 2: run ffmpeg ────────────────────────────────────────────────────
    // -progress pipe:1  → key=value progress lines on stdout
    // -stats_period 0.2 → 5 updates/sec (ffmpeg ≥ 5; silently ignored on older)
    // stderr kept separate for error capture
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);

    QStringList ffArgs = m_args;
    QString outFile = ffArgs.takeLast();  // m_args always ends with output path
    ffArgs << "-progress" << "pipe:1"
           << "-stats_period" << "0.2"
           << outFile;

    proc.start("ffmpeg", ffArgs);
    if (!proc.waitForStarted(5000)) {
        emit m_relay->jobFinished(m_job.row, false, "ffmpeg not found");
        return;
    }

    // ── Read progress lines incrementally ────────────────────────────────────
    // Use waitForReadyRead() so we block until data is actually available
    // instead of polling on a fixed timer — this is why the bar was jumping.
    QByteArray buf;
    while (proc.state() != QProcess::NotRunning) {
        if (m_cancel->loadAcquire()) {
            proc.kill();
            proc.waitForFinished(3000);
            emit m_relay->jobFinished(m_job.row, false, "Cancelled");
            return;
        }
        proc.waitForReadyRead(100);
        buf += proc.readAllStandardOutput();

        // Emit each stderr line individually for the log pane
        m_stderrBuf += proc.readAllStandardError();
        {
            int nl;
            while ((nl = m_stderrBuf.indexOf('\n')) != -1) {
                QString line = QString::fromLocal8Bit(
                                   m_stderrBuf.left(nl)).trimmed();
                m_stderrBuf.remove(0, nl + 1);
                if (!line.isEmpty())
                    emit m_relay->logLine(m_job.row,
                                          QFileInfo(m_job.inputPath).fileName(), line);
            }
        }

        if (durationUs > 0) {
            int nl;
            while ((nl = buf.indexOf('\n')) != -1) {
                QByteArray line = buf.left(nl).trimmed();
                buf.remove(0, nl + 1);
                if (line.startsWith("out_time_us=")) {
                    bool ok;
                    qint64 us = line.mid(12).toLongLong(&ok);
                    if (ok && us > 0) {
                        int pct = static_cast<int>(
                            qBound(0LL, us * 100LL / durationUs, 99LL));
                        emit m_relay->progressChanged(m_job.row, pct);
                    }
                }
            }
        }
    }

    // Drain any remaining output after process exits
    buf += proc.readAllStandardOutput();
    if (durationUs > 0) {
        int nl;
        while ((nl = buf.indexOf('\n')) != -1) {
            QByteArray line = buf.left(nl).trimmed();
            buf.remove(0, nl + 1);
            if (line.startsWith("out_time_us=")) {
                bool ok;
                qint64 us = line.mid(12).toLongLong(&ok);
                if (ok && us > 0) {
                    int pct = static_cast<int>(qBound(0LL, us * 100LL / durationUs, 99LL));
                    emit m_relay->progressChanged(m_job.row, pct);
                }
            }
        }
    }

    // Drain any remaining stderr after process exits
    m_stderrBuf += proc.readAllStandardError();
    {
        int nl;
        while ((nl = m_stderrBuf.indexOf('\n')) != -1) {
            QString line = QString::fromLocal8Bit(
                               m_stderrBuf.left(nl)).trimmed();
            m_stderrBuf.remove(0, nl + 1);
            if (!line.isEmpty())
                emit m_relay->logLine(m_job.row,
                                      QFileInfo(m_job.inputPath).fileName(), line);
        }
        // Flush any final partial line without newline
        if (!m_stderrBuf.trimmed().isEmpty())
            emit m_relay->logLine(m_job.row,
                                  QFileInfo(m_job.inputPath).fileName(),
                                  QString::fromLocal8Bit(m_stderrBuf.trimmed()));
    }

    // Extract the most useful line from stderr: last non-empty line that starts
    // with a capital letter (ffmpeg error lines look like "Error opening output...")
    auto extractError = [](const QByteArray &raw) -> QString {
        QString msg;
        const auto lines = QString::fromLocal8Bit(raw).split('\n');
        for (int i = lines.size() - 1; i >= 0; --i) {
            const QString line = lines[i].trimmed();
            if (!line.isEmpty() && line[0].isUpper() && !line.startsWith("Stream")
                && !line.startsWith("Output #") && !line.startsWith("Input #")
                && !line.startsWith("Press")) {
                msg = line;
                break;
            }
        }
        // Fallback: last non-empty line
        if (msg.isEmpty()) {
            for (int i = lines.size() - 1; i >= 0; --i) {
                if (!lines[i].trimmed().isEmpty()) { msg = lines[i].trimmed(); break; }
            }
        }
        // Truncate for display
        if (msg.length() > 120) msg = msg.left(117) + "…";
        return msg;
    };

    int exitCode = proc.exitCode();

    // ffmpeg sometimes exits 0 but still failed (e.g. "Output same as Input")
    // Detect this by checking stderr for known fatal patterns
    bool ffmpegFailed = (exitCode != 0);
    if (!ffmpegFailed && !m_stderrBuf.isEmpty()) {
        const QString errText = QString::fromLocal8Bit(m_stderrBuf);
        if (errText.contains("same as Input") ||
            errText.contains("Error opening output") ||
            errText.contains("Invalid argument") ||
            errText.contains("No such file or directory"))
            ffmpegFailed = true;
    }

    if (!ffmpegFailed) {
        emit m_relay->progressChanged(m_job.row, 100);
        emit m_relay->jobFinished(m_job.row, true, {});
    } else {
        emit m_relay->jobFinished(m_job.row, false, extractError(m_stderrBuf));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// MainWindow
// ═══════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SonifiQ");
    setMinimumSize(920, 640);
    resize(1140, 720);
    setAcceptDrops(true);
    setupUI();
}

MainWindow::~MainWindow() {
    m_cancelFlag.storeRelease(1);
    if (m_pool) m_pool->waitForDone(3000);
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void MainWindow::setupUI() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Left sidebar ──────────────────────────────────────────────────────────
    auto *sidebar = new QScrollArea();
    sidebar->setWidgetResizable(true);
    sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebar->setFixedWidth(220);
    sidebar->setFrameShape(QFrame::NoFrame);

    auto *sideWidget = new QWidget();
    auto *sideOuterLayout = new QVBoxLayout(sideWidget);
    sideOuterLayout->setContentsMargins(10, 12, 10, 12);
    sideOuterLayout->setSpacing(0);

    auto *sideGroup = new QGroupBox("Conversion Settings");
    auto *sideLayout = new QVBoxLayout(sideGroup);
    sideLayout->setContentsMargins(12, 14, 12, 14);
    sideLayout->setSpacing(6);

    auto addSideLabel = [&](const QString &text) {
        auto *lbl = new QLabel(text);
        sideLayout->addSpacing(10);
        sideLayout->addWidget(lbl);
    };

    addSideLabel("Output Format");
    m_formatCombo = new QComboBox();
    m_formatCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sideLayout->addWidget(m_formatCombo);

    addSideLabel("Quality / Bitrate");
    m_qualityCombo = new QComboBox();
    m_qualityCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sideLayout->addWidget(m_qualityCombo);
    rebuildFormatCombo(); // called after both combos exist

    addSideLabel("Channels");
    m_channelsCombo = new QComboBox();
    m_channelsCombo->addItems({"Source (keep)", "Stereo (2ch)", "Mono (1ch)"});
    m_channelsCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sideLayout->addWidget(m_channelsCombo);

    addSideLabel("Sample Rate");
    m_samplerateCombo = new QComboBox();
    m_samplerateCombo->addItems({"Source (keep)", "48000 Hz", "44100 Hz", "32000 Hz", "22050 Hz"});
    m_samplerateCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sideLayout->addWidget(m_samplerateCombo);

    sideLayout->addSpacing(16);
    m_keepTags = new QCheckBox("Preserve tags");
    m_keepTags->setChecked(true);
    sideLayout->addWidget(m_keepTags);

    m_keepCover = new QCheckBox("Preserve cover art");
    m_keepCover->setChecked(true);
    sideLayout->addWidget(m_keepCover);

    sideLayout->addStretch();
    sideOuterLayout->addWidget(sideGroup);
    sideOuterLayout->addStretch();
    sidebar->setWidget(sideWidget);
    rootLayout->addWidget(sidebar);

    // ── Right content area ────────────────────────────────────────────────────
    auto *contentWidget = new QWidget();
    auto *root = new QVBoxLayout(contentWidget);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);
    rootLayout->addWidget(contentWidget, 1);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);
    m_btnAddFiles  = new QPushButton("+ Add Files");
    m_btnAddFolder = new QPushButton("+ Add Folder");
    m_btnRemove    = new QPushButton("Remove");
    m_btnClear     = new QPushButton("Clear All");
    m_btnSettings  = new QPushButton("⚙ Settings");
    m_btnAddFiles->setObjectName("btnPrimary");
    m_btnAddFolder->setObjectName("btnPrimary");
    m_btnRemove->setObjectName("btnSecondary");
    m_btnClear->setObjectName("btnSecondary");
    m_btnSettings->setObjectName("btnSecondary");
    m_btnToggleLog = new QPushButton("Log");
    m_btnToggleLog->setObjectName("btnSecondary");
    m_btnToggleLog->setCheckable(true);
    m_btnToggleLog->setChecked(false);
    m_btnToggleLog->setToolTip("Show / hide ffmpeg output log");
    toolbar->addWidget(m_btnAddFiles);
    toolbar->addWidget(m_btnAddFolder);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_btnRemove);
    toolbar->addWidget(m_btnClear);
    toolbar->addStretch();
    toolbar->addWidget(m_btnToggleLog);
    toolbar->addWidget(m_btnSettings);
    root->addLayout(toolbar);

    // ── File table: File | Source Dir | Format | Duration | Progress | Status ──
    m_fileTable = new QTableWidget(0, 6);
    m_fileTable->setHorizontalHeaderLabels(
        {"File", "Source Dir", "Format", "Duration", "Progress", "Status"});
    m_fileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setStretchLastSection(false);
    m_fileTable->setColumnWidth(0, 340);
    m_fileTable->setColumnWidth(1, 220);
    m_fileTable->setColumnWidth(2,  72);
    m_fileTable->setColumnWidth(3,  82);
    m_fileTable->setColumnWidth(4, 175);
    m_fileTable->setColumnWidth(5, 110);
    m_fileTable->horizontalHeader()->setMinimumSectionSize(40);

    // ── Header alignment: centered except File (0) and Source Dir (1) ─────────
    for (int c = 0; c < m_fileTable->columnCount(); ++c) {
        if (c == 0 || c == 1) continue;
        if (auto *h = m_fileTable->horizontalHeaderItem(c))
            h->setTextAlignment(Qt::AlignCenter);
    }
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->setShowGrid(false);
    m_fileTable->verticalHeader()->setDefaultSectionSize(34);

    // ── Sorting: all columns except Format (col 1) ────────────────────────────
    m_fileTable->setSortingEnabled(false); // manual sort via header click
    m_fileTable->horizontalHeader()->setSectionsClickable(true);
    m_fileTable->horizontalHeader()->setSortIndicatorShown(true);
    m_fileTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

    // ── Right-click context menu ──────────────────────────────────────────────
    m_fileTable->setContextMenuPolicy(Qt::CustomContextMenu);

    m_emptyLabel = new QLabel(
        "List is empty.\nAdd files or folders, or drop them here.");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setEnabled(false); // muted appearance via palette

    m_tableStack = new QStackedWidget();
    m_tableStack->addWidget(m_emptyLabel); // index 0 — empty state
    m_tableStack->addWidget(m_fileTable);  // index 1 — table
    m_tableStack->setCurrentIndex(0);
    root->addWidget(m_tableStack, 1);

    // ── Stats line (successful / failed / not processed) ──────────────────────
    m_statsLabel = new QLabel();
    m_statsLabel->setObjectName("statsLabel");
    m_statsLabel->setTextFormat(Qt::RichText);
    m_statsLabel->setStyleSheet(
        "QLabel#statsLabel {"
        "  font-size: 13px; font-weight: 600;"
        "  padding: 6px 10px;"
        "  background-color: rgba(127,127,127,18);"
        "  border-radius: 5px;"
        "}");
    root->addWidget(m_statsLabel);

    // ── Output destination ────────────────────────────────────────────────────
    auto *destGroup = new QGroupBox("Destination");
    auto *destGroupLayout = new QVBoxLayout(destGroup);
    destGroupLayout->setSpacing(8);

    auto *destLayout = new QHBoxLayout();
    m_sameDir = new QCheckBox("Save next to source files");
    m_sameDir->setChecked(true);
    destLayout->addWidget(m_sameDir);
    destLayout->addSpacing(12);
    auto *folderLabel = new QLabel("Output folder:");
    destLayout->addWidget(folderLabel);
    m_outputDirEdit = new QLineEdit();
    m_outputDirEdit->setPlaceholderText("Select output directory…");
    m_outputDirEdit->setEnabled(false);
    destLayout->addWidget(m_outputDirEdit, 1);
    m_btnBrowse = new QPushButton("Browse");
    m_btnBrowse->setObjectName("btnSecondary");
    m_btnBrowse->setEnabled(false);
    destLayout->addWidget(m_btnBrowse);
    destGroupLayout->addLayout(destLayout);

    m_overwriteFiles = new QCheckBox(
        "Overwrite existing files at destination "
        "(files with identical source/destination path are always skipped)");
    destGroupLayout->addWidget(m_overwriteFiles);

    root->addWidget(destGroup);

    // ── Bottom bar ────────────────────────────────────────────────────────────
    auto *bottom = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setObjectName("statusLabel");
    m_totalProgress = new SegmentedProgressBar();
    m_totalProgress->setMinimumWidth(280);
    m_totalProgress->reset(0);
    m_btnStart  = new QPushButton("Convert");
    m_btnCancel = new QPushButton("Cancel");
    m_btnStart->setObjectName("btnConvert");
    m_btnCancel->setObjectName("btnSecondary");
    m_btnCancel->setEnabled(false);
    bottom->addWidget(m_statusLabel);
    bottom->addStretch();
    bottom->addWidget(m_totalProgress);
    bottom->addSpacing(16);
    bottom->addWidget(m_btnStart);
    bottom->addWidget(m_btnCancel);
    root->addLayout(bottom);

    // ── Log dock widget ───────────────────────────────────────────────────────
    m_logDock = new QDockWidget("ffmpeg Log", this);
    m_logDock->setObjectName("logDock");
    // Locked to the bottom — not floatable, and not movable since there's no
    // other valid area to drag it into (issue #17).
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_logDock->setFeatures(QDockWidget::DockWidgetClosable);

    QWidget *logContainer = new QWidget();
    QVBoxLayout *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(4, 4, 4, 4);
    logLayout->setSpacing(4);

    // Log toolbar
    QHBoxLayout *logToolbar = new QHBoxLayout();
    QLabel *logTitle = new QLabel("ffmpeg output");
    logTitle->setObjectName("logTitle");
    auto *btnClearLog = new QPushButton("Clear");
    btnClearLog->setObjectName("btnSecondary");
    btnClearLog->setFixedHeight(24);
    auto *chkAutoScroll = new QCheckBox("Auto-scroll");
    chkAutoScroll->setChecked(true);
    chkAutoScroll->setObjectName("logCheck");
    logToolbar->addWidget(logTitle);
    logToolbar->addStretch();
    logToolbar->addWidget(chkAutoScroll);
    logToolbar->addWidget(btnClearLog);
    logLayout->addLayout(logToolbar);

    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000); // cap at 5000 lines
    m_logView->setObjectName("logView");
    m_logView->setPlaceholderText("ffmpeg output will appear here during conversion…");
    logLayout->addWidget(m_logView, 1);

    m_logDock->setWidget(logContainer);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();   // hidden by default

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_btnAddFiles,  &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(m_btnAddFolder, &QPushButton::clicked, this, &MainWindow::addFolder);
    connect(m_btnRemove,    &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(m_btnClear,     &QPushButton::clicked, this, &MainWindow::clearAll);
    connect(m_btnStart,     &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(m_btnCancel,    &QPushButton::clicked, this, &MainWindow::cancelConversion);
    connect(m_btnBrowse,    &QPushButton::clicked, this, &MainWindow::browseOutputDir);
    connect(m_btnSettings,  &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(m_formatCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateFormatOptions);
    connect(m_sameDir, &QCheckBox::toggled, this, [this](bool checked) {
        m_outputDirEdit->setEnabled(!checked);
        m_btnBrowse->setEnabled(!checked);
    });
    connect(m_btnToggleLog, &QPushButton::toggled, this, &MainWindow::toggleLog);
    connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool vis) {
        m_btnToggleLog->setChecked(vis);
    });
    connect(btnClearLog, &QPushButton::clicked, this, &MainWindow::clearLog);
    // Auto-scroll: store pointer in log view's property so slot can access it
    connect(chkAutoScroll, &QCheckBox::toggled, this, [this](bool on) {
        m_logView->setProperty("autoScroll", on);
    });
    m_logView->setProperty("autoScroll", true);

    connect(m_fileTable->model(), &QAbstractItemModel::rowsInserted,
            this, &MainWindow::updateListButtons);
    connect(m_fileTable->model(), &QAbstractItemModel::rowsRemoved,
            this, &MainWindow::updateListButtons);
    connect(m_fileTable, &QTableWidget::cellClicked, this, [this](int row, int col) {
        if (col == 5) showLogPopup(row);  // Status column
    });
    connect(m_fileTable->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &MainWindow::onHeaderClicked);
    connect(m_fileTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    updateFormatOptions(0);
    updateListButtons(); // set initial state (list empty → buttons disabled)
}

// ── List-dependent button state ─────────────────────────────────────────────

void MainWindow::updateListButtons() {
    bool hasRows = m_fileTable->rowCount() > 0;
    m_btnRemove->setEnabled(hasRows && !m_running);
    m_btnClear->setEnabled(hasRows && !m_running);
    m_btnStart->setEnabled(hasRows && !m_running);
    m_tableStack->setCurrentIndex(hasRows ? 1 : 0);
    updateStats();
}

// ── Row status counting (shared by stats label and progress bar) ────────────

void MainWindow::computeRowCounts(int &success, int &failed, int &skipped, int &pending) const {
    success = failed = skipped = pending = 0;
    int rows = m_fileTable->rowCount();
    for (int r = 0; r < rows; ++r) {
        auto *it = m_fileTable->item(r, 5);
        QString s = it ? it->text() : QString();
        if      (s.startsWith("✓")) ++success;
        else if (s.startsWith("✗")) ++failed;
        else if (s.startsWith("⏭")) ++skipped;
        else                         ++pending;
    }
}

// ── Statistics: successful / failed / not processed ──────────────────────────

void MainWindow::updateStats() {
    if (!m_statsLabel) return;
    int success, failed, skipped, pending;
    computeRowCounts(success, failed, skipped, pending);
    m_statsLabel->setText(
        QString("<span style='color:#4caf7d;'>✓ %1 successful</span>"
                "&nbsp;&nbsp;&nbsp;&nbsp;"
                "<span style='color:#e05c5c;'>✗ %2 failed</span>"
                "&nbsp;&nbsp;&nbsp;&nbsp;"
                "<span style='color:#c89632;'>⏭ %3 skipped</span>"
                "&nbsp;&nbsp;&nbsp;&nbsp;"
                "<span style='color:#8b92a8;'>○ %4 not processed</span>")
            .arg(success).arg(failed).arg(skipped).arg(pending));

    if (m_totalProgress && !m_cancelFlag.loadAcquire()) {
        int total = m_running ? m_totalJobs : m_fileTable->rowCount();
        m_totalProgress->setCounts(total, success, failed, skipped);
    }
}

// ── Format combo (rebuilt after settings change) ──────────────────────────────

void MainWindow::rebuildFormatCombo() {
    if (!m_formatCombo) return;
    QString cur = m_formatCombo->currentData().toString();
    m_formatCombo->blockSignals(true);
    m_formatCombo->clear();
    const auto fmts = enabledFormats();
    for (const auto &f : fmts)
        m_formatCombo->addItem(f.label, f.id);
    // Restore previous selection if still available
    int idx = m_formatCombo->findData(cur);
    m_formatCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_formatCombo->blockSignals(false);
    updateFormatOptions(m_formatCombo->currentIndex());
}

void MainWindow::updateFormatOptions(int index) {
    if (!m_qualityCombo) return;
    const auto fmts = enabledFormats();
    if (index < 0 || index >= fmts.size()) return;
    m_qualityCombo->clear();
    m_qualityCombo->addItems(fmts[index].qualities);
}

// ── Settings dialog ───────────────────────────────────────────────────────────

void MainWindow::openSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        rebuildFormatCombo();
}

// ── Add single file row (dedup) ───────────────────────────────────────────────

bool MainWindow::addFileRow(const QString &path, QStringList *duplicates) {
    for (int i = 0; i < m_fileTable->rowCount(); ++i)
        if (m_fileTable->item(i, 0)->data(Qt::UserRole).toString() == path) {
            if (duplicates) duplicates->append(QFileInfo(path).fileName());
            return false;
        }

    QFileInfo fi(path);
    int row = m_fileTable->rowCount();
    m_fileTable->insertRow(row);

    auto *nameItem = new QTableWidgetItem(fi.fileName());
    nameItem->setData(Qt::UserRole, path);
    nameItem->setToolTip(path);
    nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_fileTable->setItem(row, 0, nameItem);

    auto *srcDirItem = new QTableWidgetItem(fi.absolutePath());
    srcDirItem->setToolTip(fi.absolutePath());
    srcDirItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_fileTable->setItem(row, 1, srcDirItem);

    auto *fmtItem = new QTableWidgetItem(fi.suffix().toUpper());
    fmtItem->setTextAlignment(Qt::AlignCenter);
    m_fileTable->setItem(row, 2, fmtItem);

    auto *durItem = new QTableWidgetItem("—");
    durItem->setTextAlignment(Qt::AlignCenter);
    m_fileTable->setItem(row, 3, durItem);

    auto *bar = new QProgressBar();
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(true);
    bar->setFormat("%p%");
    bar->setFixedHeight(18);

    auto *barWrapper = new QWidget();
    auto *barLayout  = new QHBoxLayout(barWrapper);
    barLayout->setContentsMargins(4, 0, 4, 0);
    barLayout->addWidget(bar);
    m_fileTable->setCellWidget(row, 4, barWrapper);

    auto *st = new QTableWidgetItem("Pending");
    st->setForeground(QColor(107, 116, 148));
    st->setTextAlignment(Qt::AlignCenter);
    m_fileTable->setItem(row, 5, st);
    return true;
}

// ── Recursive dir scan ────────────────────────────────────────────────────────

void MainWindow::scanDir(const QString &dirPath, bool recursive, QStringList *duplicates) {
    QStringList filters;
    for (const QString &e : audioExtensions()) filters << "*." + e;

    QDirIterator::IteratorFlags flags = recursive
                                            ? QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
                                            : QDirIterator::NoIteratorFlags;

    QDirIterator it(dirPath, filters, QDir::Files, flags);
    while (it.hasNext())
        addFileRow(it.next(), duplicates);
}

// ── File / Folder add ─────────────────────────────────────────────────────────

// Scrollable list dialog — used instead of QMessageBox when the list of
// items could be long (e.g. many duplicate files).
static void showScrollableList(QWidget *parent, const QString &title,
                               const QString &intro, const QStringList &items) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.resize(440, 360);

    auto *layout = new QVBoxLayout(&dlg);
    if (!intro.isEmpty()) {
        auto *lbl = new QLabel(intro);
        lbl->setWordWrap(true);
        layout->addWidget(lbl);
    }

    auto *list = new QListWidget();
    list->addItems(items);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setAlternatingRowColors(true);
    layout->addWidget(list, 1);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    layout->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

    dlg.exec();
}

// Show a single summary dialog for any files skipped as duplicates
static void reportDuplicates(QWidget *parent, const QStringList &dupes) {
    if (dupes.isEmpty()) return;
    QString intro = dupes.size() == 1
                        ? "This file is already in the list and was skipped:"
                        : QString("%1 file(s) were already in the list and were skipped:").arg(dupes.size());
    showScrollableList(parent, "Duplicate files skipped", intro, dupes);
}

void MainWindow::addFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Add Audio Files", QDir::homePath(),
        "Audio Files (*.flac *.mp3 *.ogg *.opus *.wav *.aiff *.aif "
        "*.m4a *.aac *.wma *.ape *.wv *.mka *.tta);;All Files (*)");
    QStringList dupes;
    for (const QString &p : files) addFileRow(p, &dupes);
    reportDuplicates(this, dupes);
    m_statusLabel->setText(QString("%1 file(s) in queue").arg(m_fileTable->rowCount()));
}

void MainWindow::addFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Add Folder", QDir::homePath());
    if (dir.isEmpty()) return;

    auto btn = QMessageBox::question(this, "Add Folder",
                                     "Include files in subfolders?",
                                     QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel) return;

    QStringList dupes;
    scanDir(dir, btn == QMessageBox::Yes, &dupes);
    reportDuplicates(this, dupes);
    m_statusLabel->setText(QString("%1 file(s) in queue").arg(m_fileTable->rowCount()));
}

void MainWindow::removeSelected() {
    QList<int> rows;
    const auto selected = m_fileTable->selectedItems();
    for (auto *item : selected) {
        int r = item->row();
        if (!rows.contains(r)) rows.prepend(r);
    }
    std::sort(rows.rbegin(), rows.rend());
    for (int r : rows) m_fileTable->removeRow(r);
    m_statusLabel->setText(QString("%1 file(s) in queue").arg(m_fileTable->rowCount()));
}

void MainWindow::clearAll() {
    if (m_running) return;
    m_fileTable->setRowCount(0);
    m_outputPaths.clear();
    m_statusLabel->setText("Ready");
    m_totalProgress->reset(0);
}

// ── Drag & Drop ───────────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e) {
    // Collect valid local paths — guard against non-file:// URLs from some apps
    QStringList paths;
    const auto urls = e->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString p = url.toLocalFile();
        if (!p.isEmpty()) paths << p;
    }
    if (paths.isEmpty()) return;

    // Accept the event BEFORE any dialog — prevents Qt re-entrancy crash
    e->acceptProposedAction();

    bool hasDir = false;
    for (const QString &p : std::as_const(paths))
        if (QFileInfo(p).isDir()) { hasDir = true; break; }

    // Defer all processing (including the dialog) to after the event loop
    // has fully unwound the drop event — QMessageBox inside dropEvent crashes
    QTimer::singleShot(0, this, [this, paths, hasDir]() {
        bool recursive = false;
        if (hasDir) {
            auto btn = QMessageBox::question(this, "Add Folder",
                                             "Include files in subfolders?",
                                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (btn == QMessageBox::Cancel) return;
            recursive = (btn == QMessageBox::Yes);
        }
        QStringList dupes;
        for (const QString &path : paths) {
            QFileInfo fi(path);
            if (fi.isDir())
                scanDir(path, recursive, &dupes);
            else if (audioExtensions().contains(fi.suffix().toLower()))
                addFileRow(path, &dupes);
        }
        reportDuplicates(this, dupes);
        m_statusLabel->setText(
            QString("%1 file(s) in queue").arg(m_fileTable->rowCount()));
    });
}

// ── Output path ───────────────────────────────────────────────────────────────

void MainWindow::browseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
    if (!dir.isEmpty()) m_outputDirEdit->setText(dir);
}

QString MainWindow::buildOutputPath(const QString &inputPath) {
    QFileInfo fi(inputPath);
    const auto fmts = enabledFormats();
    int idx = m_formatCombo->currentIndex();
    QString ext = (idx >= 0 && idx < fmts.size()) ? fmts[idx].ext : "mp3";
    QString base = fi.completeBaseName() + "." + ext;
    if (m_sameDir->isChecked()) return fi.absoluteDir().filePath(base);
    QString outDir = m_outputDirEdit->text().trimmed();
    if (outDir.isEmpty()) outDir = fi.absolutePath();
    return QDir(outDir).filePath(base);
}

// ── FFmpeg arg builder ────────────────────────────────────────────────────────

QStringList MainWindow::buildFfmpegArgs(const ConversionJob &job) {
    const auto fmts = enabledFormats();
    int idx = m_formatCombo->currentIndex();
    if (idx < 0 || idx >= fmts.size()) idx = 0;
    const FormatDef &fmt = fmts[idx];

    QStringList args;
    args << "-y" << "-i" << job.inputPath;

    // ── Stream mapping ────────────────────────────────────────────────────────
    // Opus/Ogg/WAV/AIFF: containers don't reliably carry cover art → audio only
    // AAC (m4a): cover art mapping needs a separate video codec pass, skip for now
    // MP3/FLAC: support embedded cover art via -map 0
    bool canMapAll = fmt.supportsCover && m_keepCover->isChecked();

    if (canMapAll) {
        args << "-map" << "0";
    } else {
        args << "-map" << "0:a";
    }

    if (m_keepTags->isChecked())
        args << "-map_metadata" << "0";
    else
        args << "-map_metadata" << "-1";

    // ── Channels / sample rate ────────────────────────────────────────────────
    if (m_channelsCombo->currentIndex() == 1)      args << "-ac" << "2";
    else if (m_channelsCombo->currentIndex() == 2) args << "-ac" << "1";

    if (m_samplerateCombo->currentIndex() > 0) {
        static const char* rates[] = {"","48000","44100","32000","22050"};
        args << "-ar" << rates[m_samplerateCombo->currentIndex()];
    }

    // ── Codec (must come BEFORE quality/bitrate args) ─────────────────────────
    if (!fmt.codecArg.isEmpty())
        args << "-codec:a" << fmt.codecArg;

    // ── Quality args ──────────────────────────────────────────────────────────
    int qi = m_qualityCombo->currentIndex();
    if (qi >= 0 && qi < fmt.qualityArgs.size()) {
        // Each qualityArg entry uses '\n' as separator between flag and value
        const QStringList tokens = fmt.qualityArgs[qi].split('\n');
        for (const QString &token : tokens)
            if (!token.isEmpty()) args << token;
    }

    // ── Vorbis: disable variable bitrate mode flag if using -q:a ─────────────
    // (libvorbis with -q:a doesn't want -b:a, already handled by the table above)

    // ── Output file ───────────────────────────────────────────────────────────
    args << job.outputPath;
    return args;
}

// ── Start conversion ──────────────────────────────────────────────────────────

void MainWindow::startConversion() {
    int total = m_fileTable->rowCount();
    if (total == 0) {
        QMessageBox::information(this, "No files", "Add files to convert first.");
        return;
    }
    if (!m_sameDir->isChecked() && m_outputDirEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "No output folder", "Please select an output directory.");
        return;
    }

    m_jobs.clear();
    m_ffmpegLogs.clear();
    m_outputPaths.clear();
    m_cancelFlag.storeRelease(0);
    m_doneCount.storeRelease(0);
    m_activeCount.storeRelease(0);
    m_totalJobs = total;
    m_running   = true;

    bool overwrite = m_overwriteFiles && m_overwriteFiles->isChecked();
    for (int i = 0; i < total; i++) {
        QString input = m_fileTable->item(i, 0)->data(Qt::UserRole).toString();
        ConversionJob job{ i, input, buildOutputPath(input), "Pending", overwrite };
        m_jobs.append(job);
        setJobStatus(i, "Pending");
        if (auto *bar = qobject_cast<QProgressBar*>(m_fileTable->cellWidget(i, 4)->layout()->itemAt(0)->widget()))
            bar->setValue(0);
    }

    m_totalProgress->reset(total);
    m_btnStart->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_btnAddFiles->setEnabled(false);
    m_btnAddFolder->setEnabled(false);
    m_btnRemove->setEnabled(false);
    m_btnClear->setEnabled(false);

    if (m_relay) m_relay->deleteLater();
    m_relay = new WorkerRelay(this);
    connect(m_relay, &WorkerRelay::progressChanged,
            this, &MainWindow::onProgressChanged, Qt::QueuedConnection);
    connect(m_relay, &WorkerRelay::jobFinished,
            this, &MainWindow::onJobFinished,     Qt::QueuedConnection);
    connect(m_relay, &WorkerRelay::logLine,
            this, &MainWindow::onLogLine,         Qt::QueuedConnection);

    int threads = SettingsDialog::savedThreadCount();
    if (!m_pool) m_pool = new QThreadPool(this);
    m_pool->setMaxThreadCount(threads);
    m_statusLabel->setText(
        QString("Converting %1 file(s) on %2 thread(s)…").arg(total).arg(threads));

    // Set activeCount to total BEFORE submitting any workers to avoid
    // a race where a fast-finishing job sees remaining==0 mid-queue.
    m_activeCount.storeRelease(total);

    for (const ConversionJob &job : std::as_const(m_jobs)) {
        setJobStatus(job.row, "Queued");
        auto *w = new ConversionWorker(job, buildFfmpegArgs(job), m_relay, &m_cancelFlag);
        m_pool->start(w);
    }
}

// ── Progress / finish callbacks ───────────────────────────────────────────────

void MainWindow::onProgressChanged(int row, int percent) {
    if (auto *bar = qobject_cast<QProgressBar*>(m_fileTable->cellWidget(row, 4)->layout()->itemAt(0)->widget()))
        bar->setValue(percent);
    if (percent > 0 && percent < 100)
        setJobStatus(row, "Converting…");
}

void MainWindow::onJobFinished(int row, bool success, QString errorMsg) {
    if (success) {
        setJobStatus(row, "✓ Done");
        if (auto *bar = qobject_cast<QProgressBar*>(m_fileTable->cellWidget(row, 4)->layout()->itemAt(0)->widget()))
            bar->setValue(100);
        // Record the output path so context menu can open target location
        if (row < m_jobs.size())
            m_outputPaths[row] = m_jobs[row].outputPath;
        m_doneCount.fetchAndAddAcquire(1);
    } else if (errorMsg == "Cancelled") {
        setJobStatus(row, "— Cancelled");
        if (row < m_jobs.size())
            m_outputPaths[row] = m_jobs[row].outputPath;
    } else if (errorMsg.startsWith("Skipped:")) {
        setJobStatus(row, "⏭ Skipped", errorMsg.mid(QStringLiteral("Skipped:").length()));
        if (row < m_jobs.size())
            m_outputPaths[row] = m_jobs[row].outputPath;
    } else {
        // Status column shows just "Error"; full message is in the tooltip.
        setJobStatus(row, "✗ Error", errorMsg);
        if (row < m_jobs.size())
            m_outputPaths[row] = m_jobs[row].outputPath;
    }

    int remaining = m_activeCount.fetchAndAddAcquire(-1) - 1;

    if (remaining <= 0) {
        m_running = false;
        m_btnStart->setEnabled(true);
        m_btnCancel->setEnabled(false);
        m_btnAddFiles->setEnabled(true);
        m_btnAddFolder->setEnabled(true);
        updateListButtons();
        int done = m_doneCount.loadAcquire();
        m_statusLabel->setText(
            m_cancelFlag.loadAcquire()
                ? QString("Cancelled — %1/%2 completed.").arg(done).arg(m_totalJobs)
                : QString("Done — %1/%2 converted.").arg(done).arg(m_totalJobs));
    }
}

// ── Cancel ────────────────────────────────────────────────────────────────────

void MainWindow::cancelConversion() {
    if (!m_running) return;
    m_cancelFlag.storeRelease(1);
    m_statusLabel->setText("Cancelling…");
    m_btnCancel->setEnabled(false);
    m_totalProgress->reset(m_totalJobs);
}

// ── Status label helper ───────────────────────────────────────────────────────

void MainWindow::setJobStatus(int row, const QString &status, const QString &errorMsg) {
    if (row < 0 || row >= m_fileTable->rowCount()) return;
    auto *item = m_fileTable->item(row, 5);
    if (!item) {
        item = new QTableWidgetItem();
        item->setTextAlignment(Qt::AlignCenter);
        m_fileTable->setItem(row, 5, item);
    }
    item->setText(status);
    // Tooltip shows the reason/error message only (no extra hint line)
    item->setToolTip(errorMsg.isEmpty() ? QString() : errorMsg);
    if      (status.startsWith("✓"))  item->setForeground(QColor( 76, 175, 125));
    else if (status.startsWith("✗"))  item->setForeground(QColor(224,  92,  92));
    else if (status == "Converting…") item->setForeground(QColor(240, 160,  48));
    else if (status.startsWith("⏭"))  item->setForeground(QColor(200, 150,  60));
    else if (status.startsWith("—"))  item->setForeground(QColor(139, 106, 106));
    else                               item->setForeground(QColor(107, 116, 148));
    updateStats();
}

// ── Log pane slots ───────────────────────────────────────────────────────────

// Colour rules:
//   Red   — lines containing "Error", "error", "Invalid", "No such"
//   Yellow — lines containing "Warning", "warning", "deprecated"
//   Cyan  — lines starting with "Stream mapping" or "Stream #" (codec info)
//   Grey  — everything else (encoder stats, metadata, etc.)
void MainWindow::onLogLine(int row, QString filename, QString line) {
    // Always store the raw line (before filtering) for the popup
    m_ffmpegLogs[row].append(line);

    // Suppress noisy -progress key=value lines from reaching the log
    if (line.startsWith("out_time") || line.startsWith("bitrate=") ||
        line.startsWith("total_size=") || line.startsWith("speed=") ||
        line.startsWith("dup_frames=") || line.startsWith("drop_frames=") ||
        line.startsWith("frame=") || line == "progress=continue" ||
        line == "progress=end")
        return;

    // Prefix with filename so parallel conversions are distinguishable
    QString display = QString("[%1] %2").arg(filename, line);

    // Pick colour
    QString colour;
    if (line.startsWith("Skipped:"))
        colour = "#c89632";
    else if (line.contains("Error", Qt::CaseInsensitive) ||
             line.contains("Invalid") ||
             line.contains("No such file") ||
             line.contains("same as Input"))
        colour = "#e05c5c";
    else if (line.contains("Warning", Qt::CaseInsensitive) ||
             line.contains("deprecated", Qt::CaseInsensitive))
        colour = "#f0a030";
    else if (line.startsWith("Stream ") || line.startsWith("  Stream"))
        colour = "#5bb8d4";
    else if (line.startsWith("Output #") || line.startsWith("Input #"))
        colour = "#7eb8ff";
    else
        colour = "#8b92a8";

    // appendHtml is thread-safe via QueuedConnection — we're on the main thread here
    m_logView->appendHtml(
        QString("<span style='color:%1;font-family:monospace;font-size:11px;'>%2</span>")
            .arg(colour, display.toHtmlEscaped()));

    if (m_logView->property("autoScroll").toBool()) {
        auto *bar = m_logView->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

void MainWindow::clearLog() {
    m_logView->clear();
}

void MainWindow::toggleLog(bool visible) {
    m_logDock->setVisible(visible);
}

// ── Per-file log popup ────────────────────────────────────────────────────────

void MainWindow::showLogPopup(int row) {
    if (row < 0 || row >= m_fileTable->rowCount()) return;

    QString filename = m_fileTable->item(row, 0)
                           ? m_fileTable->item(row, 0)->toolTip()
                           : QString("row %1").arg(row);
    QFileInfo fi(filename);

    const QStringList &lines = m_ffmpegLogs.value(row);

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QString("ffmpeg log — %1").arg(fi.fileName()));
    dlg->resize(820, 520);

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // Header label
    auto *header = new QLabel(QString("<b>%1</b>").arg(fi.fileName()));
    header->setTextFormat(Qt::RichText);
    layout->addWidget(header);

    // Log viewer
    auto *view = new QPlainTextEdit(dlg);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setObjectName("logView");  // reuse log styling
    view->document()->setDefaultStyleSheet(
        "span { font-family: monospace; font-size: 11px; }");

    if (lines.isEmpty()) {
        view->setPlainText("No ffmpeg output captured for this file yet.");
    } else {
        // Render with the same colour rules as the main log pane
        auto colourFor = [](const QString &line) -> QString {
            if (line.startsWith("Skipped:"))
                return "#c89632";
            if (line.contains("Error", Qt::CaseInsensitive) ||
                line.contains("Invalid") ||
                line.contains("No such file") ||
                line.contains("same as Input"))
                return "#e05c5c";
            if (line.contains("Warning", Qt::CaseInsensitive) ||
                line.contains("deprecated", Qt::CaseInsensitive))
                return "#f0a030";
            if (line.startsWith("Stream ") || line.startsWith("  Stream"))
                return "#5bb8d4";
            if (line.startsWith("Output #") || line.startsWith("Input #"))
                return "#7eb8ff";
            return "#8b92a8";
        };

        QString html;
        html.reserve(lines.size() * 80);
        for (const QString &l : lines) {
            html += QString("<span style='color:%1;font-family:monospace;"
                            "font-size:11px;'>%2</span><br>")
                        .arg(colourFor(l), l.toHtmlEscaped());
        }
        view->appendHtml(html);
        // Scroll to top so the user sees the ffmpeg invocation header first
        view->moveCursor(QTextCursor::Start);
    }

    layout->addWidget(view, 1);

    // Close button
    auto *btnClose = new QPushButton("Close");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    layout->addLayout(btnRow);

    dlg->show();
}

// ── Column sorting ───────────────────────────────────────────────────────────

void MainWindow::onHeaderClicked(int column) {
    // Format column (2) is not sortable
    if (column == 2) return;

    if (m_sortColumn == column) {
        m_sortOrder = (m_sortOrder == Qt::AscendingOrder)
        ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_sortColumn = column;
        m_sortOrder  = Qt::AscendingOrder;
    }
    m_fileTable->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);

    int rowCount = m_fileTable->rowCount();
    if (rowCount < 2) return;

    // ── Strategy: never remove bar widgets from the table mid-sort.
    // Instead snapshot all row data (items + bar value), sort the snapshot,
    // then overwrite each row in-place. Bar widgets stay in their slots the
    // whole time — only their progress value is updated.
    struct RowSnapshot {
        int     origRow;
        QString sortKey;
        // items cloned so we own them independently (col 4 is the bar widget,
        // handled separately via barValue)
        QTableWidgetItem *col0 = nullptr; // filename
        QTableWidgetItem *col1 = nullptr; // source dir
        QTableWidgetItem *col2 = nullptr; // format
        QTableWidgetItem *col3 = nullptr; // duration
        int               barValue = 0;   // col 4 = progress bar
        QTableWidgetItem *col5 = nullptr; // status
        QString           statusTooltip;
    };

    QList<RowSnapshot> snap;
    snap.reserve(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        RowSnapshot s;
        s.origRow = r;

        auto cloneCol = [&](int c) -> QTableWidgetItem* {
            auto *it = m_fileTable->item(r, c);
            return it ? it->clone() : new QTableWidgetItem();
        };
        s.col0 = cloneCol(0);
        s.col1 = cloneCol(1);
        s.col2 = cloneCol(2);
        s.col3 = cloneCol(3);
        s.col5 = cloneCol(5);
        if (auto *it = m_fileTable->item(r, 5))
            s.statusTooltip = it->toolTip();

        // Read bar value
        if (auto *w = m_fileTable->cellWidget(r, 4)) {
            if (auto *bar = w->findChild<QProgressBar*>())
                s.barValue = bar->value();
        }

        // Build sort key
        switch (column) {
        case 0: s.sortKey = s.col0->text(); break;
        case 1: s.sortKey = s.col1->text(); break;
        case 3: s.sortKey = s.col3->text(); break;
        case 4: s.sortKey = QString::asprintf("%03d", s.barValue); break;
        case 5: s.sortKey = s.col5->text(); break;
        default: s.sortKey = m_fileTable->item(r, column)
                            ? m_fileTable->item(r, column)->text() : QString(); break;
        }

        snap.append(s);
    }

    // Sort
    std::sort(snap.begin(), snap.end(), [this](const RowSnapshot &a, const RowSnapshot &b) {
        int cmp = QString::localeAwareCompare(a.sortKey, b.sortKey);
        return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
    });

    // Build old→new index map; remap auxiliary structures
    QMap<int,int> oldToNew;
    for (int newR = 0; newR < snap.size(); ++newR)
        oldToNew[snap[newR].origRow] = newR;

    QMap<int, QStringList> newLogs;
    for (auto it = m_ffmpegLogs.begin(); it != m_ffmpegLogs.end(); ++it)
        if (oldToNew.contains(it.key())) newLogs[oldToNew[it.key()]] = it.value();
    m_ffmpegLogs = newLogs;

    QMap<int, QString> newOutputPaths;
    for (auto it = m_outputPaths.begin(); it != m_outputPaths.end(); ++it)
        if (oldToNew.contains(it.key())) newOutputPaths[oldToNew[it.key()]] = it.value();
    m_outputPaths = newOutputPaths;

    for (auto &job : m_jobs)
        if (oldToNew.contains(job.row)) job.row = oldToNew[job.row];

    // Overwrite each row in-place — bar widgets never leave their slots
    for (int newR = 0; newR < snap.size(); ++newR) {
        const RowSnapshot &s = snap[newR];
        m_fileTable->setItem(newR, 0, s.col0);
        m_fileTable->setItem(newR, 1, s.col1);
        m_fileTable->setItem(newR, 2, s.col2);
        m_fileTable->setItem(newR, 3, s.col3);
        m_fileTable->setItem(newR, 5, s.col5);
        if (s.col5) s.col5->setToolTip(s.statusTooltip);
        // Update bar value in the existing widget
        if (auto *w = m_fileTable->cellWidget(newR, 4)) {
            if (auto *bar = w->findChild<QProgressBar*>())
                bar->setValue(s.barValue);
        }
    }

    // Free snapshots that are now owned by the table (setItem transferred them)
    // Nothing to do — QTableWidget took ownership of the cloned items above.
}


// ── Right-click context menu ──────────────────────────────────────────────────

void MainWindow::showContextMenu(const QPoint &pos) {
    QList<int> selectedRows;
    const auto selected = m_fileTable->selectedItems();
    for (auto *item : selected) {
        int r = item->row();
        if (!selectedRows.contains(r)) selectedRows.append(r);
    }
    if (selectedRows.isEmpty() && m_fileTable->rowCount() == 0) return;

    auto *menu = new QMenu(this);

    // ── Open Source Location ──────────────────────────────────────────────────
    auto *actOpenSrc = menu->addAction("Open Source Location");
    bool sameSourceDir = false;
    QString commonSourceDir;
    if (!selectedRows.isEmpty()) {
        QSet<QString> sourceDirs;
        for (int r : selectedRows) {
            auto *it = m_fileTable->item(r, 0);
            if (it) {
                QFileInfo fi(it->data(Qt::UserRole).toString());
                sourceDirs.insert(fi.absolutePath());
            }
        }
        sameSourceDir = (sourceDirs.size() == 1);
        if (sameSourceDir) commonSourceDir = *sourceDirs.begin();
    }
    if (!sameSourceDir) {
        actOpenSrc->setEnabled(false);
        actOpenSrc->setToolTip("Cannot open source location: the selected files are not in the same location");
    }

    // ── Open Target Location ──────────────────────────────────────────────────
    auto *actOpenTarget = menu->addAction("Open Target Location");
    bool targetKnown = false;
    QString commonTargetDir;
    if (!selectedRows.isEmpty()) {
        QSet<QString> targetDirs;
        for (int r : selectedRows) {
            auto *statusItem = m_fileTable->item(r, 5);
            if (!statusItem) continue;
            QString st = statusItem->text();
            bool processed = st.startsWith("✓") || st.startsWith("✗") || st.startsWith("—")
                             || st.startsWith("⏭");
            if (processed && m_outputPaths.contains(r)) {
                QFileInfo fi(m_outputPaths[r]);
                targetDirs.insert(fi.absolutePath());
            }
        }
        if (!targetDirs.isEmpty()) {
            targetKnown = true;
            commonTargetDir = *targetDirs.begin();
            // If multiple distinct target dirs, open each
        }
    }
    if (!targetKnown) {
        actOpenTarget->setEnabled(false);
        actOpenTarget->setToolTip("Target location not set");
    }

    // ── Remove from list ──────────────────────────────────────────────────────
    auto *actRemove = menu->addAction(selectedRows.isEmpty()
                                          ? "Remove from List" : QString("Remove %1 File(s)").arg(selectedRows.size()));
    actRemove->setEnabled(!selectedRows.isEmpty() && !m_running);

    menu->addSeparator();

    // ── Clear list ────────────────────────────────────────────────────────────
    auto *actClear = menu->addAction("Clear List");
    actClear->setEnabled(m_fileTable->rowCount() > 0 && !m_running);

    // Tooltips on disabled QActions: Qt suppresses them by default.
    // We install an event filter on the menu that catches MouseMove and shows
    // QToolTip manually for any disabled action under the cursor.
    struct MenuTooltipFilter : public QObject {
        QMenu *menu;
        explicit MenuTooltipFilter(QMenu *m) : QObject(m), menu(m) {}
        bool eventFilter(QObject *, QEvent *e) override {
            if (e->type() == QEvent::MouseMove) {
                auto *me = static_cast<QMouseEvent*>(e);
                QAction *act = menu->actionAt(me->pos());
                if (act && !act->isEnabled() && !act->toolTip().isEmpty())
                    QToolTip::showText(me->globalPosition().toPoint(), act->toolTip(), menu);
                else
                    QToolTip::hideText();
            }
            return false;
        }
    };
    menu->installEventFilter(new MenuTooltipFilter(menu));

    QAction *chosen = menu->exec(m_fileTable->viewport()->mapToGlobal(pos));
    menu->deleteLater();

    if (!chosen) return;

    if (chosen == actOpenSrc && sameSourceDir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(commonSourceDir));
    } else if (chosen == actOpenTarget && targetKnown) {
        // Open all distinct target dirs
        QSet<QString> dirs;
        for (int r : selectedRows) {
            if (m_outputPaths.contains(r)) {
                dirs.insert(QFileInfo(m_outputPaths[r]).absolutePath());
            }
        }
        for (const QString &d : dirs)
            QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    } else if (chosen == actRemove) {
        QList<int> toRemove = selectedRows;
        std::sort(toRemove.rbegin(), toRemove.rend());
        for (int r : toRemove) {
            // Update m_outputPaths keys for rows that shift up
            QMap<int, QString> newOut;
            for (auto it = m_outputPaths.begin(); it != m_outputPaths.end(); ++it) {
                if (it.key() < r) newOut[it.key()] = it.value();
                else if (it.key() > r) newOut[it.key() - 1] = it.value();
                // key == r is removed
            }
            m_outputPaths = newOut;
            QMap<int, QStringList> newLogs;
            for (auto it = m_ffmpegLogs.begin(); it != m_ffmpegLogs.end(); ++it) {
                if (it.key() < r) newLogs[it.key()] = it.value();
                else if (it.key() > r) newLogs[it.key() - 1] = it.value();
            }
            m_ffmpegLogs = newLogs;
            m_fileTable->removeRow(r);
        }
        m_statusLabel->setText(QString("%1 file(s) in queue").arg(m_fileTable->rowCount()));
    } else if (chosen == actClear) {
        clearAll();
    }
}

// ── Close guard ───────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_running) {
        auto btn = QMessageBox::question(this, "SonifiQ",
                                         "Conversion is in progress. Cancel and quit?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (btn == QMessageBox::No) { event->ignore(); return; }
        m_cancelFlag.storeRelease(1);
        if (m_pool) m_pool->waitForDone(4000);
    }
    event->accept();
}