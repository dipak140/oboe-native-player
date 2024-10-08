/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <jni.h>
#include <memory>

#include <android/asset_manager_jni.h>

#include "utils/logging.h"
#include "NativePlayer.h"


extern "C" {

std::unique_ptr<NativePlayer> nativePlayer;

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_MainActivity_native_1onStart(JNIEnv *env, jobject instance,
                                                                     jobject jAssetManager) {

    AAssetManager *assetManager = AAssetManager_fromJava(env, jAssetManager);
    if (assetManager == nullptr) {
        LOGE("Could not obtain the AAssetManager");
        return;
    }

    nativePlayer = std::make_unique<NativePlayer>(*assetManager);
    nativePlayer->start();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_RendererWrapper_native_1onSurfaceCreated(JNIEnv *env,
                                                                                jobject instance) {
    nativePlayer->onSurfaceCreated();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_RendererWrapper_native_1onSurfaceChanged(JNIEnv *env,
                                                                                jclass type,
                                                                                jint width,
                                                                                jint height) {
    nativePlayer->onSurfaceChanged(width, height);
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_RendererWrapper_native_1onDrawFrame(JNIEnv *env,
                                                                           jclass type) {
    nativePlayer->tick();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_GameSurfaceView_native_1onTouchInput(JNIEnv *env,
                                                                            jclass type,
                                                                            jint event_type,
                                                                            jlong time_since_boot_ms,
                                                                            jint pixel_x,
                                                                            jint pixel_y) {
    nativePlayer->tap(time_since_boot_ms);
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_GameSurfaceView_native_1surfaceDestroyed__(JNIEnv *env,
                                                                                  jclass type) {
    nativePlayer->onSurfaceDestroyed();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_MainActivity_native_1onStop(JNIEnv *env, jobject instance) {

    nativePlayer->stop();
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_MainActivity_native_1setDefaultStreamValues(JNIEnv *env,
                                                                                  jclass type,
                                                                                  jint sampleRate,
                                                                                  jint framesPerBurst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
}
} // extern "C"

extern "C"
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_rhythmgame_MainActivity_passPcmData(JNIEnv *env, jobject thiz,
                                                                 jobject pcm_buffer,
                                                                 jint num_channels,
                                                                 jint sample_rate) {
    // TODO: implement passPcmData()
    nativePlayer -> passPcmData(env, pcm_buffer, num_channels, sample_rate);
}