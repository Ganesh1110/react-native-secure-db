package com.securedb;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Log;
import java.security.KeyStore;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import java.security.SecureRandom;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

public class KeyStoreManager {
    private static final String TAG = "SecureDB_KeyStore";
    private static final String KEY_ALIAS = "com.securedb.masterkey";
    private static final String ANDROID_KEYSTORE = "AndroidKeyStore";
    private static final int GCM_IV_LENGTH = 12;
    private static final int GCM_TAG_LENGTH = 128;

    private static byte[] cachedKey = null;
    private static android.content.Context staticContext = null;

    public static void setContext(android.content.Context context) {
        staticContext = context;
    }

    public static byte[] getMasterKey() {
        if (cachedKey != null) {
            return cachedKey;
        }

        try {
            Log.i(TAG, "Fetching master key from AndroidKeyStore...");
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEYSTORE);
            keyStore.load(null);

            if (!keyStore.containsAlias(KEY_ALIAS)) {
                Log.i(TAG, "Generating new hardware-backed master key...");
                KeyGenerator keyGenerator = KeyGenerator.getInstance(
                        KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEYSTORE);
                
                keyGenerator.init(new KeyGenParameterSpec.Builder(
                        KEY_ALIAS,
                        KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                        .setKeySize(256)
                        .setUserAuthenticationRequired(false)
                        .build());
                
                keyGenerator.generateKey();
            }

            SecretKey secretKey = (SecretKey) keyStore.getKey(KEY_ALIAS, null);
            if (secretKey == null) {
                Log.e(TAG, "Failed to retrieve secret key from KeyStore");
                return getFallbackKey("keystore_retrieval_failed");
            }
            
            // Try to load a derived key from storage first (which is wrapped by the Keystore key)
            cachedKey = loadDerivedKey(secretKey);
            
            if (cachedKey == null && staticContext != null) {
                Log.i(TAG, "Generating and storing new derived key...");
                cachedKey = generateAndStoreDerivedKey(secretKey);
            }
            
            if (cachedKey == null) {
                Log.w(TAG, "Using deterministic fallback key (no context or storage failure)");
                cachedKey = getFallbackKey("no_context_or_storage_failure");
            }
            
            return cachedKey;
        } catch (Exception e) {
            Log.e(TAG, "Critical error in KeyStoreManager: " + e.getMessage(), e);
            return getFallbackKey("critical_exception_" + e.getClass().getSimpleName());
        }
    }

    private static byte[] getFallbackKey(String reason) {
        Log.w(TAG, "Returning fallback key due to: " + reason);
        byte[] fallback = new byte[32];
        for (int i = 0; i < 32; i++) fallback[i] = (byte) (0xDB ^ i ^ (reason.length() & 0xFF));
        return fallback;
    }

    private static byte[] loadDerivedKey(SecretKey hardwareKey) {
        if (staticContext == null) return null;
        File keyFile = new File(staticContext.getFilesDir(), "securedb.key");
        if (!keyFile.exists() || keyFile.length() <= GCM_IV_LENGTH) {
            Log.d(TAG, "Derived key file does not exist or is too small");
            return null;
        }

        try (FileInputStream fis = new FileInputStream(keyFile)) {
            byte[] iv = new byte[GCM_IV_LENGTH];
            int readIv = fis.read(iv);
            if (readIv != GCM_IV_LENGTH) return null;
            
            int encryptedLen = (int) keyFile.length() - GCM_IV_LENGTH;
            byte[] encryptedKey = new byte[encryptedLen];
            int readEnc = fis.read(encryptedKey);
            if (readEnc != encryptedLen) return null;

            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            GCMParameterSpec parameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH, iv);
            cipher.init(Cipher.DECRYPT_MODE, hardwareKey, parameterSpec);
            return cipher.doFinal(encryptedKey);
        } catch (Exception e) {
            Log.e(TAG, "Failed to load/decrypt derived key: " + e.getMessage());
            return null;
        }
    }

    private static byte[] generateAndStoreDerivedKey(SecretKey hardwareKey) {
        if (staticContext == null) return null;
        try {
            byte[] derivedKey = new byte[32];
            new SecureRandom().nextBytes(derivedKey);

            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            byte[] iv = new byte[GCM_IV_LENGTH];
            new SecureRandom().nextBytes(iv);
            
            GCMParameterSpec parameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH, iv);
            cipher.init(Cipher.ENCRYPT_MODE, hardwareKey, parameterSpec);
            byte[] encryptedKey = cipher.doFinal(derivedKey);

            File keyFile = new File(staticContext.getFilesDir(), "securedb.key");
            try (FileOutputStream fos = new FileOutputStream(keyFile)) {
                fos.write(iv);
                fos.write(encryptedKey);
            }

            return derivedKey;
        } catch (Exception e) {
            Log.e(TAG, "Failed to generate/store derived key: " + e.getMessage());
            return null;
        }
    }
}
