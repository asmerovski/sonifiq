cmake_minimum_required(VERSION 3.16)

# Version can be overridden at configure time by the release workflow:
#   cmake -DSONIFIQ_VERSION=1.2.3 ...
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

# Improve reproducibility for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
endif()

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(sonifiq
    main.cpp
    mainwindow.cpp
    mainwindow.h
    settingsdialog.cpp
    settingsdialog.h
    res.qrc

)

target_link_libraries(sonifiq PRIVATE Qt6::Widgets)

target_compile_definitions(sonifiq PRIVATE
    SONIFIQ_VERSION_STRING="${PROJECT_VERSION}"
)

# ── Install targets ───────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS sonifiq
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES res/icons/app.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME sonifiq.svg
)

install(FILES sonifiq.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# ── Packaging (CPack) ─────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "sonifiq")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT           "asmerovski@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "${PROJECT_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# DEB
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "asmerovki@gmail.com")
set(CPACK_DEBIAN_PACKAGE_SECTION    "sound")
set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libqt6widgets6, ffmpeg")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE       "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP         "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES      "qt6-qtbase, ffmpeg")

include(CPack)

