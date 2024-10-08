/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/logging.h>
#include <thread>
#include <cinttypes>
#include <jni.h>

#include "NativePlayer.h"

NativePlayer::NativePlayer(AAssetManager &assetManager): mAssetManager(assetManager) {
}

void NativePlayer::load() {

    if (!openStream()) {
        mGameState = GameState::FailedToLoad;
        return;
    }

    if (!setupAudioSources()) {
        mGameState = GameState::FailedToLoad;
        return;
    }

    scheduleSongEvents();

    Result result = mAudioStream->requestStart();
    if (result != Result::OK){
        LOGE("Failed to start stream. Error: %s", convertToText(result));
        mGameState = GameState::FailedToLoad;
        return;
    }

    mGameState = GameState::Playing;
}

void NativePlayer::start() {

    // async returns a future, we must store this future to avoid blocking. It's not sufficient
    // to store this in a local variable as its destructor will block until NativePlayer::load completes.
    mLoadingResult = std::async(&NativePlayer::load, this);
}

void NativePlayer::stop(){

    if (mAudioStream){
        mAudioStream->stop();
        mAudioStream->close();
        mAudioStream.reset();
    }
}

void NativePlayer::tap(int64_t eventTimeAsUptime) {

    if (mGameState != GameState::Playing){
        LOGW("NativePlayer not in playing state, ignoring tap event");
    } else {
        mClap->setPlaying(true);

        int64_t nextClapWindowTimeMs;
        if (mClapWindows.pop(nextClapWindowTimeMs)){

            // Convert the tap time to a song position
            int64_t tapTimeInSongMs = mSongPositionMs + (eventTimeAsUptime - mLastUpdateTime);
            TapResult result = getTapResult(tapTimeInSongMs, nextClapWindowTimeMs);
            mUiEvents.push(result);
        }
    }
}

void NativePlayer::tick(){

    switch (mGameState){
        case GameState::Playing:
            TapResult r;
            if (mUiEvents.pop(r)) {
                renderEvent(r);
            } else {
                SetGLScreenColor(kPlayingColor);
            }
            break;

        case GameState::Loading:
            SetGLScreenColor(kLoadingColor);
            break;

        case GameState::FailedToLoad:
            SetGLScreenColor(kLoadingFailedColor);
            break;
    }
}

void NativePlayer::onSurfaceCreated() {
    SetGLScreenColor(kLoadingColor);
}

void NativePlayer::onSurfaceChanged(int widthInPixels, int heightInPixels) {
}

void NativePlayer::onSurfaceDestroyed() {
}

DataCallbackResult NativePlayer::onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    auto *outputBuffer = static_cast<float *>(audioData);

    int64_t nextClapEventMs;

    for (int i = 0; i < numFrames; ++i) {

        mSongPositionMs = convertFramesToMillis(
                mCurrentFrame,
                mAudioStream->getSampleRate());

        if (mClapEvents.peek(nextClapEventMs) && mSongPositionMs >= nextClapEventMs){
            mClap->setPlaying(true);
            mClapEvents.pop(nextClapEventMs);
        }
        mMixer.renderAudio(outputBuffer+(oboeStream->getChannelCount()*i), 1);
        mCurrentFrame++;
    }

    mLastUpdateTime = nowUptimeMillis();

    return DataCallbackResult::Continue;
}

void NativePlayer::onErrorAfterClose(AudioStream *audioStream, Result error) {
    if (error == Result::ErrorDisconnected){
        mGameState = GameState::Loading;
        mAudioStream.reset();
        mMixer.removeAllTracks();
        mCurrentFrame = 0;
        mSongPositionMs = 0;
        mLastUpdateTime = 0;
        start();
    } else {
        LOGE("Stream error: %s", convertToText(error));
    }
}

/**
 * Get the result of a tap
 *
 * @param tapTimeInMillis - The time the tap occurred in milliseconds
 * @param tapWindowInMillis - The time at the middle of the "tap window" in milliseconds
 * @return TapResult can be Early, Late or Success
 */
TapResult NativePlayer::getTapResult(int64_t tapTimeInMillis, int64_t tapWindowInMillis){
    LOGD("Tap time %" PRId64 ", tap window time: %" PRId64, tapTimeInMillis, tapWindowInMillis);
    if (tapTimeInMillis <= tapWindowInMillis + kWindowCenterOffsetMs) {
        if (tapTimeInMillis >= tapWindowInMillis - kWindowCenterOffsetMs) {
            return TapResult::Success;
        } else {
            return TapResult::Early;
        }
    } else {
        return TapResult::Late;
    }
}

bool NativePlayer::openStream() {

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setFormat(AudioFormat::Float);
    builder.setFormatConversionAllowed(true);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setSampleRate(48000);
    builder.setSampleRateConversionQuality(
            SampleRateConversionQuality::Medium);
    builder.setChannelCount(2);
    builder.setDataCallback(this);
    builder.setErrorCallback(this);
    Result result = builder.openStream(mAudioStream);
    if (result != Result::OK){
        LOGE("Failed to open stream. Error: %s", convertToText(result));
        return false;
    }

    mMixer.setChannelCount(mAudioStream->getChannelCount());

    return true;
}

bool NativePlayer::setupAudioSources() {

    // Set the properties of our audio source(s) to match that of our audio stream
    AudioProperties targetProperties {
            .channelCount = mAudioStream->getChannelCount(),
            .sampleRate = mAudioStream->getSampleRate()
    };

    // Create a data source and player for the clap sound
    std::shared_ptr<AAssetDataSource> mClapSource {
            AAssetDataSource::newFromCompressedAsset(mAssetManager, kClapFilename, targetProperties)
    };
    if (mClapSource == nullptr){
        LOGE("Could not load source data for clap sound");
        return false;
    }

    mClap = std::make_unique<Player>(mClapSource);

//    // Create a data source and player for our backing track
//    std::shared_ptr<AAssetDataSource> backingTrackSource {
//            AAssetDataSource::newFromCompressedAsset(mAssetManager, kBackingTrackFilename, targetProperties)
//    };
//    if (backingTrackSource == nullptr){
//        LOGE("Could not load source data for backing track");
//        return false;
//    }
    // Add both players to a mixer
    mMixer.addTrack(mClap.get());
    return true;
}

void NativePlayer::scheduleSongEvents() {

    for (auto t : kClapEvents) mClapEvents.push(t);
    for (auto t : kClapWindows) mClapWindows.push(t);
}

void NativePlayer::passPcmData(
        JNIEnv *env,jobject pcm_buffer,
                               jint num_channels,
                               jint sample_rate){

    uint8_t *pcmData = static_cast<uint8_t *>(env->GetDirectBufferAddress(pcm_buffer));
    jlong numBytes = env->GetDirectBufferCapacity(pcm_buffer);

    AudioProperties targetProperties {
            .channelCount = static_cast<int32_t>(num_channels),
            .sampleRate = static_cast<int32_t>(sample_rate)
    };

    std::shared_ptr<AAssetDataSource> backingTrackSource {
            AAssetDataSource::newFromPcmData(
                    pcmData,
                    numBytes,
                    targetProperties
            )
    };

    if (backingTrackSource == nullptr){
        LOGE("Could not load source data for clap sound");
        return;
    }else{
        LOGE("Rnandomsdasdsa");
    }

    LOGD("backingTrackSource numSamples: %" PRId64, backingTrackSource->getSize());

    mBackingTrack = std::make_unique<Player>(backingTrackSource);
    mBackingTrack->setPlaying(true);
    mBackingTrack->setLooping(true);

    mMixer.addTrack(mBackingTrack.get());
}