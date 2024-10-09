package com.google.oboe.samples.rhythmgame;
import android.content.Context;
import android.content.res.AssetManager;
import android.os.Environment;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class FileUtils {

    /**
     * Copies a file from the assets directory to the app's external files directory.
     *
     * @param context     The application context.
     * @param assetName   The name of the asset file (e.g., "sample.mp3").
     * @param outputDir   The directory in external storage where the file will be copied.
     * @return The `File` object pointing to the copied file.
     * @throws IOException If an I/O error occurs.
     */
    public static File copyAssetToExternalStorage(Context context, String assetName, String outputDir) throws IOException {
        AssetManager assetManager = context.getAssets();

        // Create the output directory if it doesn't exist
        File dir = new File(context.getExternalFilesDir(null), outputDir);
        if (!dir.exists()) {
            dir.mkdirs();
        }

        // Create the output file
        File outFile = new File(dir, assetName);

        // Open the asset
        InputStream in = assetManager.open(assetName);
        FileOutputStream out = new FileOutputStream(outFile);

        // Copy the asset to the output file
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }

        // Close streams
        in.close();
        out.flush();
        out.close();

        return outFile;
    }
}
