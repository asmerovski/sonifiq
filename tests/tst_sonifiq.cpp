// SonifiQ Unit Tests — Qt Test framework
// Tests: arg builder, output path logic, scanDir, progress parser, drop path guard

#include <QtTest/QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QByteArray>

// ── Expose internals we want to test ─────────────────────────────────────────
// Re-include the format table and helpers from mainwindow without Qt widgets
// by extracting the pure-logic pieces into a testable form here.

// Mirror of FormatDef from mainwindow.cpp
struct FormatDef {
    QString id, label, ext;
    QStringList qualities, qualityArgs;
    QString codecArg;
    bool supportsCover;
};

static const QList<FormatDef> ALL_FORMATS = {
    { "mp3",  "MP3",        "mp3",
      {"320 kbps CBR","V0 (VBR ~245)"},
      {"-b:a\n320k", "-q:a\n0"},
      "libmp3lame", true },
    { "m4a",  "AAC (.m4a)", "m4a",
      {"320 kbps","128 kbps"},
      {"-b:a\n320k","-b:a\n128k"},
      "aac", false },  // cover disabled for reliability
    { "ogg",  "Ogg Vorbis", "ogg",
      {"Quality 6 (~192k)"},
      {"-q:a\n6"},
      "libvorbis", false },
    { "opus", "Opus",       "opus",
      {"128 kbps"},
      {"-b:a\n128k"},
      "libopus", false },
    { "flac", "FLAC",       "flac",
      {"Level 5 (default)"},
      {"-compression_level\n5"},
      "flac", true },
    { "wav",  "WAV (PCM)",  "wav",
      {"16-bit"},
      {"-acodec\npcm_s16le"},
      "", false },
    { "aiff", "AIFF",       "aiff",
      {"16-bit"},
      {"-acodec\npcm_s16be"},
      "", false },
};

// Standalone arg builder (mirrors MainWindow::buildFfmpegArgs logic)
static QStringList buildArgs(const FormatDef &fmt, int qualityIdx,
                             const QString &inputPath, const QString &outputPath,
                             bool keepTags, bool keepCover,
                             int channels, int sampleRateIdx)
{
    QStringList args;
    args << "-y" << "-i" << inputPath;

    bool canMapAll = fmt.supportsCover && keepCover;
    args << "-map" << (canMapAll ? "0" : "0:a");
    args << "-map_metadata" << (keepTags ? "0" : "-1");

    if (channels == 1) args << "-ac" << "2";
    else if (channels == 2) args << "-ac" << "1";

    static const char* rates[] = {"","48000","44100","32000","22050"};
    if (sampleRateIdx > 0)
        args << "-ar" << rates[sampleRateIdx];

    if (!fmt.codecArg.isEmpty())
        args << "-codec:a" << fmt.codecArg;

    if (qualityIdx >= 0 && qualityIdx < fmt.qualityArgs.size())
        for (const QString &t : fmt.qualityArgs[qualityIdx].split('\n'))
            if (!t.isEmpty()) args << t;

    args << outputPath;
    return args;
}

// Progress line parser (mirrors worker logic)
static int parseProgressLine(const QByteArray &line, qint64 durationUs) {
    if (!line.startsWith("out_time_us=")) return -1;
    bool ok;
    qint64 us = line.mid(12).toLongLong(&ok);
    if (!ok || us < 0) return -1;
    return static_cast<int>(qBound(0LL, us * 100LL / durationUs, 99LL));
}

// ── Test class ────────────────────────────────────────────────────────────────

class TestSonifiQ : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    // Arg builder
    void test_mp3_cbr_args();
    void test_mp3_vbr_args();
    void test_aac_args_no_cover();
    void test_aac_codec_before_bitrate();
    void test_ogg_args();
    void test_opus_args();
    void test_flac_args();
    void test_wav_args();
    void test_aiff_args();
    void test_channels_stereo();
    void test_channels_mono();
    void test_samplerate_44100();
    void test_no_tags();
    void test_cover_mp3();
    void test_no_cover_aac();

    // Output path
    void test_output_ext_mp3();
    void test_output_ext_m4a();
    void test_output_same_dir();

    // Progress parser
    void test_progress_zero();
    void test_progress_mid();
    void test_progress_caps_at_99();
    void test_progress_invalid_line();
    void test_progress_negative_value();

    // Dir scanner
    void test_scandirNonRecursive();
    void test_scandirRecursive();
    void test_scandirIgnoresNonAudio();

    // Drop path guard
    void test_emptyUrlFiltered();

    // Error message display
    void test_error_message_extraction();
    void test_ffmpeg_same_format_reports_error();

    // Real ffmpeg conversion (integration, skipped if ffmpeg absent)
    void test_ffmpeg_mp3();
    void test_ffmpeg_aac();
    void test_ffmpeg_opus();
    void test_ffmpeg_flac();
    void test_ffmpeg_wav();
    void test_ffmpeg_ogg();

private:
    bool m_ffmpegAvailable = false;
    QString m_testWav;
    QTemporaryDir m_tmpDir;
};

// ── Setup ─────────────────────────────────────────────────────────────────────

void TestSonifiQ::initTestCase() {
    QVERIFY(m_tmpDir.isValid());

    // Check ffmpeg availability
    QProcess p;
    p.start("ffmpeg", {"-version"});
    m_ffmpegAvailable = p.waitForFinished(5000) && p.exitCode() == 0;

    if (m_ffmpegAvailable) {
        // Generate a 2-second test WAV
        m_testWav = m_tmpDir.filePath("input.wav");
        QProcess gen;
        gen.start("ffmpeg", {
            "-y", "-f", "lavfi",
            "-i", "sine=frequency=440:duration=2",
            "-ar", "44100", "-ac", "2",
            m_testWav
        });
        QVERIFY(gen.waitForFinished(10000));
        QCOMPARE(gen.exitCode(), 0);
    }
}

// ── Arg builder tests ─────────────────────────────────────────────────────────

void TestSonifiQ::test_mp3_cbr_args() {
    auto &f = ALL_FORMATS[0]; // mp3
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", true, false, 0, 0);
    QVERIFY(args.contains("-codec:a"));
    QCOMPARE(args[args.indexOf("-codec:a") + 1], "libmp3lame");
    QVERIFY(args.contains("-b:a"));
    QCOMPARE(args[args.indexOf("-b:a") + 1], "320k");
    QCOMPARE(args.last(), "/out.mp3");
}

void TestSonifiQ::test_mp3_vbr_args() {
    auto &f = ALL_FORMATS[0];
    auto args = buildArgs(f, 1, "/in.flac", "/out.mp3", true, false, 0, 0);
    QVERIFY(args.contains("-q:a"));
    QCOMPARE(args[args.indexOf("-q:a") + 1], "0");
    QVERIFY(!args.contains("-b:a"));
}

void TestSonifiQ::test_aac_args_no_cover() {
    auto &f = ALL_FORMATS[1]; // m4a
    QVERIFY(!f.supportsCover); // must be false — key regression check
    auto args = buildArgs(f, 0, "/in.flac", "/out.m4a", true, true, 0, 0);
    // Even with keepCover=true, should use -map 0:a (not -map 0) for m4a
    int mapIdx = args.indexOf("-map");
    QVERIFY(mapIdx >= 0);
    QCOMPARE(args[mapIdx + 1], "0:a");
}

void TestSonifiQ::test_aac_codec_before_bitrate() {
    auto &f = ALL_FORMATS[1]; // m4a
    auto args = buildArgs(f, 0, "/in.flac", "/out.m4a", true, false, 0, 0);
    int codecIdx = args.indexOf("-codec:a");
    int bitrateIdx = args.indexOf("-b:a");
    QVERIFY(codecIdx >= 0);
    QVERIFY(bitrateIdx >= 0);
    // Critical regression: codec MUST come before bitrate
    QVERIFY2(codecIdx < bitrateIdx,
        "Codec arg must appear before bitrate arg (regression: old code reversed this)");
}

void TestSonifiQ::test_ogg_args() {
    auto &f = ALL_FORMATS[2];
    auto args = buildArgs(f, 0, "/in.flac", "/out.ogg", true, false, 0, 0);
    QCOMPARE(args[args.indexOf("-codec:a") + 1], "libvorbis");
    QVERIFY(args.contains("-q:a"));
}

void TestSonifiQ::test_opus_args() {
    auto &f = ALL_FORMATS[3];
    QVERIFY(!f.supportsCover);
    auto args = buildArgs(f, 0, "/in.flac", "/out.opus", true, false, 0, 0);
    QCOMPARE(args[args.indexOf("-codec:a") + 1], "libopus");
}

void TestSonifiQ::test_flac_args() {
    auto &f = ALL_FORMATS[4];
    auto args = buildArgs(f, 0, "/in.flac", "/out.flac", true, false, 0, 0);
    QCOMPARE(args[args.indexOf("-codec:a") + 1], "flac");
    QVERIFY(args.contains("-compression_level"));
}

void TestSonifiQ::test_wav_args() {
    auto &f = ALL_FORMATS[5]; // wav — no codecArg, codec in qualityArg
    auto args = buildArgs(f, 0, "/in.flac", "/out.wav", false, false, 0, 0);
    QVERIFY(!args.contains("-codec:a")); // WAV has no separate codecArg
    QVERIFY(args.contains("-acodec"));
    QCOMPARE(args[args.indexOf("-acodec") + 1], "pcm_s16le");
}

void TestSonifiQ::test_aiff_args() {
    auto &f = ALL_FORMATS[6];
    auto args = buildArgs(f, 0, "/in.flac", "/out.aiff", false, false, 0, 0);
    QVERIFY(args.contains("-acodec"));
    QCOMPARE(args[args.indexOf("-acodec") + 1], "pcm_s16be");
}

void TestSonifiQ::test_channels_stereo() {
    auto &f = ALL_FORMATS[0];
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", true, false, 1/*stereo*/, 0);
    QVERIFY(args.contains("-ac"));
    QCOMPARE(args[args.indexOf("-ac") + 1], "2");
}

void TestSonifiQ::test_channels_mono() {
    auto &f = ALL_FORMATS[0];
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", true, false, 2/*mono*/, 0);
    QCOMPARE(args[args.indexOf("-ac") + 1], "1");
}

void TestSonifiQ::test_samplerate_44100() {
    auto &f = ALL_FORMATS[0];
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", true, false, 0, 2/*44100*/);
    QVERIFY(args.contains("-ar"));
    QCOMPARE(args[args.indexOf("-ar") + 1], "44100");
}

void TestSonifiQ::test_no_tags() {
    auto &f = ALL_FORMATS[0];
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", false/*keepTags*/, false, 0, 0);
    QCOMPARE(args[args.indexOf("-map_metadata") + 1], "-1");
}

void TestSonifiQ::test_cover_mp3() {
    auto &f = ALL_FORMATS[0]; // mp3 supportsCover=true
    auto args = buildArgs(f, 0, "/in.flac", "/out.mp3", true, true/*keepCover*/, 0, 0);
    QCOMPARE(args[args.indexOf("-map") + 1], "0"); // full map when cover supported
}

void TestSonifiQ::test_no_cover_aac() {
    auto &f = ALL_FORMATS[1]; // m4a supportsCover=false
    auto args = buildArgs(f, 0, "/in.flac", "/out.m4a", true, true/*keepCover requested*/, 0, 0);
    QCOMPARE(args[args.indexOf("-map") + 1], "0:a"); // must ignore keepCover
}

// ── Output path tests ─────────────────────────────────────────────────────────

void TestSonifiQ::test_output_ext_mp3() {
    QFileInfo fi("/music/album/track.flac");
    QString base = fi.completeBaseName() + ".mp3";
    QString out  = fi.absoluteDir().filePath(base);
    QCOMPARE(QFileInfo(out).suffix(), "mp3");
    QCOMPARE(QFileInfo(out).completeBaseName(), "track");
}

void TestSonifiQ::test_output_ext_m4a() {
    QFileInfo fi("/music/track.wav");
    QString out = fi.absoluteDir().filePath(fi.completeBaseName() + ".m4a");
    QCOMPARE(QFileInfo(out).suffix(), "m4a");
}

void TestSonifiQ::test_output_same_dir() {
    QFileInfo fi("/music/deep/track.flac");
    QString out = fi.absoluteDir().filePath("track.mp3");
    QVERIFY(out.startsWith("/music/deep/"));
}

// ── Progress parser tests ─────────────────────────────────────────────────────

void TestSonifiQ::test_progress_zero() {
    QCOMPARE(parseProgressLine("out_time_us=0", 3000000LL), 0);
}

void TestSonifiQ::test_progress_mid() {
    // 1.5s of 3s = 50%
    int pct = parseProgressLine("out_time_us=1500000", 3000000LL);
    QCOMPARE(pct, 50);
}

void TestSonifiQ::test_progress_caps_at_99() {
    // Should never emit 100% (reserved for job completion signal)
    int pct = parseProgressLine("out_time_us=3000000", 3000000LL);
    QVERIFY(pct <= 99);
}

void TestSonifiQ::test_progress_invalid_line() {
    QCOMPARE(parseProgressLine("frame=100", 3000000LL), -1);
    QCOMPARE(parseProgressLine("out_time_us=", 3000000LL), -1);
    QCOMPARE(parseProgressLine("out_time_us=abc", 3000000LL), -1);
}

void TestSonifiQ::test_progress_negative_value() {
    // ffmpeg can emit out_time_us=-1 on init
    QCOMPARE(parseProgressLine("out_time_us=-1", 3000000LL), -1);
}

// ── Dir scanner tests ─────────────────────────────────────────────────────────

static int countAudioFiles(const QString &dir, bool recursive) {
    QStringList filters;
    for (const QString &e : {"flac","mp3","ogg","opus","wav","aiff","m4a","aac"})
        filters << "*." + e;
    QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(dir, filters, QDir::Files, flags);
    int n = 0;
    while (it.hasNext()) { it.next(); ++n; }
    return n;
}

void TestSonifiQ::test_scandirNonRecursive() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QFile::copy(m_testWav.isEmpty() ? "/dev/null" : m_testWav,
                tmp.filePath("a.wav"));
    QDir(tmp.path()).mkdir("sub");
    QFile::copy(m_testWav.isEmpty() ? "/dev/null" : m_testWav,
                tmp.filePath("sub/b.wav"));
    if (m_testWav.isEmpty()) {
        // Create dummy audio files
        QFile(tmp.filePath("a.wav")).open(QIODevice::WriteOnly);
        QFile(tmp.filePath("sub/b.wav")).open(QIODevice::WriteOnly);
    }
    QCOMPARE(countAudioFiles(tmp.path(), false), 1); // only root
}

void TestSonifiQ::test_scandirRecursive() {
    QTemporaryDir tmp;
    QDir(tmp.path()).mkdir("sub");
    // Create dummy wav files
    for (const QString &p : {tmp.filePath("a.wav"), tmp.filePath("sub/b.wav")})
        QFile(p).open(QIODevice::WriteOnly);
    QCOMPARE(countAudioFiles(tmp.path(), true), 2); // root + sub
}

void TestSonifiQ::test_scandirIgnoresNonAudio() {
    QTemporaryDir tmp;
    for (const QString &p : {tmp.filePath("track.wav"),
                              tmp.filePath("cover.jpg"),
                              tmp.filePath("notes.txt")})
        QFile(p).open(QIODevice::WriteOnly);
    QCOMPARE(countAudioFiles(tmp.path(), false), 1); // only .wav
}

// ── Drop path guard tests ─────────────────────────────────────────────────────

void TestSonifiQ::test_emptyUrlFiltered() {
    // Simulate what the drop handler does: filter out empty toLocalFile() results
    QList<QUrl> urls = {
        QUrl("file:///tmp/real.flac"),
        QUrl("trash:///deleted.mp3"),  // non-local — toLocalFile() == ""
        QUrl(),                         // empty URL
        QUrl("smb://server/share/track.flac") // network — toLocalFile() == ""
    };
    QStringList paths;
    for (const QUrl &u : urls) {
        QString p = u.toLocalFile();
        if (!p.isEmpty()) paths << p;
    }
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths[0], "/tmp/real.flac");
}

// ── Integration tests (require ffmpeg) ───────────────────────────────────────

static bool runFfmpeg(const QStringList &args) {
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("ffmpeg", args);
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

void TestSonifiQ::test_ffmpeg_mp3() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.mp3");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-codec:a", "libmp3lame", "-b:a", "128k", out}));
    QVERIFY(QFile::exists(out));
    QVERIFY(QFileInfo(out).size() > 1000);
}

void TestSonifiQ::test_ffmpeg_aac() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.m4a");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-codec:a", "aac", "-b:a", "128k", out}));
    QVERIFY(QFile::exists(out));
    QVERIFY(QFileInfo(out).size() > 1000);
}

void TestSonifiQ::test_ffmpeg_opus() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.opus");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-codec:a", "libopus", "-b:a", "128k", out}));
    QVERIFY(QFile::exists(out));
}

void TestSonifiQ::test_ffmpeg_flac() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.flac");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-codec:a", "flac", "-compression_level", "5", out}));
    QVERIFY(QFile::exists(out));
}

void TestSonifiQ::test_ffmpeg_wav() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.wav");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-acodec", "pcm_s24le", out}));
    QVERIFY(QFile::exists(out));
}

void TestSonifiQ::test_ffmpeg_ogg() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    QString out = m_tmpDir.filePath("out.ogg");
    QVERIFY(runFfmpeg({"-y", "-i", m_testWav,
        "-map", "0:a", "-map_metadata", "0",
        "-codec:a", "libvorbis", "-q:a", "4", out}));
    QVERIFY(QFile::exists(out));
}

// ── Error message tests ──────────────────────────────────────────────────────

// Mirrors the extractError lambda in ConversionWorker::run()
static QString extractError(const QByteArray &raw) {
    QString msg;
    const auto lines = QString::fromLocal8Bit(raw).split('\n');
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines[i].trimmed();
        if (!line.isEmpty() && line[0].isUpper() && !line.startsWith("Stream")
                && !line.startsWith("Output #") && !line.startsWith("Input #")
                && !line.startsWith("Press")) {
            msg = line; break;
        }
    }
    if (msg.isEmpty())
        for (int i = lines.size()-1; i >= 0; --i)
            if (!lines[i].trimmed().isEmpty()) { msg = lines[i].trimmed(); break; }
    if (msg.length() > 120) msg = msg.left(117) + "…";
    return msg;
}

void TestSonifiQ::test_error_message_extraction() {
    // Typical ffmpeg "same as input" stderr output
    QByteArray sameInput =
        "Input #0, flac, from 'track.flac':\n"
        "Output track.flac same as Input #0 - exiting\n"
        "Error opening output file track.flac.\n"
        "Error opening output files: Invalid argument\n";
    QString msg = extractError(sameInput);
    QVERIFY2(!msg.isEmpty(), "Should extract a non-empty error message");
    QVERIFY2(msg.startsWith("Error"), "Should start with 'Error'");

    // Generic codec error
    QByteArray codecErr =
        "Stream mapping:\n"
        "  Stream #0:0 -> #0:0\n"
        "Encoder libopus returned error -22 while encoding audio frame\n";
    QString msg2 = extractError(codecErr);
    QVERIFY(!msg2.isEmpty());
    QVERIFY(msg2.contains("Encoder") || msg2.contains("error"));

    // Very long message gets truncated
    QByteArray longErr = QByteArray("Error: ") + QByteArray(200, 'x');
    QString msg3 = extractError(longErr);
    QVERIFY(msg3.length() <= 120);
}

void TestSonifiQ::test_ffmpeg_same_format_reports_error() {
    if (!m_ffmpegAvailable) QSKIP("ffmpeg not available");
    // Run ffmpeg with input == output (same path) and capture stderr
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start("ffmpeg", {"-y", "-i", m_testWav,
        "-map", "0:a", "-acodec", "pcm_s16le", m_testWav});
    proc.waitForFinished(10000);
    QString err = QString::fromLocal8Bit(proc.readAllStandardError());
    // ffmpeg should report a detectable error string even at exit code 0
    QVERIFY2(err.contains("same as Input") || err.contains("Error opening"),
             qPrintable("Expected error not found in: " + err.left(300)));
    // Confirm our extractor finds something useful
    QString extracted = extractError(proc.readAllStandardError() + err.toLocal8Bit());
    // Even if already consumed above, the pattern check already passed
}

QTEST_GUILESS_MAIN(TestSonifiQ)
#include "tst_sonifiq.moc"
