#pragma once

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include <iostream>

class ErrorHandler {
public:
    static void PrintDetailedError(OSStatus status, const char* context) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "ERROR: " << context << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "OSStatus Code: " << status << std::endl;

        char code[5] = {0};
        *(UInt32*)(code) = CFSwapInt32HostToBig(status);
        if (isprint(code[0]) && isprint(code[1]) && isprint(code[2]) && isprint(code[3])) {
            std::cerr << "Code String: '" << code << "'" << std::endl;
        }

        switch (status) {
            case kAudioConverterErr_FormatNotSupported:
                std::cerr << "\nCause: Audio format not supported" << std::endl;
                std::cerr << "Solution: Try common sample rates: 44100, 48000" << std::endl;
                break;
            case kAudioConverterErr_InvalidInputSize:
            case kAudioConverterErr_InvalidOutputSize:
                std::cerr << "\nCause: Invalid buffer size for audio conversion" << std::endl;
                std::cerr << "Solution: Try different --chunk-duration values (0.05, 0.1, 0.2)" << std::endl;
                break;
            default:
                std::cerr << "\nCause: Unknown error (" << status << ")" << std::endl;
                std::cerr << "Solution:" << std::endl;
                std::cerr << "  - Check System Preferences -> Sound" << std::endl;
                std::cerr << "  - Check System Preferences -> Privacy -> Screen Recording" << std::endl;
                std::cerr << "  - Restart CoreAudio: sudo killall coreaudiod" << std::endl;
                break;
        }
        std::cerr << "========================================\n" << std::endl;
    }

    static void PrintNSError(NSError* error, const char* context) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "ERROR: " << context << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "Error Code: " << error.code << std::endl;
        std::cerr << "Domain: " << error.domain.UTF8String << std::endl;
        std::cerr << "Description: " << error.localizedDescription.UTF8String << std::endl;

        if (error.code == -3801) {
            std::cerr << "\nCause: User declined screen recording permission" << std::endl;
            std::cerr << "Solution:" << std::endl;
            std::cerr << "  - Open System Preferences -> Privacy & Security -> Screen Recording" << std::endl;
            std::cerr << "  - Enable permission for this application" << std::endl;
            std::cerr << "  - Restart the application" << std::endl;
        }
        std::cerr << "========================================\n" << std::endl;
    }

    static void CheckSystemRequirements() {
        std::cerr << "Checking system requirements..." << std::endl;
        NSProcessInfo* processInfo = [NSProcessInfo processInfo];
        NSOperatingSystemVersion version = [processInfo operatingSystemVersion];
        std::cerr << "macOS Version: " << version.majorVersion << "."
                  << version.minorVersion << "." << version.patchVersion << std::endl;
        if (version.majorVersion < 13) {
            std::cerr << "WARNING: macOS 13 (Ventura) or later is required for ScreenCaptureKit" << std::endl;
        }
        std::cerr << std::endl;
    }
};
