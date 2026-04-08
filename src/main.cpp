#include "common/common.h"
#include <csignal>

#ifdef __APPLE__
    #include "mac/system_audio_capture.h"
    #include "mac/error_handler.h"
#elif _WIN32
    #include "windows/wasapi_capture.h"
    #include "windows/error_handler.h"
#endif

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        if (g_capture) g_capture->Stop();
        return TRUE;
    }
    return FALSE;
}
#else
void SignalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (g_capture) g_capture->Stop();
    }
}
#endif

int main(int argc, char* argv[]) {
    std::cerr << "========================================" << std::endl;
#ifdef __APPLE__
    std::cerr << "macOS Audio Capture v1.0" << std::endl;
#elif _WIN32
    std::cerr << "WASAPI Audio Capture v1.0" << std::endl;
#endif
    std::cerr << "========================================\n" << std::endl;

    ErrorHandler::CheckSystemRequirements();

    CaptureConfig config;
    int parseResult = ParseArguments(argc, argv, config);
    if (parseResult == -1) return 0;
    if (parseResult != 0) return parseResult;

#ifdef __APPLE__
    SystemAudioCapture capture(config);
#elif _WIN32
    WASAPICapture capture(config);
#endif
    g_capture = &capture;

#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif

    if (!capture.Initialize()) {
        std::cerr << "\n!!! INITIALIZATION FAILED !!!" << std::endl;
#ifdef __APPLE__
        std::cerr << "Common solutions:" << std::endl;
        std::cerr << "  1. Grant Screen Recording permission in System Preferences" << std::endl;
        std::cerr << "  2. Make sure macOS 13 (Ventura) or later is installed" << std::endl;
        std::cerr << "  3. Restart CoreAudio: sudo killall coreaudiod" << std::endl;
#elif _WIN32
        std::cerr << "Common solutions:" << std::endl;
        std::cerr << "  1. Make sure audio device is working" << std::endl;
        std::cerr << "  2. Try running as Administrator" << std::endl;
        std::cerr << "  3. Update audio drivers" << std::endl;
        std::cerr << "  4. Restart Windows Audio service" << std::endl;
#endif
        return static_cast<int>(ErrorCode::UNKNOWN_ERROR);
    }

    std::cerr << "Starting capture... (Press Ctrl+C to stop)" << std::endl;
    std::cerr << "Audio data will be written to stdout (binary PCM format)" << std::endl;
    std::cerr << "========================================\n" << std::endl;

    capture.StartCapture();

    std::cerr << "\nCapture stopped." << std::endl;
    return static_cast<int>(ErrorCode::SUCCESS);
}
