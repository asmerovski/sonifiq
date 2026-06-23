# SonifiQ

A fast, multi-threaded audio format converter for Linux, built with Qt6 and powered by ffmpeg.

![SonifiQ screenshot](sonifiq.svg)

---

## Features

- **Multi-threaded** — configurable parallel conversion (up to your CPU core count)
- **Per-file progress bar** — real-time progress via `ffmpeg -progress pipe:1`
- **Output formats** — MP3, AAC (.m4a), Ogg Vorbis, Opus, FLAC, WAV (PCM), AIFF
- **Quality presets** — CBR/VBR bitrates, compression levels per format
- **Tag preservation** — copies ID3/Vorbis tags and cover art
- **Drag & drop** — files and folders, with optional subfolder recursion
- **Settings dialog** — enable/disable individual formats, codec availability precheck
- **ffmpeg log pane** — dockable, hidable, colour-coded output from ffmpeg
- **Dark theme UI**

---

## Requirements

| Dependency | Version | Install (Arch) | Install (Ubuntu/Debian) |
|------------|---------|----------------|-------------------------|
| Qt6 Widgets | 6.2+   | `qt6-base`     | `qt6-base-dev`          |
| CMake       | 3.16+  | `cmake`        | `cmake`                 |
| ffmpeg      | 4.0+   | `ffmpeg`       | `ffmpeg`                |
| ffprobe     | (bundled with ffmpeg) | — | — |

---

## Build from source

```bash
# Arch / CachyOS / Manjaro
sudo pacman -S qt6-base cmake ninja ffmpeg

# Ubuntu 22.04+
sudo apt-get install qt6-base-dev libgl-dev libxkbcommon-dev cmake ninja-build ffmpeg

# Build
git clone https://github.com/YOUR_USERNAME/sonifiq.git
cd sonifiq
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sonifiq
```
---

## Install (system-wide)

```bash
cmake --install build --prefix /usr/local
# Binary  → /usr/local/bin/sonifiq
# Icon    → /usr/local/share/icons/hicolor/scalable/apps/sonifiq.svg
# Launcher→ /usr/local/share/applications/sonifiq.desktop
```

---
## License

GPL V3
