#pragma once
#include <QString>
#include <QStringList>
#include <QColor>
#include <QList>
#include <QJsonObject>

// A theme is purely a set of colours — no layout, sizing, or icon changes.
// Everything else in the UI (fonts, spacing, the segmented progress bar,
// the "terminal-style" ffmpeg log pane) stays fixed across themes.
//
// Themes live on disk as individual JSON files (one file per theme) under
// ThemeManager::themesDirPath(), so users can edit built-in themes or drop
// in their own without touching the binary. See Theme::toJson()/fromJson().
struct Theme {
    QString id;       // stored in QSettings, e.g. "dark"; also the filename stem
    QString name;      // shown in the theme picker, e.g. "Dark"
    bool    isDark = true;
    int     order  = 500; // controls position in the theme picker; built-ins use 0-99

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

    QJsonObject toJson() const;
    // Parses a theme JSON object. Sets *ok=false (and returns a partially
    // filled Theme) if required fields are missing or a colour fails to
    // parse, so the caller can skip malformed files instead of crashing.
    static Theme fromJson(const QJsonObject &obj, bool *ok = nullptr);
};

class ThemeManager {
public:
    static QList<Theme> allThemes();
    static Theme themeById(const QString &id);

    // Reads the persisted theme id from QSettings (defaults to "dark").
    static QString currentThemeId();

    // Builds the app-wide stylesheet for `id`, applies it via
    // qApp->setStyleSheet(), and persists the choice to QSettings.
    // No-op visually (still persists the id) when themingEnabled() is false —
    // the app follows the system look until theming is switched back on.
    static void applyTheme(const QString &id);

    // Convenience: applyTheme(currentThemeId()), or clears the stylesheet to
    // follow the system look if themingEnabled() is false. Call once at startup.
    static void applySavedTheme();

    // Whether SonifiQ applies a custom theme at all. When false, the app
    // uses Qt's default style and palette for the platform — i.e. whatever
    // the OS/desktop's own light/dark setting normally gives a Qt app.
    // Persisted in QSettings ("appearance/themingEnabled"), defaults to true.
    static bool themingEnabled();

    // Toggles theming on/off and immediately applies the result: re-applies
    // currentThemeId()'s stylesheet if enabling, clears the stylesheet
    // (follow system look) if disabling.
    static void setThemingEnabled(bool enabled);

    // Directory holding one *.json file per theme, e.g.
    // ~/.local/share/SonifiQ/themes on Linux. Created and seeded with the
    // built-in themes on first run (only if empty) so there are working
    // examples to copy/edit. Exposed so the UI can offer to open it.
    static QString themesDirPath();

    // Ids of built-in themes that currently have no corresponding file in
    // themesDirPath() (e.g. the user deleted "dark.json"). Themes that exist
    // but were edited are NOT considered missing — only ones absent entirely.
    static QStringList missingBuiltInThemeIds();

    // (Re)writes a JSON file for every missing built-in theme, leaving any
    // existing files (including edited built-ins and custom themes) alone.
    // Returns how many were restored.
    static int restoreMissingBuiltInThemes();

private:
    static QString buildStyleSheet(const Theme &t);
    static QList<Theme> builtInThemes();      // hard-coded seed/fallback set
    static QList<Theme> loadThemesFromDisk();
    static void ensureThemesDirSeeded();
};