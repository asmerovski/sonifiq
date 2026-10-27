cmake_minimum_required(VERSION 3.16)

# Version can be overridden at configure time by the release workflow:
#   cmake -DSONIFIQ_VERSION=1.2.3 ..
if(NOT DEFINED SONIFIQ_VERSION)
    set(SONIFIQ_VERSION "1.0.0")
endif()

project(SonifiQ
    VERSION     ${SONIFIQ_VERSION}
    DESCRIPTION "Qt6 audio format converter for Linux"
    LANGUAGES   CXX
)

# ── Build settings ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
endif()

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(sonifiq
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/res.qrc
)

target_link_libraries(sonifiq PRIVATE Qt6::Widgets)

target_compile_definitions(sonifiq PRIVATE
    SONIFIQ_VERSION_STRING="${PROJECT_VERSION}"
)

# Strip release binaries
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET sonifiq POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:sonifiq>
        COMMENT "Stripping sonifiq"
    )
endif()

# ── Install targets ───────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS sonifiq
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_SOURCE_DIR}/src/res/icons/app.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME sonifiq.svg
)

install(FILES ${CMAKE_SOURCE_DIR}/sonifiq.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Debian doc files (copyright + changelog)
# Use lowercase package name explicitly — CMAKE_INSTALL_DOCDIR resolves to
# share/doc/SonifiQ (PROJECT_NAME) but lintian requires share/doc/sonifiq.
install(FILES
    ${CMAKE_SOURCE_DIR}/packaging/debian/copyright
    ${CMAKE_SOURCE_DIR}/packaging/debian/changelog.gz
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/sonifiq
)

# ── Packaging (CPack) ─────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "sonifiq")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT           "asmerovski@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
"SonifiQ is a Qt6 audio format converter for Linux.
It uses the ffmpeg command-line tool as a processing back-end
to convert between common audio formats: FLAC, MP3, OGG, OPUS, WAV.")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# DEB ─────────────────────────────────────────────────────────────────────────
# Maintainer format must be "Display Name <email>" for lintian no-phrase
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "asmerovski <asmerovski@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION    "sound")
# libc6: satisfies lintian missing-dependency-on-libc
# libqt6widgets6: virtual package provided by libqt6widgets6t64 on Ubuntu 24.04
set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libc6, libqt6widgets6, ffmpeg")

# RPM ─────────────────────────────────────────────────────────────────────────
set(CPACK_RPM_PACKAGE_LICENSE       "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP         "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_VENDOR        "AsmerM")
# ffmpeg is in RPM Fusion, not the base Fedora repo — use Requires for the
# Qt runtime (always available) and Recommends for ffmpeg so the package
# installs cleanly on stock Fedora; users who lack ffmpeg get a runtime error.
set(CPACK_RPM_PACKAGE_REQUIRES      "qt6-qtbase, /usr/bin/ffmpeg")
set(CPACK_RPM_PACKAGE_URL           "https://github.com/asmerovski/sonifiq")
set(CPACK_RPM_PACKAGE_PACKAGER      "AsmerM <asmerovski@gmail.com>")
set(CPACK_RPM_PACKAGE_DESCRIPTION   "${CPACK_PACKAGE_DESCRIPTION}")
# Exclude dirs owned by filesystem/shared-mime-info to avoid rpmlint E
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/share/applications"
    "/usr/share/icons"
    "/usr/share/icons/hicolor"
    "/usr/share/icons/hicolor/scalable"
    "/usr/share/icons/hicolor/scalable/apps"
)

include(CPack)