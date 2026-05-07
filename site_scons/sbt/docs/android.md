# Android APK Build Guide

Build Android APKs from any sbt module's demo or binary.

## Prerequisites

### 1. Java Development Kit (JDK 17+)

AGP (Android Gradle Plugin) requires JDK 17 or higher.

```bash
# Arch Linux
sudo pacman -S jdk17-openjdk

# Ubuntu / Debian
sudo apt install openjdk-17-jdk

# Fedora
sudo dnf install java-17-openjdk-devel
```

Set `JAVA_HOME` if multiple JDKs are installed:
```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
```

### 2. Android SDK

Install the Android command-line tools from https://developer.android.com/studio#command-line-tools-only

```bash
mkdir -p /path/to/android-sdk/cmdline-tools
unzip commandlinetools-linux-*.zip -d /path/to/android-sdk/cmdline-tools/
mv /path/to/android-sdk/cmdline-tools/cmdline-tools /path/to/android-sdk/cmdline-tools/latest
```

Install required SDK components:
```bash
/path/to/android-sdk/cmdline-tools/latest/bin/sdkmanager \
    "platform-tools" \
    "platforms;android-36" \
    "build-tools;36.0.0" \
    "ndk;28.0.13004108"
```

Accept licenses:
```bash
/path/to/android-sdk/cmdline-tools/latest/bin/sdkmanager --licenses
```

### 3. Gradle (system-wide, no wrapper)

sbt uses Gradle directly — no `gradlew` wrapper is embedded in templates.

```bash
# Arch Linux
sudo pacman -S gradle

# Or install manually from https://gradle.org/releases/
```

### 4. Environment Variables

| Variable | Default | Required | Description |
|----------|---------|----------|-------------|
| `ANDROID_NDK_PATH` | _(none)_ | **yes** | Path to NDK (e.g. `/path/to/android-sdk/ndk/28.0.13004108`) |
| `ANDROID_SDK_PATH` | `/path/to/android-sdk` | recommended | Path to SDK root |
| `JAVA_HOME` | _(auto-detected)_ | if multiple JDKs | JDK 17+ for Gradle |
| `ANDROID_API` | `24` | no | Minimum Android API level |

```bash
# Add to .bashrc / .zshrc
export ANDROID_NDK_PATH=/path/to/android-sdk/ndk/28.0.13004108
export ANDROID_SDK_PATH=/path/to/android-sdk
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
```

## Tested Versions

These are the versions known to work together:

| Component | Version |
|-----------|---------|
| Gradle | 9.3.1 |
| Android Gradle Plugin (AGP) | 9.1.0 |
| Android NDK | 28.0.13004108 (r28) |
| compileSdk | 36 |
| minSdk | 24 (Android 7.0) |
| targetSdk | 36 |
| JDK | 17+ (tested with 17 and 25) |
| Kotlin | bundled with AGP 9.x (no separate plugin) |

## Building

### Install dependencies (vcpkg)

```bash
make dep m=imgui demo=1 platform=android machine=arm64 static=1
```

### Build APK

```bash
make m=imgui platform=android machine=arm64 static=1 demo=1
```

Output: `build/arm64-android-bionic/ndk-28/fast/demo/<name>.apk`

### Other architectures

| `machine=` | Android ABI | Notes |
|------------|------------|-------|
| `arm64` / `aarch64` | arm64-v8a | Most common (default) |
| `arm32` | armeabi-v7a | Legacy 32-bit ARM |
| `x86_64` | x86_64 | Emulator / ChromeOS |
| `x86` | x86 | Legacy emulator |

```bash
make m=imgui platform=android machine=arm32 static=1 demo=1
```

### Install on device

```bash
adb install build/arm64-android-bionic/ndk-28/fast/demo/imgui_opengl3_android_demo.apk
# or via build/last symlink:
adb install build/last/demo/imgui_opengl3_android_demo.apk
```

## How It Works

### Template system

sbt provides default Android project templates in `site_scons/sbt/android/`. When building a demo or binary for `platform=android`, sbt:

1. Copies the default Gradle project templates to a staging directory
2. If the module provides an override at `<module>/android/`, overlays those files on top
3. Replaces `__SBT_*__` placeholders in all text files
4. Compiles C++ sources into a `.so` (SharedLibrary via NDK clang)
5. Copies the `.so` into the staged project's `jniLibs/<abi>/`
6. Runs `gradle assembleDebug`
7. Copies the resulting APK to the build output directory

### Two modes

**Terminal mode** (default): For non-graphical programs. The app displays a scrollable monospace terminal view showing captured `stdout` and `stderr` output. The user's `main()` runs in a background thread.

**NativeActivity mode** (override): For graphical apps (e.g. ImGui). Uses `android_native_app_glue` and `android_main()` entry point. The module provides a custom `AndroidManifest.xml` and `MainActivity.kt`.

### Placeholders

| Placeholder | Replaced with |
|------------|---------------|
| `__SBT_PROJECT_NAME__` | Demo/binary name |
| `__SBT_NAMESPACE__` | Java namespace (defaults to `<app-name>.android.terminal`) |
| `__SBT_APP_ID__` | Application ID (namespace + sanitized name) |
| `__SBT_APP_LABEL__` | Display name in launcher |
| `__SBT_LIB_NAME__` | Shared library name (without lib prefix / .so suffix) |
| `__SBT_ABI__` | NDK ABI string (e.g. `arm64-v8a`) |

## Module Override

To customize the Android packaging for your module, create a `<module>/android/` directory with:

### config.py (required for override)

```python
namespace = "com.example.myapp"   # Java package namespace
native_activity = True            # True for NativeActivity, False for terminal mode
```

### Override files

Any file placed in `<module>/android/` (except `config.py`) is copied on top of the default templates. Typical overrides:

```
mymodule/android/
├── config.py
└── app/src/main/
    ├── AndroidManifest.xml       # Custom manifest
    └── java/com/example/myapp/
        └── MainActivity.kt       # Custom Activity
```

### Example: imgui override

```
mymodule/android/
├── config.py                     # namespace="com.example.imgui", native_activity=True
└── app/src/main/
    ├── AndroidManifest.xml       # NativeActivity with configChanges
    └── java/com/example/imgui/
        └── MainActivity.kt       # NativeActivity + keyboard/unicode JNI
```

## Troubleshooting

### "SDK location not found"

Set `ANDROID_SDK_PATH`:
```bash
export ANDROID_SDK_PATH=/path/to/android-sdk
```

### Gradle version mismatch

AGP 9.1.0 requires Gradle 9.x. Check with `gradle --version`.

### Missing NDK

```bash
export ANDROID_NDK_PATH=/path/to/android-sdk/ndk/28.0.13004108
```

### APK crashes on launch

Check logcat for native crash details:
```bash
adb logcat -s <app-name>:* AndroidRuntime:* DEBUG:*
```

The terminal bridge wraps `main()` in a try/catch and reports uncaught exceptions. For signal crashes (SIGSEGV, etc.), check the `DEBUG` tag in logcat.
