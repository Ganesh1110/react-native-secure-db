package com.turbodb;

import com.facebook.fbreact.specs.NativeTurboDBSpec;
import com.facebook.react.bridge.ReactApplicationContext;
import java.io.File;

public class TurboDBModule extends NativeTurboDBSpec {
  public static final String NAME = "TurboDB";

  static {
    System.loadLibrary("react-native-turbo-db");
  }

  public TurboDBModule(ReactApplicationContext reactContext) {
    super(reactContext);
    KeyStoreManager.init(reactContext);
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public void install() {
    long jsiRuntimePointer = getReactApplicationContext().getJavaScriptContextHolder().get();
    if (jsiRuntimePointer != 0) {
      nativeInstall(jsiRuntimePointer, 1);
    }
  }

  @Override
  public String getDocumentsDirectory() {
    File docsDir = getReactApplicationContext().getFilesDir();
    return docsDir.getAbsolutePath();
  }

  @Override
  public String getVersion() {
    return "0.1.0";
  }

  private native void nativeInstall(long jsiRuntimePointer, int installMode);
}
