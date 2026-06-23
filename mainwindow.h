#pragma once
#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QList>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QSpinBox>
#include <QMutex>
#include <QAtomicInt>
#include <QRunnable>
#include <QThreadPool>
#include <QDockWidget>
#include <QPlainTextEdit>
#include "settingsdialog.h"

struct ConversionJob {
    int     row;
    QString inputPath;
    QString outputPath;
    QString status;
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
    void browseOutputDir();
    void updateFormatOptions(int index);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void applyStylesheet();
    void rebuildFormatCombo();
    void addFileRow(const QString &path);
    void scanDir(const QString &dirPath, bool recursive);
    void setJobStatus(int row, const QString &status);
    void updateOverallProgress();
    QString buildOutputPath(const QString &inputPath);
    QStringList buildFfmpegArgs(const ConversionJob &job);

    // Toolbar
    QTableWidget  *m_fileTable = nullptr;
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
    QCheckBox     *m_keepTags = nullptr;
    QCheckBox     *m_keepCover = nullptr;
    QSpinBox      *m_threadsSpin = nullptr;

    // Log pane
    QDockWidget   *m_logDock = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton   *m_btnToggleLog = nullptr;

    // Bottom bar
    QPushButton   *m_btnStart = nullptr;
    QPushButton   *m_btnCancel = nullptr;
    QProgressBar  *m_totalProgress = nullptr;
    QLabel        *m_statusLabel = nullptr;

    // State
    QList<ConversionJob>  m_jobs;
    WorkerRelay          *m_relay       = nullptr;
    QAtomicInt            m_cancelFlag  { 0 };
    QAtomicInt            m_doneCount   { 0 };
    QAtomicInt            m_activeCount { 0 };
    int                   m_totalJobs   = 0;
    bool                  m_running     = false;
};
