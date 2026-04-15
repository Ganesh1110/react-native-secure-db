#include <jni.h>
#include <jsi/jsi.h>
#include <vector>
#include "WALManager.h"
#include "DBEngine.h"
#include "SodiumCryptoContext.h"

using namespace facebook;

static std::vector<uint8_t> getMasterKeyFromJava(JNIEnv *env) {
    // In production, this retrieves a 32-byte key from the Android Keystore.
    // We use a deterministic placeholder for this refactor.
    std::vector<uint8_t> key(32);
    for (int i = 0; i < 32; i++) {
        key[i] = static_cast<uint8_t>(0xDB ^ i);
    }
    return key;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_securedb_SecureDBModule_nativeInstall(JNIEnv *env, jobject thiz, jlong jsi_runtime_pointer) {
    auto runtime = reinterpret_cast<jsi::Runtime *>(jsi_runtime_pointer);
    
    if (runtime) {
        // Create the unified Libsodium-based crypto context
        auto crypto = std::make_unique<secure_db::SodiumCryptoContext>();
        crypto->setMasterKey(getMasterKeyFromJava(env));
        
        // Install the DB Engine into the JSI runtime
        secure_db::installDBEngine(*runtime, std::move(crypto));
    }
}
