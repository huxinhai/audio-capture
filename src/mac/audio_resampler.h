#pragma once

#import <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <cstdint>
#include "error_handler.h"

class AudioResampler {
private:
    AudioConverterRef converter = nullptr;
    AudioStreamBasicDescription inputDesc = {};
    AudioStreamBasicDescription outputDesc = {};
    bool initialized = false;

    const uint8_t* callbackData = nullptr;
    UInt32 callbackDataSize = 0;
    std::vector<uint8_t> tempBuffer;

    static OSStatus ConverterCallback(AudioConverterRef,
                                       UInt32* ioNumberDataPackets,
                                       AudioBufferList* ioData,
                                       AudioStreamPacketDescription**,
                                       void* inUserData) {
        auto* self = static_cast<AudioResampler*>(inUserData);
        if (self->callbackDataSize == 0) {
            *ioNumberDataPackets = 0;
            ioData->mBuffers[0].mDataByteSize = 0;
            ioData->mBuffers[0].mData = nullptr;
            return noErr;
        }
        ioData->mBuffers[0].mData = const_cast<uint8_t*>(self->callbackData);
        ioData->mBuffers[0].mDataByteSize = self->callbackDataSize;
        *ioNumberDataPackets = self->callbackDataSize / self->inputDesc.mBytesPerFrame;
        self->callbackData = nullptr;
        self->callbackDataSize = 0;
        return noErr;
    }

public:
    AudioResampler() {}
    ~AudioResampler() { Cleanup(); }

    bool Initialize(const AudioStreamBasicDescription& inDesc,
                    const AudioStreamBasicDescription& outDesc) {
        inputDesc = inDesc;
        outputDesc = outDesc;

        std::cerr << "Creating audio resampler..." << std::endl;
        OSStatus status = AudioConverterNew(&inputDesc, &outputDesc, &converter);
        if (status != noErr) {
            ErrorHandler::PrintDetailedError(status, "Failed to create AudioConverter");
            return false;
        }

        UInt32 quality = kAudioConverterQuality_Medium;
        AudioConverterSetProperty(converter, kAudioConverterSampleRateConverterQuality,
                                  sizeof(quality), &quality);

        initialized = true;
        std::cerr << "Audio resampler initialized successfully!" << std::endl;
        return true;
    }

    bool ProcessAudio(const uint8_t* inputData, uint32_t inputSize,
                      std::vector<uint8_t>& outputData) {
        if (!initialized || !inputData || inputSize == 0) return false;

        UInt32 inputFrames = inputSize / inputDesc.mBytesPerFrame;
        double ratio = outputDesc.mSampleRate / inputDesc.mSampleRate;
        UInt32 outputFrames = (UInt32)(inputFrames * ratio) + 16;
        UInt32 outputSize = outputFrames * outputDesc.mBytesPerFrame;

        if (tempBuffer.size() < outputSize) {
            tempBuffer.resize(outputSize);
        }

        AudioBufferList outputBufferList;
        outputBufferList.mNumberBuffers = 1;
        outputBufferList.mBuffers[0].mNumberChannels = outputDesc.mChannelsPerFrame;
        outputBufferList.mBuffers[0].mDataByteSize = outputSize;
        outputBufferList.mBuffers[0].mData = tempBuffer.data();

        callbackData = inputData;
        callbackDataSize = inputSize;

        UInt32 outputPackets = outputFrames;
        OSStatus status = AudioConverterFillComplexBuffer(
            converter, ConverterCallback, this,
            &outputPackets, &outputBufferList, nullptr);

        if (status != noErr && status != -1) return false;

        UInt32 actualOutputSize = outputBufferList.mBuffers[0].mDataByteSize;
        if (actualOutputSize > 0) {
            outputData.insert(outputData.end(),
                              tempBuffer.begin(),
                              tempBuffer.begin() + actualOutputSize);
        }
        return true;
    }

    void Flush(std::vector<uint8_t>&) {}

    void Cleanup() {
        if (converter) {
            AudioConverterDispose(converter);
            converter = nullptr;
        }
        initialized = false;
    }
};
