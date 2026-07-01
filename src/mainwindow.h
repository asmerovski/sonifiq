#pragma once
#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QList>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QMutex>
#include <QAtomicInt>
#include <QRunnable>
#include <QThreadPool>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QMap>
#include "settingsdialog.h"

// Custom progress bar that shows success/failed/skipped counts as
// distinct coloured segments instead of a single uniform fill.
class SegmentedProgressBar;

struct ConversionJob {
    int     row;
    QString inputPath;
    QString outputPath;
    QString status;
    bool    overwrite = false;
};

// ── Worker signal relay ───────────────────────────────────────────────────────
class WorkerRelay : public QObject {
    Q_OBJECT
public:
    explicit WorkerRelay(QObject *parent = nullptr) : QObject(parent) {}
signals:
    void progressChanged(int row, int percent);
    void jobFinished(int row, bool success, QString errorMsg);
    void logLine(int row, QString filename, QString line);
};

// ── Per-file conversion worker ────────────────────────────────────────────────
class ConversionWorker : public QRunnable {
public:
    ConversionWorker(const ConversionJob &job,
                     const QStringList   &ffmpegArgs,
                     WorkerRelay         *relay,
                     QAtomicInt          *cancelFlag)
        : m_job(job), m_args(ffmpegArgs), m_relay(relay), m_cancel(cancelFlag)
    { setAutoDelete(true); }

    void run() override;

private:
    ConversionJob  m_job;
    QStringList    m_args;
    WorkerRelay   *m_relay = nullptr;
    QAtomicInt    *m_cancel;
    QByteArray     m_stderrBuf;
};

// ── Main window ───────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void addFiles();
    void addFolder();
    void removeSelected();
    void clearAll();
    void startConversion();
    void cancelConversion();
    void openSettings();
    void onProgressChanged(int row, int percent);
    void onJobFinished(int row, bool success, QString errorMsg);
    void onLogLine(int row, QString filename, QString line);
    void clearLog();
    void toggleLog(bool visible);
    void showLogPopup(int row);
    void browseOutputDir();
    void updateFormatOptions(int index);
    void updateListButtons();
    void onHeaderClicked(int column);
    void showContextMenu(const QPoint &pos);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void rebuildFormatCombo();
    bool addFileRow(const QString &path, QStringList *duplicates = nullptr);
    void scanDir(const QString &dirPath, bool recursive, QStringList *duplicates = nullptr);
    void setJobStatus(int row, const QString &status, const QString &errorMsg = {});
    void updateStats();
    void computeRowCounts(int &success, int &failed, int &skipped, int &pending) const;
    QString buildOutputPath(const QString &inputPath);
    QStringList buildFfmpegArgs(const ConversionJob &job);

    // Toolbar
    QTableWidget  *m_fileTable   = nullptr;
    QStackedWidget *m_tableStack = nullptr;
    QLabel        *m_emptyLabel  = nullptr;
    QPushButton   *m_btnAddFiles = nullptr;
    QPushButton   *m_btnAddFolder = nullptr;
    QPushButton   *m_btnRemove = nullptr;
    QPushButton   *m_btnClear = nullptr;
    QPushButton   *m_btnSettings = nullptr;

    // Settings panel
    QComboBox     *m_formatCombo = nullptr;
    QComboBox     *m_qualityCombo = nullptr;
    QComboBox     *m_channelsCombo = nullptr;
    QComboBox     *m_samplerateCombo = nullptr;
    QCheckBox     *m_sameDir = nullptr;
    QLineEdit     *m_outputDirEdit = nullptr;
    QPushButton   *m_btnBrowse = nullptr;
    QCheckBox     *m_overwriteFiles = nullptr;
    QCheckBox     *m_keepTags = nullptr;
    QCheckBox     *m_keepCover = nullptr;

    // Log pane
    QDockWidget   *m_logDock = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton   *m_btnToggleLog = nullptr;

    // Bottom bar
    QPushButton   *m_btnStart = nullptr;
    QPushButton   *m_btnCancel = nullptr;
    SegmentedProgressBar *m_totalProgress = nullptr;
    QLabel        *m_statusLabel = nullptr;
    QLabel        *m_statsLabel = nullptr;

    // State
    QList<ConversionJob>  m_jobs;
    QMap<int, QStringList> m_ffmpegLogs;   // per-row raw log lines
    QMap<int, QString>    m_outputPaths;   // per-row output path (set after job finishes)
    WorkerRelay          *m_relay       = nullptr;
    QAtomicInt            m_cancelFlag  { 0 };
    QAtomicInt            m_doneCount   { 0 };
    QAtomicInt            m_activeCount { 0 };
    QThreadPool          *m_pool        = nullptr;
    int                   m_totalJobs   = 0;
    bool                  m_running     = false;

    // Sorting state
    int           m_sortColumn = -1;
    Qt::SortOrder m_sortOrder  = Qt::AscendingOrder;
};