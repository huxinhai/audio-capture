# Audio Capture

[中文文档](README_CN.md) | English

A cross-platform system audio capture tool that records all audio currently playing on your system (Loopback Recording). Supports **Windows** (WASAPI) and **macOS** (ScreenCaptureKit).

> **Note:** This project was previously named `audiotee-wasapi` and only supported Windows. The original Windows-only code is preserved on the [`audiotee-wasapi`](../../tree/audiotee-wasapi) branch. If you had the old repository URL bookmarked, GitHub will automatically redirect to this new location.

## Features

| Feature | Windows | macOS |
|---------|:-------:|:-----:|
| Capture system audio (all playing sounds) | ✅ | ✅ |
| Real-time audio format conversion (sample rate, channels, bit depth) | ✅ | ✅ |
| Custom sample rates (8000 - 192000 Hz) | ✅ | ✅ |
| Custom channels (1-8) | ✅ | ✅ |
| Custom bit depth (16/24/32 bits) | ✅ | ✅ |
| Output raw PCM audio data to stdout | ✅ | ✅ |
| Ctrl+C graceful exit | ✅ | ✅ |
| Event-driven mode (low latency) | ✅ | ✅ |
| Custom buffer size | ✅ | ✅ |
| Mute mode | 📝 | 📝 |
| Process filtering | 📝 | 📝 |

## System Requirements

### Windows

- **OS**: Windows 10/11 (tested)
- **Compiler**: Visual Studio 2017+ (2022 recommended) or any MSVC compiler with C++17 support
- **Build Tools**: CMake 3.15+

### macOS

- **OS**: macOS 14 (Sonoma) or later
- **Permission**: Screen Recording permission (required by ScreenCaptureKit for audio capture)
- **Compiler**: Xcode Command Line Tools (Clang with Objective-C++ support)
- **Build Tools**: CMake 3.15+

## Download Pre-built Binaries

Download the latest pre-built binaries from [GitHub Releases](https://github.com/huxinhai/audio-capture/releases):

- `audio_capture-windows-x64` — Windows x64
- `audio_capture-macos-arm64` — macOS Apple Silicon (M1/M2/M3/M4)
- `audio_capture-macos-x86_64` — macOS Intel

No compilation needed — just download and run.

## Build Instructions

### macOS

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

The binary will be at `build/bin/audio_capture`.

### Windows (CMake + Visual Studio)

```batch
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

The binary will be at `build\bin\Release\audio_capture.exe`.

### Windows (cl.exe Direct Compilation)

Open "x64 Native Tools Command Prompt for VS 2022" and run:

```batch
scripts\build_simple.bat
```

## Usage

### Basic Usage

```bash
# Capture system audio and save as raw PCM file
audio_capture > output.pcm

# Pipe to FFmpeg to convert to MP3
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 output.mp3
```

### Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `--sample-rate <Hz>` | Target sample rate (8000-192000 Hz, default: device default) | `--sample-rate 16000` |
| `--channels <count>` | Number of channels (1-8, default: device default) | `--channels 1` |
| `--bit-depth <bits>` | Bit depth: 16, 24, or 32 (default: device default) | `--bit-depth 16` |
| `--chunk-duration <seconds>` | Audio chunk duration (default: 0.2s) | `--chunk-duration 0.1` |
| `--mute` | Mute system audio while capturing (not yet implemented) | `--mute` |
| `--include-processes <PID>` | Only capture from specified PIDs (not yet implemented) | `--include-processes 1234` |
| `--exclude-processes <PID>` | Exclude specified PIDs (not yet implemented) | `--exclude-processes 1234` |
| `--help` / `-h` | Show help message | `--help` |

### Examples

#### Speech Recognition (16kHz mono)

```bash
audio_capture --sample-rate 16000 --channels 1 --bit-depth 16 > speech.pcm
```

#### CD Quality (44.1kHz stereo)

```bash
audio_capture --sample-rate 44100 --channels 2 --bit-depth 16 > music.pcm
```

#### Real-time Streaming to FFmpeg

```bash
# WAV
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 output.wav

# MP3
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 -b:a 192k output.mp3

# FLAC
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 -c:a flac output.flac
```

#### Low Latency / High Stability

```bash
# Lower latency
audio_capture --chunk-duration 0.05 > output.pcm

# Higher stability
audio_capture --chunk-duration 0.5 > output.pcm
```

### Audio Format

The program outputs **raw PCM audio data** (no file header) to stdout. Status and error messages go to stderr.

**Default behavior** (no format options): uses device native format, converted to 16-bit PCM integer.

**Custom format** (with `--sample-rate`, `--channels`, `--bit-depth`):
- Sample Rate: 8000-192000 Hz
- Channels: 1 (mono) to 8
- Bit Depth: 16/24/32 bits

**FFmpeg parameter mapping:**

| Output Format | FFmpeg Parameters |
|---------------|-------------------|
| 16kHz, Mono, 16-bit | `-f s16le -ar 16000 -ac 1` |
| 44.1kHz, Stereo, 16-bit | `-f s16le -ar 44100 -ac 2` |
| 48kHz, Stereo, 24-bit | `-f s24le -ar 48000 -ac 2` |
| 48kHz, Stereo, 32-bit | `-f s32le -ar 48000 -ac 2` |

## Troubleshooting

### macOS

#### "No audio device found" or capture fails silently

1. Open **System Settings > Privacy & Security > Screen Recording**
2. Add your terminal app (Terminal.app, iTerm2, etc.) to the allowed list
3. Restart the terminal app after granting permission

#### No audio captured

- Ensure system audio is playing
- Check that system volume is not muted
- Verify macOS version is 14.0 or later: `sw_vers`

### Windows

#### "No audio device found"

1. Right-click the volume icon in the taskbar
2. Select "Open Sound settings"
3. Ensure an output device is available and not disabled

#### "Access denied"

- Run as Administrator
- Check Windows Privacy Settings > Microphone access permissions

#### "Audio device is in use"

1. Close applications that exclusively use the audio device
2. Open Sound Settings > Device properties > Additional device properties
3. Go to "Advanced" tab, uncheck "Allow applications to take exclusive control of this device"

#### Audio stuttering or frame drops

- Increase buffer size: `--chunk-duration 0.5`
- Close other CPU-intensive programs

### General

#### No audio captured (output file is empty)

- Ensure system is playing audio
- Check system volume is not at 0
- Verify default playback device settings

#### Format errors with FFmpeg

Run the program to view the actual output format (printed to stderr), then match FFmpeg parameters accordingly.

## Project Structure

```
audio-capture/
├── .github/
│   └── workflows/
│       └── build-cross-platform.yml  # CI: Windows + macOS builds
├── src/
│   ├── common/
│   │   └── common.h                  # Shared types, CLI parsing
│   ├── mac/
│   │   ├── system_audio_capture.h    # macOS capture interface
│   │   ├── system_audio_capture.mm   # ScreenCaptureKit implementation
│   │   ├── audio_resampler.h         # AudioToolbox resampler
│   │   └── error_handler.h           # macOS error diagnostics
│   ├── windows/
│   │   ├── wasapi_capture.h          # Windows capture interface
│   │   ├── wasapi_capture.cpp        # WASAPI implementation
│   │   ├── audio_resampler.h         # Media Foundation resampler
│   │   └── error_handler.h           # Windows error diagnostics
│   └── main.cpp                      # Entry point (platform dispatch)
├── scripts/
│   ├── build.bat                     # Windows CMake build script
│   └── build_simple.bat              # Windows cl.exe direct build
├── CMakeLists.txt                    # Cross-platform CMake config
├── README.md                         # This file (English)
└── README_CN.md                      # Chinese documentation
```

## Automated Build and Release

This project uses GitHub Actions for cross-platform CI/CD. Builds are triggered on push to `main` or `dev` branches.

Artifacts produced:
- `audio_capture-windows-x64` (Windows x64)
- `audio_capture-macos-arm64` (macOS ARM64)
- `audio_capture-macos-x86_64` (macOS Intel)

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | System initialization failed |
| 2 | No audio device found |
| 3 | Device access denied |
| 4 | Unsupported audio format |
| 5 | Insufficient buffer |
| 6 | Device in use |
| 7 | Driver error |
| 8 | Invalid parameter |
| 99 | Unknown error |

## License

This project is open source and free to use and modify.

## Contributing

Issues and Pull Requests are welcome!

## Related Links

- [Windows WASAPI Documentation](https://docs.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- [Apple ScreenCaptureKit Documentation](https://developer.apple.com/documentation/screencapturekit)
- [FFmpeg](https://ffmpeg.org/)
- [Visual Studio](https://visualstudio.microsoft.com/)
- [CMake](https://cmake.org/download/)

## Changelog

### v2.0

- ✅ Cross-platform support (Windows + macOS)
- ✅ macOS audio capture via ScreenCaptureKit
- ✅ Unified CLI interface across platforms
- ✅ Cross-platform CI/CD (GitHub Actions)
- ✅ Refactored architecture (platform-specific modules)

### v1.0

- ✅ Windows WASAPI audio capture
- ✅ Custom sample rates and buffer sizes
- ✅ Event-driven and polling modes
- ✅ Detailed error diagnostic system
- ✅ Raw PCM output to stdout
