#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mfapi.h>
#include <vector>
#include <memory>
#include <iostream>
#include "../common/common.h"
#include "error_handler.h"
#include "audio_resampler.h"

class WASAPICapture;

extern WASAPICapture* g_capture;

class WASAPICapture {
private:
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    WAVEFORMATEX* pOutputFormat = nullptr;
    UINT32 bufferFrameCount = 0;

    CaptureConfig cfg;

    bool running = false;
    bool needsResampling = false;
    std::unique_ptr<AudioResampler> resampler;

public:
    WASAPICapture(const CaptureConfig& c) : cfg(c) {}
    ~WASAPICapture() { Cleanup(); }

    bool Initialize();
    void StartCapture();
    void StartCapturePolling();
    void Stop();
    void Cleanup();
};
