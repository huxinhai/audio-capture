#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstring>
#include <csignal>

// ============================================================
// Error codes (shared across Windows and macOS)
// ============================================================
enum class ErrorCode {
    SUCCESS = 0,
    SYSTEM_INIT_FAILED = 1,    // COM_INIT_FAILED on Windows
    NO_AUDIO_DEVICE = 2,
    DEVICE_ACCESS_DENIED = 3,
    AUDIO_FORMAT_NOT_SUPPORTED = 4,
    INSUFFICIENT_BUFFER = 5,
    DEVICE_IN_USE = 6,
    DRIVER_ERROR = 7,
    INVALID_PARAMETER = 8,
    UNKNOWN_ERROR = 99
};

// ============================================================
// Capture configuration (shared across platforms)
// ============================================================
struct CaptureConfig {
    int sampleRate = 0;       // 0 = use device default
    int channels = 0;         // 0 = use device default
    int bitDepth = 0;         // 0 = use device default
    double chunkDuration = 0.2;
    bool mute = false;
    std::vector<uint32_t> includeProcesses;
    std::vector<uint32_t> excludeProcesses;
};

// ============================================================
// CLI argument parsing (shared across platforms)
// ============================================================
inline void PrintUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --sample-rate <Hz>           Target sample rate (default: device default)\n"
              << "  --channels <count>           Number of channels (default: device default)\n"
              << "  --bit-depth <bits>           Bit depth: 16, 24, or 32 (default: device default)\n"
              << "  --chunk-duration <seconds>   Duration of each audio chunk (default: 0.2)\n"
              << "  --mute                       Mute system audio while capturing\n"
              << "  --include-processes <pid>... Only capture audio from these process IDs\n"
              << "  --exclude-processes <pid>... Exclude audio from these process IDs\n"
              << "  --help                       Show this help message\n"
              << "\nExamples:\n"
              << "  " << programName << " --sample-rate 48000 --channels 2 --bit-depth 16\n"
              << "  " << programName << " --sample-rate 44100\n"
              << "  " << programName << " --channels 1 --bit-depth 24\n"
              << std::endl;
}

// Returns ErrorCode::SUCCESS on success, or an error code.
// On --help, returns -1 (caller should exit with 0).
inline int ParseArguments(int argc, char* argv[], CaptureConfig& config) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return -1;
        }
        else if (arg == "--sample-rate") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: --sample-rate requires a value" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
            try {
                int rate = std::stoi(argv[++i]);
                if (rate < 8000 || rate > 192000) {
                    std::cerr << "ERROR: Sample rate out of range: " << rate << std::endl;
                    std::cerr << "Valid range: 8000 - 192000 Hz" << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
                config.sampleRate = rate;
            } catch (const std::exception&) {
                std::cerr << "ERROR: Invalid sample rate value: " << argv[i] << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else if (arg == "--channels") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: --channels requires a value" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
            try {
                int ch = std::stoi(argv[++i]);
                if (ch < 1 || ch > 8) {
                    std::cerr << "ERROR: Channel count out of range: " << ch << std::endl;
                    std::cerr << "Valid range: 1 - 8 channels" << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
                config.channels = ch;
            } catch (const std::exception&) {
                std::cerr << "ERROR: Invalid channel count value: " << argv[i] << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else if (arg == "--bit-depth") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: --bit-depth requires a value" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
            try {
                int bits = std::stoi(argv[++i]);
                if (bits != 16 && bits != 24 && bits != 32) {
                    std::cerr << "ERROR: Invalid bit depth: " << bits << std::endl;
                    std::cerr << "Valid values: 16, 24, 32 bits" << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
                config.bitDepth = bits;
            } catch (const std::exception&) {
                std::cerr << "ERROR: Invalid bit depth value: " << argv[i] << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else if (arg == "--chunk-duration") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: --chunk-duration requires a value" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
            try {
                double duration = std::stod(argv[++i]);
                if (duration < 0.01 || duration > 10.0) {
                    std::cerr << "ERROR: Chunk duration out of range: " << duration << std::endl;
                    std::cerr << "Valid range: 0.01 - 10.0 seconds" << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
                config.chunkDuration = duration;
            } catch (const std::exception&) {
                std::cerr << "ERROR: Invalid chunk duration value: " << argv[i] << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else if (arg == "--mute") {
            config.mute = true;
        }
        else if (arg == "--include-processes") {
            bool foundAny = false;
            while (i + 1 < argc && isdigit(argv[i + 1][0])) {
                try {
                    config.includeProcesses.push_back((uint32_t)std::stoul(argv[++i]));
                    foundAny = true;
                } catch (const std::exception&) {
                    std::cerr << "ERROR: Invalid process ID: " << argv[i] << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
            }
            if (!foundAny) {
                std::cerr << "ERROR: --include-processes requires at least one process ID" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else if (arg == "--exclude-processes") {
            bool foundAny = false;
            while (i + 1 < argc && isdigit(argv[i + 1][0])) {
                try {
                    config.excludeProcesses.push_back((uint32_t)std::stoul(argv[++i]));
                    foundAny = true;
                } catch (const std::exception&) {
                    std::cerr << "ERROR: Invalid process ID: " << argv[i] << std::endl;
                    return static_cast<int>(ErrorCode::INVALID_PARAMETER);
                }
            }
            if (!foundAny) {
                std::cerr << "ERROR: --exclude-processes requires at least one process ID" << std::endl;
                return static_cast<int>(ErrorCode::INVALID_PARAMETER);
            }
        }
        else {
            std::cerr << "ERROR: Unknown argument: " << arg << std::endl;
            PrintUsage(argv[0]);
            return static_cast<int>(ErrorCode::INVALID_PARAMETER);
        }
    }
    return static_cast<int>(ErrorCode::SUCCESS);
}
