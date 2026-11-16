# Toolchains

What to install to build and test sihd on every target, and where sbt looks for it. Native Linux builds only need the [README requirements](../README.md#requirements).

sbt finds a cross compiler by its GNU prefix on `PATH`, picks the test runner from the target, and reads the Android paths from environment variables:

| Target | Make flags | sbt looks for | Test runner |
|--------|-----------|---------------|-------------|
| aarch64 | `machine=aarch64` | `aarch64-linux-gnu-gcc` | `qemu-aarch64` |
| arm32 | `machine=arm` | `arm-none-linux-gnueabihf-gcc`, else `arm-linux-gnueabihf-gcc` | `qemu-arm` |
| riscv64 | `machine=riscv64` | `riscv64-linux-gnu-gcc` | `qemu-riscv64` |
| riscv32 | `machine=riscv32` | `riscv32-linux-gnu-gcc` | `qemu-riscv32` |
| musl | `libc=musl` | `x86_64-linux-musl-gcc` | native |
| Windows | `platform=win` | `x86_64-w64-mingw32-g++`, posix threads | `wine` |
| WebAssembly | `platform=web` | `emcc` and `EMSDK` (`emsdk_env.sh`) | `node` |
| Android | `platform=android` | `ANDROID_NDK_PATH`, `ANDROID_SDK_PATH`, `JAVA_HOME`, `gradle` | emulator, see [Android](#android) |

Check what sbt picks for a target without building: `make info machine=aarch64`.

riscv32 is not part of the CI matrix (`scripts/lib.sh`).

## Without root: `scripts/setup/toolchains.sh`

The script downloads everything into one prefix and prints the lines to add to your shell startup file:

```shell
scripts/setup/toolchains.sh                                   # every target
scripts/setup/toolchains.sh --prefix ~/apt/toolchains aarch64 qemu android
```

The default prefix is `$SIHD_TOOLCHAINS`, or `~/.local/share/sihd-toolchains` when it is unset. Targets are `aarch64 arm32 riscv64 riscv32 musl windows web qemu android`. Downloads are kept in `<prefix>/downloads`, so a second run only fetches what is missing.

| Target | Source | Version |
|--------|--------|---------|
| aarch64, arm32 | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | gcc 15.2 |
| riscv64, riscv32 | [riscv-collab/riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain/releases) glibc builds | gcc 16.1 |
| musl | [Bootlin](https://toolchains.bootlin.com/) `x86-64--musl--stable-2026.08-1` | buildroot stable 2026.08 |
| windows | Arch `mingw-w64-gcc` package, extracted | gcc 16.2 |
| qemu | Arch `qemu-user-static` package, extracted | 11.1 |
| web | [emsdk](https://github.com/emscripten-core/emsdk) | emscripten 6.0.13, with node |
| android | Temurin JDK 17, Gradle 9.8.1, Android command line tools | NDK r28 |

The toolchains ship under their own prefixes (`aarch64-none-linux-gnu-`, `riscv64-unknown-linux-gnu-`, `x86_64-buildroot-linux-musl-`); `<prefix>/bin` links them under the names sbt looks for.

The `windows` and `qemu` targets extract Arch packages and need `pacman`. On other distributions install the packages below.

## With the distribution packages

| Target | Arch | Debian / Ubuntu |
|--------|------|-----------------|
| aarch64 | `aarch64-linux-gnu-gcc` | `g++-aarch64-linux-gnu` |
| arm32 | AUR `arm-none-linux-gnueabihf-toolchain-bin` | `g++-arm-linux-gnueabihf` |
| riscv64 | `riscv64-linux-gnu-gcc` | `g++-riscv64-linux-gnu` |
| musl | `scripts/setup/toolchains.sh musl` | same |
| Windows | `mingw-w64-gcc` | `g++-mingw-w64-x86-64-posix`, then `update-alternatives --config x86_64-w64-mingw32-g++` for the posix one |
| WebAssembly | `emscripten` | `emscripten`, or emsdk |
| Runners | `qemu-user-static`, `wine` | `qemu-user-static`, `wine` |

The static arm32 build needs Arm's `arm-none-linux-gnueabihf` toolchain: it ships the static libraries the distribution one lacks.

No distribution packages a musl C++ cross compiler, and musl.cc stops at gcc 11, which has no `std::expected`.

The musl builds, shared and static, also need the musl loader `/lib/ld-musl-x86_64.so.1` on the host: the meson ports of `make dep` run a test program through it, and so do the shared test binaries. Without it `make dep` stops on `Could not invoke sanity check executable`. Install the `musl` package: `sudo pacman -S musl` or `sudo apt install musl`.

## Running the tests of a foreign target

`make itest machine=aarch64` (or `platform=win`, `platform=web`) runs every test through its runner. Static binaries need nothing more; dynamic ones get `QEMU_LD_PREFIX` from `gcc -print-sysroot`.

Tests that start a child process need `binfmt_misc` to know qemu, or the kernel cannot run the foreign child: they fail with `Exec format error` (`TestTestWatchdog`, `TestBtScan.test_no_system_bus`, `TestMessageQueue.test_message_queue_process`, `TestSharedMemory`). Registering needs root, so `scripts/setup/toolchains.sh qemu` does not do it. Once per host:

```shell
sudo pacman -S qemu-user-static qemu-user-static-binfmt   # Arch
sudo apt install qemu-user-static                         # Debian / Ubuntu, registers on install
ls /proc/sys/fs/binfmt_misc                               # qemu-aarch64, qemu-arm, qemu-riscv64 ...
```

A qemu outside `/usr/bin` needs its own `/etc/binfmt.d` entry with the `F` flag. Without root, register inside a user namespace for one run, see [sbt cross-compilation](../site_scons/sbt/docs/cross-compilation.md).

wine 11.22 refuses a shared `LockFileEx` on a write-only handle with `Access denied`, so `TestFileMutex.test_filemutex_shared` fails under it; wine 10.0 passes the same binary.

## Android

The `android` target of the script installs the JDK, Gradle and the Android SDK packages: `platform-tools`, `platforms;android-36`, `build-tools;36.0.0`, `ndk;28.0.13004108`, `emulator` and `system-images;android-35;google_apis;x86_64`. It stops on `sdkmanager --licenses` for you to read and accept the licenses.

```shell
scripts/build/compile_android.sh       # checks the environment, then make dep + make of the android target
make android-emu-setup                 # creates the x86_64 emulator, once
make android-emu-run build/last/demo/util_demo.apk
make android-emu-stop
```

The x86_64 emulator image runs arm64 APKs through its ARM translation, and `/dev/kvm` must be readable and writable by you.

An APK build reads the signing config `keystore.properties` named by `android_sign_config`. `scripts/build/compile_android.sh` generates a throwaway one with `keytool`; a hand-run `make platform=android` needs it first:

```shell
bash -c 'source scripts/lib.sh; ensure_android_sign_config'
```

Gradle must run on JDK 17 to 25: JDK 27 breaks the Android Gradle Plugin 9.1. More in [sbt android](../site_scons/sbt/docs/android.md).
