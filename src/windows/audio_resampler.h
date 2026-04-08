#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <vector>
#include <iostream>
#include "error_handler.h"

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

class AudioResampler {
private:
    IMFTransform* pResampler = nullptr;
    IMFMediaType* pInputType = nullptr;
    IMFMediaType* pOutputType = nullptr;

    WAVEFORMATEX* pInputFormat = nullptr;
    WAVEFORMATEX* pOutputFormat = nullptr;

    bool initialized = false;

    bool TryGetOutput(std::vector<BYTE>& outputData) {
        MFT_OUTPUT_DATA_BUFFER outputBuffer = {};
        MFT_OUTPUT_STREAM_INFO streamInfo = {};
        IMFMediaBuffer* pBuffer = nullptr;

        HRESULT hr = pResampler->GetOutputStreamInfo(0, &streamInfo);
        if (FAILED(hr)) return false;

        hr = MFCreateSample(&outputBuffer.pSample);
        if (FAILED(hr)) return false;

        DWORD bufferSize = streamInfo.cbSize > 0 ? streamInfo.cbSize : 8192;
        hr = MFCreateMemoryBuffer(bufferSize, &pBuffer);
        if (FAILED(hr)) {
            SafeRelease(&outputBuffer.pSample);
            return false;
        }

        outputBuffer.pSample->AddBuffer(pBuffer);
        SafeRelease(&pBuffer);

        DWORD status = 0;
        hr = pResampler->ProcessOutput(0, 1, &outputBuffer, &status);

        if (SUCCEEDED(hr)) {
            IMFMediaBuffer* pOutBuffer = nullptr;
            hr = outputBuffer.pSample->ConvertToContiguousBuffer(&pOutBuffer);
            if (SUCCEEDED(hr)) {
                BYTE* pOutData = nullptr;
                DWORD outSize = 0;
                hr = pOutBuffer->Lock(&pOutData, nullptr, &outSize);
                if (SUCCEEDED(hr) && outSize > 0) {
                    outputData.assign(pOutData, pOutData + outSize);
                    pOutBuffer->Unlock();
                }
                SafeRelease(&pOutBuffer);
            }
            SafeRelease(&outputBuffer.pSample);
            return true;
        } else {
            SafeRelease(&outputBuffer.pSample);
            return false;
        }
    }

public:
    AudioResampler() {}
    ~AudioResampler() { Cleanup(); }

    bool Initialize(WAVEFORMATEX* inputFormat, WAVEFORMATEX* outputFormat) {
        if (!inputFormat || !outputFormat) return false;

        pInputFormat = inputFormat;
        pOutputFormat = outputFormat;

        std::cerr << "Creating audio resampler..." << std::endl;
        HRESULT hr = CoCreateInstance(CLSID_CResamplerMediaObject, nullptr,
                                     CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pResampler));
        if (FAILED(hr)) {
            std::cerr << "Failed to create resampler COM object: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        hr = MFCreateMediaType(&pInputType);
        if (FAILED(hr)) return false;

        UINT32 waveFormatSize = sizeof(WAVEFORMATEX) + pInputFormat->cbSize;
        hr = MFInitMediaTypeFromWaveFormatEx(pInputType, pInputFormat, waveFormatSize);
        if (FAILED(hr)) return false;

        hr = MFCreateMediaType(&pOutputType);
        if (FAILED(hr)) return false;

        UINT32 outputWaveFormatSize = sizeof(WAVEFORMATEX) + pOutputFormat->cbSize;
        hr = MFInitMediaTypeFromWaveFormatEx(pOutputType, pOutputFormat, outputWaveFormatSize);
        if (FAILED(hr)) return false;

        hr = pResampler->SetInputType(0, pInputType, 0);
        if (FAILED(hr)) {
            std::cerr << "Failed to set input type: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        hr = pResampler->SetOutputType(0, pOutputType, 0);
        if (FAILED(hr)) {
            std::cerr << "Failed to set output type: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        pResampler->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        pResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        pResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

        initialized = true;
        std::cerr << "Audio resampler initialized successfully!" << std::endl;
        return true;
    }

    bool ProcessAudio(const BYTE* inputData, UINT32 inputSize,
                     std::vector<BYTE>& outputData) {
        if (!initialized || !inputData || inputSize == 0) return false;

        HRESULT hr;

        std::vector<BYTE> tempOutput;
        while (TryGetOutput(tempOutput)) {
            outputData.insert(outputData.end(), tempOutput.begin(), tempOutput.end());
            tempOutput.clear();
        }

        IMFSample* pSample = nullptr;
        IMFMediaBuffer* pBuffer = nullptr;

        hr = MFCreateSample(&pSample);
        if (FAILED(hr)) return !outputData.empty();

        hr = MFCreateMemoryBuffer(inputSize, &pBuffer);
        if (FAILED(hr)) { SafeRelease(&pSample); return !outputData.empty(); }

        BYTE* pBufferData = nullptr;
        hr = pBuffer->Lock(&pBufferData, nullptr, nullptr);
        if (FAILED(hr)) { SafeRelease(&pBuffer); SafeRelease(&pSample); return !outputData.empty(); }

        memcpy(pBufferData, inputData, inputSize);
        pBuffer->Unlock();
        pBuffer->SetCurrentLength(inputSize);

        pSample->AddBuffer(pBuffer);
        SafeRelease(&pBuffer);

        hr = pResampler->ProcessInput(0, pSample, 0);
        SafeRelease(&pSample);

        if (FAILED(hr) && hr != MF_E_NOTACCEPTING) return !outputData.empty();

        while (TryGetOutput(tempOutput)) {
            outputData.insert(outputData.end(), tempOutput.begin(), tempOutput.end());
            tempOutput.clear();
        }

        return true;
    }

    void Flush(std::vector<BYTE>& outputData) {
        if (!initialized) return;
        pResampler->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        std::vector<BYTE> tempOutput;
        while (TryGetOutput(tempOutput)) {
            outputData.insert(outputData.end(), tempOutput.begin(), tempOutput.end());
            tempOutput.clear();
        }
    }

    void Cleanup() {
        SafeRelease(&pOutputType);
        SafeRelease(&pInputType);
        SafeRelease(&pResampler);
        initialized = false;
    }
};
