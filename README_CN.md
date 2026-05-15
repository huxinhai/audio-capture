# Audio Capture

中文文档 | [English](README.md)

跨平台系统音频捕获工具，可以录制系统正在播放的所有音频（Loopback Recording）。支持 **Windows**（WASAPI）和 **macOS**（ScreenCaptureKit）。

> **说明：** 本项目原名 `audiotee-wasapi`，仅支持 Windows。原始 Windows 专用代码保留在 [`audiotee-wasapi`](../../tree/audiotee-wasapi) 分支。如果你之前收藏了旧仓库地址，GitHub 会自动重定向到新地址。

## 功能特性

| 功能 | Windows | macOS |
|------|:-------:|:-----:|
| 捕获系统音频（所有正在播放的声音） | ✅ | ✅ |
| 实时音频格式转换（采样率、声道数、位深） | ✅ | ✅ |
| 自定义采样率（8000 - 192000 Hz） | ✅ | ✅ |
| 自定义声道数（1-8） | ✅ | ✅ |
| 自定义位深（16/24/32 位） | ✅ | ✅ |
| 输出原始 PCM 音频数据到标准输出 | ✅ | ✅ |
| Ctrl+C 优雅退出 | ✅ | ✅ |
| 事件驱动模式（低延迟） | ✅ | ✅ |
| 自定义缓冲区大小 | ✅ | ✅ |
| 静音模式 | 📝 | 📝 |
| 进程过滤 | 📝 | 📝 |

## 系统要求

### Windows

- **操作系统**: Windows 10/11（已测试通过）
- **编译器**: Visual Studio 2017+（推荐 2022）或任何支持 C++17 的 MSVC 编译器
- **构建工具**: CMake 3.15+

### macOS

- **操作系统**: macOS 14 (Sonoma) 或更高版本
- **权限**: 需要屏幕录制权限（ScreenCaptureKit 捕获音频需要此权限）
- **编译器**: Xcode Command Line Tools（支持 Objective-C++ 的 Clang）
- **构建工具**: CMake 3.15+

## 下载预编译版本

从 [GitHub Releases](https://github.com/huxinhai/audio-capture/releases) 下载最新预编译文件：

- `audio_capture-windows-x64` — Windows x64
- `audio_capture-macos-arm64` — macOS Apple Silicon (M1/M2/M3/M4)
- `audio_capture-macos-x86_64` — macOS Intel

无需编译，下载即可运行。

## 编译方法

### macOS

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

生成的可执行文件位于 `build/bin/audio_capture`。

### Windows（CMake + Visual Studio）

```batch
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

生成的可执行文件位于 `build\bin\Release\audio_capture.exe`。

### Windows（cl.exe 直接编译）

打开 "x64 Native Tools Command Prompt for VS 2022"，运行：

```batch
scripts\build_simple.bat
```

## 使用说明

### 基本用法

```bash
# 捕获系统音频并保存为原始 PCM 文件
audio_capture > output.pcm

# 通过管道传递给 FFmpeg 转换为 MP3
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 output.mp3
```

### 命令行参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `--sample-rate <Hz>` | 目标采样率（8000-192000 Hz，默认：设备默认值） | `--sample-rate 16000` |
| `--channels <数量>` | 声道数（1-8，默认：设备默认值） | `--channels 1` |
| `--bit-depth <位数>` | 位深：16、24 或 32（默认：设备默认值） | `--bit-depth 16` |
| `--chunk-duration <秒>` | 音频块持续时间（默认：0.2 秒） | `--chunk-duration 0.1` |
| `--mute` | 捕获时静音系统音频（暂未实现） | `--mute` |
| `--include-processes <PID>` | 只捕获指定进程的音频（暂未实现） | `--include-processes 1234` |
| `--exclude-processes <PID>` | 排除指定进程的音频（暂未实现） | `--exclude-processes 1234` |
| `--help` / `-h` | 显示帮助信息 | `--help` |

### 使用示例

#### 语音识别（16kHz 单声道）

```bash
audio_capture --sample-rate 16000 --channels 1 --bit-depth 16 > speech.pcm
```

#### CD 音质（44.1kHz 立体声）

```bash
audio_capture --sample-rate 44100 --channels 2 --bit-depth 16 > music.pcm
```

#### 实时流式转换到 FFmpeg

```bash
# WAV
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 output.wav

# MP3
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 -b:a 192k output.mp3

# FLAC
audio_capture 2>/dev/null | ffmpeg -f s16le -ar 48000 -ac 2 -i pipe:0 -c:a flac output.flac
```

#### 低延迟 / 高稳定性

```bash
# 更低延迟
audio_capture --chunk-duration 0.05 > output.pcm

# 更高稳定性
audio_capture --chunk-duration 0.5 > output.pcm
```

### 音频格式

程序输出**原始 PCM 音频数据**（无文件头）到标准输出。状态和错误信息输出到标准错误。

**默认行为**（不指定格式参数）：使用设备原生格式，自动转换为 16 位 PCM 整数。

**自定义格式**（使用 `--sample-rate`、`--channels`、`--bit-depth`）：
- 采样率: 8000-192000 Hz
- 声道数: 1（单声道）到 8
- 位深: 16/24/32 位

**FFmpeg 参数对照表：**

| 输出格式 | FFmpeg 参数 |
|---------|------------|
| 16kHz, 单声道, 16位 | `-f s16le -ar 16000 -ac 1` |
| 44.1kHz, 立体声, 16位 | `-f s16le -ar 44100 -ac 2` |
| 48kHz, 立体声, 24位 | `-f s24le -ar 48000 -ac 2` |
| 48kHz, 立体声, 32位 | `-f s32le -ar 48000 -ac 2` |

## 常见问题

### macOS

#### "No audio device found" 或捕获无声

1. 打开 **系统设置 > 隐私与安全性 > 屏幕录制**
2. 将你的终端应用（Terminal.app、iTerm2 等）添加到允许列表
3. 授权后重启终端应用

#### 没有捕获到音频

- 确保系统正在播放音频
- 检查系统音量是否静音
- 确认 macOS 版本为 14.0 或更高：`sw_vers`

### Windows

#### "No audio device found"

1. 右键点击任务栏的音量图标
2. 选择"打开声音设置"
3. 确保有可用的输出设备且未被禁用

#### "Access denied"

- 以管理员身份运行程序
- 检查 Windows 隐私设置 > 麦克风访问权限

#### "Audio device is in use"

1. 关闭独占使用音频设备的程序
2. 打开声音设置 > 设备属性 > 其他设备属性
3. 进入"高级"选项卡，取消勾选"允许应用程序独占控制此设备"

#### 音频断断续续或丢帧

- 增大缓冲区：`--chunk-duration 0.5`
- 关闭其他占用 CPU 的程序

### 通用

#### 没有捕获到音频（输出文件为空）

- 确保系统正在播放音频
- 检查系统音量是否为 0
- 验证默认播放设备设置是否正确

#### 与 FFmpeg 配合使用时出现格式错误

运行程序查看实际输出格式（输出到 stderr），然后相应调整 FFmpeg 参数。

## 项目结构

```
audio-capture/
├── .github/
│   └── workflows/
│       └── build-cross-platform.yml  # CI: Windows + macOS 构建
├── src/
│   ├── common/
│   │   └── common.h                  # 共享类型、CLI 解析
│   ├── mac/
│   │   ├── system_audio_capture.h    # macOS 捕获接口
│   │   ├── system_audio_capture.mm   # ScreenCaptureKit 实现
│   │   ├── audio_resampler.h         # AudioToolbox 重采样器
│   │   └── error_handler.h           # macOS 错误诊断
│   ├── windows/
│   │   ├── wasapi_capture.h          # Windows 捕获接口
│   │   ├── wasapi_capture.cpp        # WASAPI 实现
│   │   ├── audio_resampler.h         # Media Foundation 重采样器
│   │   └── error_handler.h           # Windows 错误诊断
│   └── main.cpp                      # 入口（平台分发）
├── scripts/
│   ├── build.bat                     # Windows CMake 构建脚本
│   └── build_simple.bat              # Windows cl.exe 直接编译脚本
├── CMakeLists.txt                    # 跨平台 CMake 配置
├── README.md                         # 英文文档
└── README_CN.md                      # 本文件（中文）
```

## 自动构建和发布

本项目使用 GitHub Actions 进行跨平台 CI/CD。推送到 `main` 或 `dev` 分支时自动触发构建。

构建产物：
- `audio_capture-windows-x64`（Windows x64）
- `audio_capture-macos-arm64`（macOS ARM64）
- `audio_capture-macos-x86_64`（macOS Intel）

## 退出代码

| 代码 | 含义 |
|------|------|
| 0 | 成功 |
| 1 | 系统初始化失败 |
| 2 | 未找到音频设备 |
| 3 | 设备访问被拒绝 |
| 4 | 不支持的音频格式 |
| 5 | 缓冲区不足 |
| 6 | 设备被占用 |
| 7 | 驱动错误 |
| 8 | 无效参数 |
| 99 | 未知错误 |

## 许可证

本项目为开源项目，可自由使用和修改。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 相关链接

- [Windows WASAPI 官方文档](https://docs.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- [Apple ScreenCaptureKit 文档](https://developer.apple.com/documentation/screencapturekit)
- [FFmpeg 官方网站](https://ffmpeg.org/)
- [Visual Studio 下载](https://visualstudio.microsoft.com/)
- [CMake 下载](https://cmake.org/download/)

## 更新日志

### v2.0

- ✅ 跨平台支持（Windows + macOS）
- ✅ macOS 音频捕获（基于 ScreenCaptureKit）
- ✅ 统一的跨平台 CLI 接口
- ✅ 跨平台 CI/CD（GitHub Actions）
- ✅ 重构架构（平台特定模块）

### v1.0

- ✅ Windows WASAPI 音频捕获
- ✅ 支持自定义采样率和缓冲区大小
- ✅ 事件驱动和轮询两种模式
- ✅ 详细的错误诊断系统
- ✅ 支持原始 PCM 输出到标准输出
