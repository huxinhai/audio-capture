#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <comdef.h>
#include <audioclient.h>

class ErrorHandler {
public:
    static void PrintDetailedError(HRESULT hr, const char* context) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "ERROR: " << context << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "HRESULT Code: 0x" << std::hex << hr << std::dec << std::endl;

        _com_error err(hr);
        std::wcerr << L"System Message: " << err.ErrorMessage() << std::endl;

        switch (hr) {
            case E_POINTER:
                std::cerr << "\nCause: Invalid pointer" << std::endl;
                std::cerr << "Solution: This is a programming error. Please report this bug." << std::endl;
                break;
            case E_INVALIDARG:
                std::cerr << "\nCause: Invalid argument provided" << std::endl;
                std::cerr << "Solution: Check command line parameters (sample rate, chunk duration, etc.)" << std::endl;
                break;
            case E_OUTOFMEMORY:
                std::cerr << "\nCause: Insufficient memory" << std::endl;
                std::cerr << "Solution: Close other applications to free up memory" << std::endl;
                break;
            case E_ACCESSDENIED:
                std::cerr << "\nCause: Access denied / Permission issue" << std::endl;
                std::cerr << "Solution: " << std::endl;
                std::cerr << "  - Run as Administrator" << std::endl;
                std::cerr << "  - Check Windows Privacy Settings -> Microphone access" << std::endl;
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                std::cerr << "\nCause: Audio device was removed or disabled" << std::endl;
                std::cerr << "Solution: Check if audio device is properly connected" << std::endl;
                break;
            case AUDCLNT_E_DEVICE_IN_USE:
                std::cerr << "\nCause: Audio device is exclusively used by another application" << std::endl;
                std::cerr << "Solution: Close applications using audio exclusively" << std::endl;
                break;
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                std::cerr << "\nCause: Requested audio format is not supported by device" << std::endl;
                std::cerr << "Solution: Try without --sample-rate parameter (use device default)" << std::endl;
                break;
            case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
                std::cerr << "\nCause: Buffer size is not aligned with device requirements" << std::endl;
                std::cerr << "Solution: Try different --chunk-duration values (0.05, 0.1, 0.2)" << std::endl;
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                std::cerr << "\nCause: Windows Audio service is not running" << std::endl;
                std::cerr << "Solution: Start 'Windows Audio' service in services.msc" << std::endl;
                break;
            case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
                std::cerr << "\nCause: Failed to create audio endpoint" << std::endl;
                std::cerr << "Solution: Restart Windows Audio service or update audio drivers" << std::endl;
                break;
            case CO_E_NOTINITIALIZED:
                std::cerr << "\nCause: COM library not initialized" << std::endl;
                std::cerr << "Solution: This is a programming error. Please report this bug." << std::endl;
                break;
            case REGDB_E_CLASSNOTREG:
                std::cerr << "\nCause: Required COM component not registered" << std::endl;
                std::cerr << "Solution: Run Windows Update or 'sfc /scannow'" << std::endl;
                break;
            default:
                std::cerr << "\nCause: Unknown error (0x" << std::hex << hr << std::dec << ")" << std::endl;
                std::cerr << "Solution: Update audio drivers or restart Windows Audio service" << std::endl;
                break;
        }
        std::cerr << "========================================\n" << std::endl;
    }

    static void CheckSystemRequirements() {
        std::cerr << "Checking system requirements..." << std::endl;

        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);

        #pragma warning(push)
        #pragma warning(disable: 4996)
        if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
            std::cerr << "Windows Version: " << osvi.dwMajorVersion << "."
                     << osvi.dwMinorVersion << " Build " << osvi.dwBuildNumber << std::endl;
            if (osvi.dwMajorVersion < 6) {
                std::cerr << "WARNING: Windows Vista or later is required for WASAPI" << std::endl;
            }
        }
        #pragma warning(pop)

        BOOL isAdmin = FALSE;
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
        PSID AdministratorsGroup;
        if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                     &AdministratorsGroup)) {
            CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
            FreeSid(AdministratorsGroup);
        }

        if (isAdmin) {
            std::cerr << "Privilege Level: Administrator (OK)" << std::endl;
        } else {
            std::cerr << "Privilege Level: Standard User" << std::endl;
        }
        std::cerr << std::endl;
    }
};
