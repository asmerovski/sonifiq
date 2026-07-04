#include "thememanager.h"
#include <QApplication>
#include <QSettings>

// ── Built-in themes ────────────────────────────────────────────────────────
// Two dark, two light. Only colours differ between them — layout, spacing
// and the segmented total-progress bar / ffmpeg log pane are intentionally
// left untouched by theming (see mainwindow.cpp).

QList<Theme> ThemeManager::allThemes() {
    static const QList<Theme> themes = {
        // ── Dark (default) ──────────────────────────────────────────────────
        Theme{
            "dark", "Dark", true,
            QColor(0x23, 0x25, 0x2d),   // windowBg
            QColor(0x2a, 0x2d, 0x37),   // panelBg
            QColor(0x1c, 0x1e, 0x25),   // baseBg
            QColor(0xe8, 0xe9, 0xee),   // textColor
            QColor(0x90, 0x96, 0xab),   // mutedText
            QColor(0x3d, 0x41, 0x50),   // borderColor
            QColor(0x2f, 0x32, 0x3c),   // buttonBg
            QColor(0x3a, 0x3e, 0x4a),   // buttonHoverBg
            QColor(0x26, 0x29, 0x33),   // buttonPressedBg
            QColor(0x45, 0x4a, 0x5a),   // buttonBorder
            QColor(0x3d, 0x7a, 0x5c),   // primaryBg
            QColor(0x46, 0x91, 0x6d),   // primaryHoverBg
            QColor(0x33, 0x66, 0x4c),   // primaryPressedBg
            QColor(0xff, 0xff, 0xff),   // primaryText
            QColor(0x3d, 0x6f, 0xa5),   // selectionBg
            QColor(0x2a, 0x2d, 0x37),   // headerBg
        },
        // ── Midnight (deeper dark) ──────────────────────────────────────────
        Theme{
            "midnight", "Midnight", true,
            QColor(0x14, 0x15, 0x1a),
            QColor(0x1a, 0x1c, 0x22),
            QColor(0x10, 0x11, 0x14),
            QColor(0xd8, 0xda, 0xe2),
            QColor(0x7d, 0x82, 0x96),
            QColor(0x2a, 0x2d, 0x38),
            QColor(0x1e, 0x20, 0x28),
            QColor(0x26, 0x2a, 0x35),
            QColor(0x17, 0x18, 0x1d),
            QColor(0x33, 0x38, 0x4a),
            QColor(0x2f, 0x6b, 0x4d),
            QColor(0x38, 0x80, 0x5c),
            QColor(0x27, 0x59, 0x40),
            QColor(0xff, 0xff, 0xff),
            QColor(0x2c, 0x5a, 0x86),
            QColor(0x1a, 0x1c, 0x22),
        },
        // ── Light ────────────────────────────────────────────────────────────
        Theme{
            "light", "Light", false,
            QColor(0xf4, 0xf5, 0xf7),
            QColor(0xff, 0xff, 0xff),
            QColor(0xff, 0xff, 0xff),
            QColor(0x23, 0x26, 0x2f),
            QColor(0x67, 0x6c, 0x7c),
            QColor(0xd7, 0xda, 0xe0),
            QColor(0xff, 0xff, 0xff),
            QColor(0xee, 0xf0, 0xf4),
            QColor(0xe2, 0xe5, 0xeb),
            QColor(0xc9, 0xcc, 0xd6),
            QColor(0x3f, 0x9d, 0x72),
            QColor(0x35, 0x90, 0x67),
            QColor(0x2c, 0x7d, 0x59),
            QColor(0xff, 0xff, 0xff),
            QColor(0xcf, 0xe1, 0xf5),
            QColor(0xec, 0xee, 0xf2),
        },
        // ── Daylight (warm light) ────────────────────────────────────────────
        Theme{
            "daylight", "Daylight", false,
            QColor(0xfa, 0xf7, 0xf0),
            QColor(0xff, 0xff, 0xff),
            QColor(0xff, 0xfd, 0xf9),
            QColor(0x2b, 0x2a, 0x26),
            QColor(0x7a, 0x75, 0x68),
            QColor(0xe2, 0xdc, 0xcb),
            QColor(0xff, 0xfe, 0xfb),
            QColor(0xf2, 0xec, 0xdd),
            QColor(0xe8, 0xe0, 0xcc),
            QColor(0xd8, 0xcf, 0xb5),
            QColor(0xb5, 0x84, 0x2e),
            QColor(0xa5, 0x76, 0x2a),
            QColor(0x8f, 0x67, 0x24),
            QColor(0xff, 0xff, 0xff),
            QColor(0xf0, 0xdd, 0xb0),
            QColor(0xf3, 0xed, 0xe0),
        },
        // ── Pastel Blue (soft light, blue-tinted) ───────────────────────────
        Theme{
            "pastel_blue", "Pastel Blue", false,
            QColor(0xee, 0xf3, 0xfa),   // windowBg
            QColor(0xf8, 0xfb, 0xfe),   // panelBg
            QColor(0xff, 0xff, 0xff),   // baseBg
            QColor(0x24, 0x30, 0x3f),   // textColor
            QColor(0x6b, 0x7c, 0x93),   // mutedText
            QColor(0xc8, 0xd8, 0xea),   // borderColor
            QColor(0xff, 0xff, 0xff),   // buttonBg
            QColor(0xe3, 0xed, 0xf9),   // buttonHoverBg
            QColor(0xd3, 0xe2, 0xf2),   // buttonPressedBg
            QColor(0xb7, 0xcb, 0xe3),   // buttonBorder
            QColor(0x5a, 0x9b, 0xd8),   // primaryBg
            QColor(0x4d, 0x8c, 0xc9),   // primaryHoverBg
            QColor(0x3f, 0x78, 0xb0),   // primaryPressedBg
            QColor(0xff, 0xff, 0xff),   // primaryText
            QColor(0xcf, 0xe3, 0xf7),   // selectionBg
            QColor(0xe6, 0xee, 0xf8),   // headerBg
        },
        // ── Pastel Blue Dark (dark, same blue family as Pastel Blue) ────────
        Theme{
            "pastel_blue_dark", "Pastel Blue Dark", true,
            QColor(0x1b, 0x23, 0x30),   // windowBg
            QColor(0x21, 0x2b, 0x3a),   // panelBg
            QColor(0x16, 0x1d, 0x29),   // baseBg
            QColor(0xdb, 0xe6, 0xf3),   // textColor
            QColor(0x7c, 0x8f, 0xa8),   // mutedText
            QColor(0x33, 0x41, 0x5a),   // borderColor
            QColor(0x23, 0x2e, 0x40),   // buttonBg
            QColor(0x2b, 0x37, 0x50),   // buttonHoverBg
            QColor(0x1a, 0x23, 0x33),   // buttonPressedBg
            QColor(0x3c, 0x4c, 0x68),   // buttonBorder
            QColor(0x6f, 0xa8, 0xdc),   // primaryBg (pastel blue accent)
            QColor(0x82, 0xb7, 0xe4),   // primaryHoverBg
            QColor(0x5c, 0x96, 0xcc),   // primaryPressedBg
            QColor(0x16, 0x20, 0x2c),   // primaryText (dark, for contrast on the pastel accent)
            QColor(0x2f, 0x4a, 0x68),   // selectionBg
            QColor(0x21, 0x2b, 0x3a),   // headerBg
        },
    };
    return themes;
}

Theme ThemeManager::themeById(const QString &id) {
    for (const auto &t : allThemes())
        if (t.id == id) return t;
    return allThemes().first(); // "dark"
}

QString ThemeManager::currentThemeId() {
    QSettings s("SonifiQ", "SonifiQ");
    return s.value("appearance/theme", "dark").toString();
}

static QString c(const QColor &color) { return color.name(QColor::HexRgb); }

QString ThemeManager::buildStyleSheet(const Theme &t) {
    return QString(R"(
        QMainWindow, QDialog, QWidget {
            background-color: %windowBg;
            color: %textColor;
        }

        QLabel { color: %textColor; background: transparent; }
        QLabel#introLabel, QLabel#checkStatus, QLabel#logTitle { color: %mutedText; }

        QGroupBox {
            border: 1px solid %borderColor;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 8px;
            background-color: %panelBg;
            color: %textColor;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: %textColor;
        }

        QScrollArea { background: transparent; border: none; }

        QPushButton {
            background-color: %buttonBg;
            color: %textColor;
            border: 1px solid %buttonBorder;
            border-radius: 5px;
            padding: 6px 12px;
        }
        QPushButton:hover    { background-color: %buttonHoverBg; }
        QPushButton:pressed  { background-color: %buttonPressedBg; }
        QPushButton:disabled { color: %mutedText; border-color: %borderColor; }
        QPushButton:checked  { background-color: %buttonPressedBg; border-color: %primaryBg; }

        QPushButton#btnConvert {
            background-color: %primaryBg;
            color: %primaryText;
            border: 1px solid %primaryBg;
            font-weight: 600;
        }
        QPushButton#btnConvert:hover    { background-color: %primaryHoverBg; }
        QPushButton#btnConvert:pressed  { background-color: %primaryPressedBg; }
        QPushButton#btnConvert:disabled {
            background-color: %buttonBg; color: %mutedText; border-color: %borderColor;
        }

        QPushButton#btnPrimary {
            border: 1px solid %primaryBg;
        }
        QPushButton#btnPrimary:hover { background-color: %buttonHoverBg; }

        QLineEdit, QSpinBox, QComboBox {
            background-color: %baseBg;
            color: %textColor;
            border: 1px solid %borderColor;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
            color: %mutedText;
            background-color: %panelBg;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background-color: %baseBg;
            color: %textColor;
            border: 1px solid %borderColor;
            selection-background-color: %selectionBg;
            selection-color: %textColor;
        }

        QCheckBox { color: %textColor; spacing: 6px; }
        QCheckBox::indicator {
            width: 14px; height: 14px;
            border: 1px solid %borderColor;
            border-radius: 3px;
            background-color: %baseBg;
        }
        QCheckBox::indicator:checked { background-color: %primaryBg; border-color: %primaryBg; }
        QCheckBox:disabled { color: %mutedText; }

        QTableWidget {
            background-color: %baseBg;
            alternate-background-color: %headerBg;
            color: %textColor;
            gridline-color: %borderColor;
            border: 1px solid %borderColor;
            border-radius: 4px;
            selection-background-color: %selectionBg;
            selection-color: %textColor;
        }
        QTableWidget::item:selected { background-color: %selectionBg; color: %textColor; }
        QHeaderView::section {
            background-color: %headerBg;
            color: %textColor;
            border: none;
            border-right: 1px solid %borderColor;
            border-bottom: 1px solid %borderColor;
            padding: 4px 6px;
        }

        QListWidget {
            background-color: %baseBg;
            color: %textColor;
            border: 1px solid %borderColor;
            border-radius: 4px;
            alternate-background-color: %panelBg;
        }

        QDockWidget {
            color: %textColor;
        }
        QDockWidget::title {
            background-color: %panelBg;
            padding: 4px 8px;
            border-bottom: 1px solid %borderColor;
        }

        QTabWidget::pane {
            border: 1px solid %borderColor;
            border-radius: 4px;
            top: -1px;
        }
        QTabBar::tab {
            background-color: %buttonBg;
            color: %mutedText;
            border: 1px solid %borderColor;
            border-bottom: none;
            padding: 6px 14px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background-color: %panelBg;
            color: %textColor;
        }

        QMenu {
            background-color: %panelBg;
            color: %textColor;
            border: 1px solid %borderColor;
        }
        QMenu::item:selected { background-color: %selectionBg; color: %textColor; }

        QToolTip {
            background-color: %panelBg;
            color: %textColor;
            border: 1px solid %borderColor;
            padding: 3px 6px;
        }

        QDialogButtonBox QPushButton { min-width: 72px; }

        /* The ffmpeg log pane (main dock + per-file popup) always stays a
           fixed dark "terminal" look, independent of the active theme —
           its log-line colours (colourFor() in mainwindow.cpp) are tuned
           for a dark background and would lose contrast on light themes. */
        QPlainTextEdit#logView {
            background-color: #1a1c22;
            color: #d8dae2;
            border: 1px solid #2a2d38;
            border-radius: 4px;
        }
    )")
        .replace("%windowBg",          c(t.windowBg))
        .replace("%panelBg",           c(t.panelBg))
        .replace("%baseBg",            c(t.baseBg))
        .replace("%textColor",         c(t.textColor))
        .replace("%mutedText",         c(t.mutedText))
        .replace("%borderColor",       c(t.borderColor))
        .replace("%buttonBg",          c(t.buttonBg))
        .replace("%buttonHoverBg",     c(t.buttonHoverBg))
        .replace("%buttonPressedBg",   c(t.buttonPressedBg))
        .replace("%buttonBorder",      c(t.buttonBorder))
        .replace("%primaryBg",         c(t.primaryBg))
        .replace("%primaryHoverBg",    c(t.primaryHoverBg))
        .replace("%primaryPressedBg",  c(t.primaryPressedBg))
        .replace("%primaryText",       c(t.primaryText))
        .replace("%selectionBg",       c(t.selectionBg))
        .replace("%headerBg",          c(t.headerBg));
}

void ThemeManager::applyTheme(const QString &id) {
    const Theme t = themeById(id);
    if (auto *app = qApp) app->setStyleSheet(buildStyleSheet(t));

    QSettings s("SonifiQ", "SonifiQ");
    s.setValue("appearance/theme", t.id);
}

void ThemeManager::applySavedTheme() {
    applyTheme(currentThemeId());
}