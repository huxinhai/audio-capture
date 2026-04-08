#include "system_audio_capture.h"
#include "error_handler.h"
#include <unistd.h>

// Global state
SystemAudioCapture* g_capture = nullptr;
std::atomic<bool> g_running(true);

// ============================================================
// AudioStreamDelegate implementation
// ============================================================
@implementation AudioStreamDelegate
- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
               ofType:(SCStreamOutputType)type {
    (void)stream;
    if (type == SCStreamOutputTypeAudio && self.capture) {
        self.capture->OnAudioBuffer(sampleBuffer);
    }
}
@end

// ============================================================
// SystemAudioCapture implementation
// ============================================================
bool SystemAudioCapture::Initialize() {
    std::cerr << "Initializing macOS Audio Capture..." << std::endl;

    // SCK delivers 48000Hz, stereo, Float32 non-interleaved.
    // We configure deviceFormat as interleaved (we manually interleave before feeding AudioConverter).
    deviceFormat.mSampleRate = 48000;
    deviceFormat.mFormatID = kAudioFormatLinearPCM;
    deviceFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    deviceFormat.mChannelsPerFrame = 2;
    deviceFormat.mBitsPerChannel = 32;
    deviceFormat.mBytesPerFrame = deviceFormat.mChannelsPerFrame * (deviceFormat.mBitsPerChannel / 8);
    deviceFormat.mFramesPerPacket = 1;
    deviceFormat.mBytesPerPacket = deviceFormat.mBytesPerFrame;

    std::cerr << "Device format: " << (int)deviceFormat.mSampleRate << "Hz, "
              << deviceFormat.mChannelsPerFrame << " channels, "
              << deviceFormat.mBitsPerChannel << " bits (Float32)" << std::endl;

    int targetSampleRate = (cfg.sampleRate > 0) ? cfg.sampleRate : (int)deviceFormat.mSampleRate;
    int targetChannels = (cfg.channels > 0) ? cfg.channels : (int)deviceFormat.mChannelsPerFrame;
    int targetBitDepth = (cfg.bitDepth > 0) ? cfg.bitDepth : 16;

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
    if (cfg.chunkDuration < 0.01 || cfg.chunkDuration > 10.0) {
        std::cerr << "\nERROR: Invalid chunk duration: " << cfg.chunkDuration << std::endl;
        return false;
    }

    // Output format: integer PCM
    outputFormat.mSampleRate = targetSampleRate;
    outputFormat.mFormatID = kAudioFormatLinearPCM;
    outputFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    outputFormat.mChannelsPerFrame = targetChannels;
    outputFormat.mBitsPerChannel = targetBitDepth;
    outputFormat.mBytesPerFrame = targetChannels * (targetBitDepth / 8);
    outputFormat.mFramesPerPacket = 1;
    outputFormat.mBytesPerPacket = outputFormat.mBytesPerFrame;

    needsResampling = true;

    std::cerr << "Format conversion:" << std::endl;
    std::cerr << "  Input:  " << (int)deviceFormat.mSampleRate << "Hz, "
              << deviceFormat.mChannelsPerFrame << "ch, "
              << deviceFormat.mBitsPerChannel << "bit Float32" << std::endl;
    std::cerr << "  Output: " << targetSampleRate << "Hz, "
              << targetChannels << "ch, "
              << targetBitDepth << "bit Integer PCM" << std::endl;

    resampler = std::make_unique<AudioResampler>();
    if (!resampler->Initialize(deviceFormat, outputFormat)) {
        std::cerr << "Failed to initialize audio resampler" << std::endl;
        return false;
    }

    // Set up SCStream
    __block bool initSuccess = false;
    __block bool initDone = false;
    __block NSError* initError = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    auto* captureThis = this;
    auto& includeProcs = cfg.includeProcesses;
    auto& excludeProcs = cfg.excludeProcesses;

    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                                 onScreenWindowsOnly:NO
                                                   completionHandler:^(SCShareableContent* content, NSError* error) {
        if (error) {
            initError = error;
            initDone = true;
            dispatch_semaphore_signal(sem);
            return;
        }

        SCContentFilter* contentFilter = nil;
        NSMutableArray<SCRunningApplication*>* apps = [NSMutableArray array];

        if (!includeProcs.empty()) {
            for (SCRunningApplication* app in content.applications) {
                for (uint32_t pid : includeProcs) {
                    if (app.processID == (pid_t)pid) [apps addObject:app];
                }
            }
            contentFilter = [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject
                                                 includingApplications:apps
                                                   exceptingWindows:@[]];
        } else if (!excludeProcs.empty()) {
            for (SCRunningApplication* app in content.applications) {
                bool excluded = false;
                for (uint32_t pid : excludeProcs) {
                    if (app.processID == (pid_t)pid) { excluded = true; break; }
                }
                if (!excluded) [apps addObject:app];
            }
            contentFilter = [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject
                                                 includingApplications:apps
                                                   exceptingWindows:@[]];
        } else {
            contentFilter = [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject
                                                 includingApplications:content.applications
                                                   exceptingWindows:@[]];
        }

        captureThis->filter = contentFilter;

        SCStreamConfiguration* streamConfig = [[SCStreamConfiguration alloc] init];
        streamConfig.capturesAudio = YES;
        streamConfig.excludesCurrentProcessAudio = YES;
        streamConfig.channelCount = (NSInteger)captureThis->deviceFormat.mChannelsPerFrame;
        streamConfig.sampleRate = (NSInteger)captureThis->deviceFormat.mSampleRate;
        streamConfig.width = 2;
        streamConfig.height = 2;
        streamConfig.minimumFrameInterval = CMTimeMake(1, 1);

        captureThis->config = streamConfig;
        captureThis->stream = [[SCStream alloc] initWithFilter:contentFilter
                                                 configuration:streamConfig
                                                      delegate:nil];
        initSuccess = true;
        initDone = true;
        dispatch_semaphore_signal(sem);
    }];

    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

    if (!initDone) { std::cerr << "Timeout waiting for ScreenCaptureKit" << std::endl; return false; }
    if (initError) { ErrorHandler::PrintNSError(initError, "Failed to get shareable content"); return false; }
    if (!initSuccess) { std::cerr << "Failed to initialize ScreenCaptureKit" << std::endl; return false; }

    if (cfg.mute) std::cerr << "Note: Mute functionality is not yet implemented" << std::endl;

    std::cerr << "\n✓ Initialization successful!" << std::endl;
    std::cerr << "========================================" << std::endl;
    std::cerr << "Output Audio Format:" << std::endl;
    std::cerr << "  Sample Rate: " << targetSampleRate << " Hz" << std::endl;
    std::cerr << "  Channels:    " << targetChannels << std::endl;
    std::cerr << "  Bit Depth:   " << targetBitDepth << " bits" << std::endl;
    std::cerr << "========================================\n" << std::endl;
    return true;
}

void SystemAudioCapture::StartCapture() {
    if (!stream) return;

    freopen(nullptr, "wb", stdout);

    delegate = [[AudioStreamDelegate alloc] init];
    delegate.capture = this;
    audioQueue = dispatch_queue_create("com.audiocapture.audio", DISPATCH_QUEUE_SERIAL);

    NSError* error = nil;
    [stream addStreamOutput:delegate type:SCStreamOutputTypeAudio
             sampleHandlerQueue:audioQueue error:&error];
    if (error) { ErrorHandler::PrintNSError(error, "Failed to add stream output"); return; }

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block NSError* startError = nil;
    [stream startCaptureWithCompletionHandler:^(NSError* err) {
        startError = err;
        dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
    if (startError) { ErrorHandler::PrintNSError(startError, "Failed to start capture"); return; }

    std::cerr << "Capture started (ScreenCaptureKit audio-only mode)" << std::endl;

    while (g_running.load()) usleep(100000);

    dispatch_semaphore_t stopSem = dispatch_semaphore_create(0);
    [stream stopCaptureWithCompletionHandler:^(NSError*) {
        dispatch_semaphore_signal(stopSem);
    }];
    dispatch_semaphore_wait(stopSem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

    if (needsResampling && resampler) {
        std::vector<uint8_t> finalData;
        resampler->Flush(finalData);
        if (!finalData.empty()) {
            std::lock_guard<std::mutex> lock(writeMutex);
            fwrite(finalData.data(), 1, finalData.size(), stdout);
            fflush(stdout);
        }
    }
}

void SystemAudioCapture::OnAudioBuffer(CMSampleBufferRef sampleBuffer) {
    if (!formatLogged) {
        formatLogged = true;
        CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription(sampleBuffer);
        if (fmt) {
            const AudioStreamBasicDescription* asbd = CMAudioFormatDescriptionGetStreamBasicDescription(fmt);
            if (asbd) {
                std::cerr << "Actual SCK format: " << asbd->mSampleRate << "Hz, "
                          << asbd->mChannelsPerFrame << "ch, "
                          << asbd->mBitsPerChannel << "bit, flags=0x"
                          << std::hex << asbd->mFormatFlags << std::dec
                          << (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved ? " (non-interleaved)" : " (interleaved)")
                          << std::endl;
            }
        }
    }

    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer) return;

    CMItemCount numFrames = CMSampleBufferGetNumSamples(sampleBuffer);
    if (numFrames == 0) return;

    // Get AudioBufferList to access per-channel planes (non-interleaved)
    size_t bufferListSizeNeeded = 0;
    CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, &bufferListSizeNeeded, nullptr, 0, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, nullptr);

    std::vector<uint8_t> ablMemory(bufferListSizeNeeded);
    AudioBufferList* abl = reinterpret_cast<AudioBufferList*>(ablMemory.data());
    CMBlockBufferRef retainedBlock = nullptr;

    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, nullptr, abl, bufferListSizeNeeded, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &retainedBlock);

    if (status != noErr) {
        if (retainedBlock) CFRelease(retainedBlock);
        return;
    }

    int srcChannels = (int)deviceFormat.mChannelsPerFrame;
    size_t totalSamples = numFrames * srcChannels;

    if (interleaveBuffer.size() < totalSamples) interleaveBuffer.resize(totalSamples);

    // Interleave: [L0 L1...][R0 R1...] → [L0 R0 L1 R1...]
    if ((int)abl->mNumberBuffers >= srcChannels) {
        for (int ch = 0; ch < srcChannels; ch++) {
            const float* chData = (const float*)abl->mBuffers[ch].mData;
            for (CMItemCount f = 0; f < numFrames; f++) {
                interleaveBuffer[f * srcChannels + ch] = chData[f];
            }
        }
    } else {
        const float* src = (const float*)abl->mBuffers[0].mData;
        memcpy(interleaveBuffer.data(), src, totalSamples * sizeof(float));
    }

    if (retainedBlock) CFRelease(retainedBlock);

    uint32_t interleavedSize = (uint32_t)(totalSamples * sizeof(float));
    const uint8_t* interleavedBytes = reinterpret_cast<const uint8_t*>(interleaveBuffer.data());

    if (needsResampling && resampler) {
        audioOutputBuffer.clear();
        if (resampler->ProcessAudio(interleavedBytes, interleavedSize, audioOutputBuffer)) {
            if (!audioOutputBuffer.empty()) {
                std::lock_guard<std::mutex> lock(writeMutex);
                fwrite(audioOutputBuffer.data(), 1, audioOutputBuffer.size(), stdout);
                fflush(stdout);
            }
        }
    } else {
        std::lock_guard<std::mutex> lock(writeMutex);
        fwrite(interleavedBytes, 1, interleavedSize, stdout);
        fflush(stdout);
    }
}

void SystemAudioCapture::Stop() {
    g_running.store(false);
}

void SystemAudioCapture::Cleanup() {
    if (delegate) delegate.capture = nullptr;
    if (resampler) resampler.reset();
    stream = nil; filter = nil; config = nil; delegate = nil; audioQueue = nil;
}
