#pragma once

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include "../common/common.h"
#include "audio_resampler.h"

class SystemAudioCapture;

// Global state for signal handling
extern SystemAudioCapture* g_capture;
extern std::atomic<bool> g_running;

// ObjC delegate that receives audio buffers from SCStream
@interface AudioStreamDelegate : NSObject <SCStreamOutput>
@property (nonatomic, assign) SystemAudioCapture* capture;
@end

class SystemAudioCapture {
private:
    SCStream* stream = nil;
    SCContentFilter* filter = nil;
    SCStreamConfiguration* config = nil;
    AudioStreamDelegate* delegate = nil;
    dispatch_queue_t audioQueue = nil;

    CaptureConfig cfg;

    bool needsResampling = false;
    std::unique_ptr<AudioResampler> resampler;
    bool formatLogged = false;

    AudioStreamBasicDescription deviceFormat = {};
    AudioStreamBasicDescription outputFormat = {};

    std::mutex writeMutex;
    std::vector<uint8_t> audioOutputBuffer;
    std::vector<float> interleaveBuffer;

public:
    SystemAudioCapture(const CaptureConfig& c) : cfg(c) {}
    ~SystemAudioCapture() { Cleanup(); }

    bool Initialize();
    void StartCapture();
    void OnAudioBuffer(CMSampleBufferRef sampleBuffer);
    void Stop();
    void Cleanup();
};
