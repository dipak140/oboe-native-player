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

package com.google.oboe.samples.rhythmgame;

import android.content.Context;
import android.content.res.AssetManager;
import androidx.appcompat.app.AppCompatActivity;

import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;
import com.arthenica.ffmpegkit.FFmpegKit;
import com.arthenica.ffmpegkit.FFmpegSession;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        if (BuildConfig.FLAVOR == "ffmpegExtractor"){
            System.loadLibrary("avutil");
            System.loadLibrary("swresample");
            System.loadLibrary("avcodec");
            System.loadLibrary("avformat");
        }
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setDefaultStreamValues(this);
    }

    protected void onResume(){
        super.onResume();
        processAudio();
    }

    protected void onPause(){
        native_onStop();
        super.onPause();
    }

    static void setDefaultStreamValues(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1){
            AudioManager myAudioMgr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int defaultSampleRate = Integer.parseInt(sampleRateStr);
            String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);

            native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst);
        }
    }

    public void convertMp3ToPcm() {
        String outputDir = "samples";
        File dir = new File(getExternalFilesDir(null), outputDir);

        if (!dir.exists()) {
            dir.mkdirs();
        }

        File outFile = new File(dir, "Music.pcm");
        String mp3FilePath = getExternalFilesDir(null) + "/" + outputDir + "/Music.mp3";
        String pcmFilePath = outFile.getAbsolutePath();
        String cmdStr = "-i " + mp3FilePath + " -f f16le -acodec pcm_f32le -ar 44100 -ac 1 " + pcmFilePath;
        FFmpegSession rc = FFmpegKit.execute(cmdStr);
        if (rc.getReturnCode().isValueSuccess()) {
            // Conversion successful
        } else {
            // Handle error
        }
    }

    private void processAudio() {
        convertMp3ToPcm();

        String mp3FilePath = getExternalFilesDir(null) + "/" + "samples" + "/Music.mp3";
        String pcmFilePath = getExternalFilesDir(null) + "/" + "samples" + "/Music.pcm";

        try {
            byte[] pcmData = readPcmData(pcmFilePath);
            ByteBuffer pcmBuffer = ByteBuffer.allocateDirect(pcmData.length);
            pcmBuffer.put(pcmData);

            // Reset buffer position to zero before passing to native code
            pcmBuffer.flip();

            // Assuming stereo audio at 44100 Hz
            native_onStart(getAssets(), pcmBuffer, 1, 44100);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private native void native_onStart(AssetManager assetManager, ByteBuffer pcmBuffer, int numChannels, int sampleRate);
    private native void native_onStop();
    private static native void native_setDefaultStreamValues(int defaultSampleRate,
                                                      int defaultFramesPerBurst);
    public native void passPcmData(ByteBuffer pcmBuffer, int numChannels, int sampleRate);


    public byte[] readPcmData(String pcmFilePath) throws IOException {
        File pcmFile = new File(pcmFilePath);
        long fileLength = pcmFile.length();

        if (fileLength > Integer.MAX_VALUE) {
            throw new IOException("File is too large to read into a byte array.");
        }

        byte[] pcmData = new byte[(int) fileLength];
        FileInputStream fis = null;
        try {
            fis = new FileInputStream(pcmFile);
            int offset = 0;
            int bytesRead;

            // Keep reading until the entire file is read
            while (offset < pcmData.length &&
                    (bytesRead = fis.read(pcmData, offset, pcmData.length - offset)) != -1) {
                offset += bytesRead;
            }

            // Check if we read the entire file
            if (offset < pcmData.length) {
                throw new IOException("Could not completely read the file " + pcmFilePath);
            }

        } finally {
            if (fis != null) {
                fis.close();
            }
        }

        return pcmData;
    }

}
