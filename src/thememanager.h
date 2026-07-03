#pragma once
#include <QString>
#include <QColor>
#include <QList>

// A theme is purely a set of colours — no layout, sizing, or icon changes.
// Everything else in the UI (fonts, spacing, the segmented progress bar,
// the "terminal-style" ffmpeg log pane) stays fixed across themes.
struct Theme {
    QString id;       // stored in QSettings, e.g. "dark"
    QString name;      // shown in the theme picker, e.g. "Dark"
    bool    isDark = true;

    QColor windowBg;        // main window / dialog background
    QColor panelBg;         // sidebar / group box / dock background
    QColor baseBg;          // input fields, table background
    QColor textColor;       // primary text
    QColor mutedText;       // secondary/hint text
    QColor borderColor;     // borders on buttons, inputs, group boxes, table
    QColor buttonBg;
    QColor buttonHoverBg;
    QColor buttonPressedBg;
    QColor buttonBorder;
    QColor primaryBg;       // accent action (e.g. Convert)
    QColor primaryHoverBg;
    QColor primaryPressedBg;
    QColor primaryText;
    QColor selectionBg;     // selected table rows / focus highlight
    QColor headerBg;        // table header background
};

class ThemeManager {
public:
    static QList<Theme> allThemes();
    static Theme themeById(const QString &id);

    // Reads the persisted theme id from QSettings (defaults to "dark").
    static QString currentThemeId();

    // Builds the app-wide stylesheet for `id`, applies it via
    // qApp->setStyleSheet(), and persists the choice to QSettings.
    static void applyTheme(const QString &id);

    // Convenience: applyTheme(currentThemeId()). Call once at startup.
    static void applySavedTheme();

private:
    static QString buildStyleSheet(const Theme &t);
};
