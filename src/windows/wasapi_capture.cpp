#include "wasapi_capture.h"
#include <audiopolicy.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

WASAPICapture* g_capture = nullptr;

bool WASAPICapture::Initialize() {
    std::cerr << "Initializing WASAPI Audio Capture..." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to initialize COM library");
        return false;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to initialize Media Foundation");
        return false;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to create audio device enumerator");
        return false;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to get default audio device");
        return false;
    }

    // Get device name
    IPropertyStore* pProps = nullptr;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
            std::wcerr << L"Using audio device: " << varName.pwszVal << std::endl;
            PropVariantClear(&varName);
        }
        SafeRelease(&pProps);
    }

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to activate audio client");
        return false;
    }

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to get audio format");
        return false;
    }

    std::cerr << "Device format: " << pwfx->nSamplesPerSec << "Hz, "
              << pwfx->nChannels << " channels, "
              << pwfx->wBitsPerSample << " bits" << std::endl;

    int targetSampleRate = (cfg.sampleRate > 0) ? cfg.sampleRate : pwfx->nSamplesPerSec;
    int targetChannels = (cfg.channels > 0) ? cfg.channels : pwfx->nChannels;
    int targetBitDepth = (cfg.bitDepth > 0) ? cfg.bitDepth : pwfx->wBitsPerSample;

    // Validate
    if (cfg.sampleRate > 0 && (cfg.sampleRate < 8000 || cfg.sampleRate > 192000)) {
        std::cerr << "\nERROR: Invalid sample rate: " << cfg.sampleRate << std::endl;
        return false;
    }
    if (cfg.channels > 0 && (cfg.channels < 1 || cfg.channels > 8)) {
        std::cerr << "\nERROR: Invalid channel count: " << cfg.channels << std::endl;
        return false;
    }
    if (cfg.bitDepth > 0 && (cfg.bitDepth != 16 && cfg.bitDepth != 24 && cfg.bitDepth != 32)) {
        std::cerr << "\nERROR: Invalid bit depth: " << cfg.bitDepth << std::endl;
        return false;
    }

    needsResampling = (targetSampleRate != (int)pwfx->nSamplesPerSec) ||
                     (targetChannels != (int)pwfx->nChannels) ||
                     (targetBitDepth != (int)pwfx->wBitsPerSample);

    if (needsResampling) {
        std::cerr << "Format conversion:" << std::endl;
        std::cerr << "  Input:  " << pwfx->nSamplesPerSec << "Hz, "
                  << pwfx->nChannels << "ch, " << pwfx->wBitsPerSample << "bit" << std::endl;
        std::cerr << "  Output: " << targetSampleRate << "Hz, "
                  << targetChannels << "ch, " << targetBitDepth << "bit" << std::endl;

        pOutputFormat = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
        if (!pOutputFormat) return false;

        ZeroMemory(pOutputFormat, sizeof(WAVEFORMATEX));
        pOutputFormat->wFormatTag = WAVE_FORMAT_PCM;
        pOutputFormat->nChannels = targetChannels;
        pOutputFormat->nSamplesPerSec = targetSampleRate;
        pOutputFormat->wBitsPerSample = targetBitDepth;
        pOutputFormat->nBlockAlign = (targetChannels * targetBitDepth) / 8;
        pOutputFormat->nAvgBytesPerSec = targetSampleRate * pOutputFormat->nBlockAlign;
        pOutputFormat->cbSize = 0;

        resampler = std::make_unique<AudioResampler>();
        if (!resampler->Initialize(pwfx, pOutputFormat)) {
            std::cerr << "Failed to initialize audio resampler" << std::endl;
            return false;
        }
    } else {
        std::cerr << "No format conversion needed, using device format" << std::endl;
    }

    if (cfg.chunkDuration < 0.01 || cfg.chunkDuration > 10.0) {
        std::cerr << "\nERROR: Invalid chunk duration: " << cfg.chunkDuration << std::endl;
        return false;
    }

    REFERENCE_TIME hnsRequestedDuration = (REFERENCE_TIME)(cfg.chunkDuration * 10000000);
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsRequestedDuration, 0, pwfx, nullptr);

    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to initialize audio client");
        return false;
    }

    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to get audio buffer size");
        return false;
    }

    std::cerr << "Buffer size: " << bufferFrameCount << " frames ("
              << (double)bufferFrameCount / pwfx->nSamplesPerSec * 1000 << " ms)" << std::endl;

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) {
        ErrorHandler::PrintDetailedError(hr, "Failed to get capture client service");
        return false;
    }

    if (cfg.mute) std::cerr << "Note: Mute functionality is not yet implemented" << std::endl;

    std::cerr << "\n✓ Initialization successful!" << std::endl;
    std::cerr << "========================================" << std::endl;
    std::cerr << "Output Audio Format:" << std::endl;
    if (needsResampling) {
        std::cerr << "  Sample Rate: " << pOutputFormat->nSamplesPerSec << " Hz" << std::endl;
        std::cerr << "  Channels:    " << pOutputFormat->nChannels << std::endl;
        std::cerr << "  Bit Depth:   " << pOutputFormat->wBitsPerSample << " bits" << std::endl;
    } else {
        std::cerr << "  Sample Rate: " << pwfx->nSamplesPerSec << " Hz" << std::endl;
        std::cerr << "  Channels:    " << pwfx->nChannels << std::endl;
        std::cerr << "  Bit Depth:   " << pwfx->wBitsPerSample << " bits" << std::endl;
    }
    std::cerr << "========================================\n" << std::endl;
    return true;
}

void WASAPICapture::StartCapture() {
    if (!pAudioClient) return;

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent) { std::cerr << "Failed to create event" << std::endl; return; }

    HRESULT hr = pAudioClient->SetEventHandle(hEvent);
    if (FAILED(hr)) {
        std::cerr << "Failed to set event handle, falling back to polling mode" << std::endl;
        CloseHandle(hEvent);
        StartCapturePolling();
        return;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) { std::cerr << "Failed to start audio client" << std::endl; CloseHandle(hEvent); return; }

    running = true;
    _setmode(_fileno(stdout), _O_BINARY);

    std::cerr << "Using event-driven capture mode" << std::endl;

    while (running) {
        DWORD waitResult = WaitForSingleObject(hEvent, 2000);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) continue;
            else break;
        }

        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);

        while (SUCCEEDED(hr) && packetLength != 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
            if (SUCCEEDED(hr)) {
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                    std::cerr << "Warning: Audio data discontinuity detected" << std::endl;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    UINT32 outputSize = numFramesAvailable * (needsResampling ? pOutputFormat->nBlockAlign : pwfx->nBlockAlign);
                    std::vector<BYTE> silence(outputSize, 0);
                    std::cout.write(reinterpret_cast<char*>(silence.data()), silence.size());
                } else {
                    UINT32 inputSize = numFramesAvailable * pwfx->nBlockAlign;
                    if (needsResampling && resampler) {
                        std::vector<BYTE> outputData;
                        if (resampler->ProcessAudio(pData, inputSize, outputData)) {
                            if (!outputData.empty())
                                std::cout.write(reinterpret_cast<char*>(outputData.data()), outputData.size());
                        }
                    } else {
                        std::cout.write(reinterpret_cast<char*>(pData), inputSize);
                    }
                }
                std::cout.flush();
                pCaptureClient->ReleaseBuffer(numFramesAvailable);
            }
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
    }

    pAudioClient->Stop();

    if (needsResampling && resampler) {
        std::vector<BYTE> finalData;
        resampler->Flush(finalData);
        if (!finalData.empty()) {
            std::cout.write(reinterpret_cast<char*>(finalData.data()), finalData.size());
            std::cout.flush();
        }
    }
    CloseHandle(hEvent);
}

void WASAPICapture::StartCapturePolling() {
    if (!pAudioClient) return;

    HRESULT hr = pAudioClient->Start();
    if (FAILED(hr)) { std::cerr << "Failed to start audio client" << std::endl; return; }

    running = true;
    _setmode(_fileno(stdout), _O_BINARY);

    std::cerr << "Using polling mode" << std::endl;

    DWORD sleepTime = static_cast<DWORD>(cfg.chunkDuration * 1000 / 4);
    if (sleepTime < 1) sleepTime = 1;

    while (running) {
        Sleep(sleepTime);

        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);

        while (SUCCEEDED(hr) && packetLength != 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
            if (SUCCEEDED(hr)) {
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                    std::cerr << "Warning: Audio data discontinuity detected" << std::endl;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    UINT32 outputSize = numFramesAvailable * (needsResampling ? pOutputFormat->nBlockAlign : pwfx->nBlockAlign);
                    std::vector<BYTE> silence(outputSize, 0);
                    std::cout.write(reinterpret_cast<char*>(silence.data()), silence.size());
                } else {
                    UINT32 inputSize = numFramesAvailable * pwfx->nBlockAlign;
                    if (needsResampling && resampler) {
                        std::vector<BYTE> outputData;
                        if (resampler->ProcessAudio(pData, inputSize, outputData)) {
                            if (!outputData.empty())
                                std::cout.write(reinterpret_cast<char*>(outputData.data()), outputData.size());
                        }
                    } else {
                        std::cout.write(reinterpret_cast<char*>(pData), inputSize);
                    }
                }
                std::cout.flush();
                pCaptureClient->ReleaseBuffer(numFramesAvailable);
            }
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
    }

    pAudioClient->Stop();

    if (needsResampling && resampler) {
        std::vector<BYTE> finalData;
        resampler->Flush(finalData);
        if (!finalData.empty()) {
            std::cout.write(reinterpret_cast<char*>(finalData.data()), finalData.size());
            std::cout.flush();
        }
    }
}

void WASAPICapture::Stop() {
    running = false;
}

void WASAPICapture::Cleanup() {
    if (pAudioClient) pAudioClient->Stop();
    if (resampler) resampler.reset();
    if (pwfx) { CoTaskMemFree(pwfx); pwfx = nullptr; }
    if (pOutputFormat) { CoTaskMemFree(pOutputFormat); pOutputFormat = nullptr; }
    SafeRelease(&pCaptureClient);
    SafeRelease(&pAudioClient);
    SafeRelease(&pDevice);
    SafeRelease(&pEnumerator);
    MFShutdown();
    CoUninitialize();
}
