<div align="center">

# RE5 Recompiled

**An unofficial static recompilation of Resident Evil 5 (Xbox 360) for Android and Linux**

[Third-party notices](THIRD_PARTY_NOTICES.md) · [License: GPL-3.0](LICENSE)

</div>

> [!IMPORTANT]
> This is a non-commercial fan project for interoperability and preservation research. It is **not** affiliated
> with, endorsed by or sponsored by Capcom, Microsoft, or any other rights holder. **This repository contains no
> game files and no game code.** Any build you make needs files extracted from your own legally obtained copy of
> the game. See [Legal](#legal).

## Table of contents

- [Overview](#overview)
- [How it works](#how-it-works)
- [Project status](#project-status)
- [Requirements](#requirements)
- [Building from source](#building-from-source)
- [Running on Android](#running-on-android)
- [Repository layout](#repository-layout)
- [Contributing](#contributing)
- [Credits](#credits)
- [Legal](#legal)
- [License](#license)

## Overview

RE5 Recompiled translates the PowerPC code of the Xbox 360 executable of Resident Evil 5 into C++ ahead of time
and compiles it into a native program for ARM64 Android phones and x86-64 Linux. It is built on the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), which provides the Xbox 360 kernel, the runtime and the
emulation of the console's GPU on Vulkan.

The Android application is a launcher that lets you choose your own game files, with graphics and performance
settings, touch controls and gamepad support. The launcher is adapted from a GPL-3.0 project (see [NOTICE](NOTICE)).

## How it works

| Part | How it runs |
|---|---|
| Game code (CPU) | Recompiled ahead of time: the PowerPC instructions of `default.xex` become C++, built into a native library. There is no interpreter or JIT for the game code. |
| Xbox 360 kernel and system libraries | Reimplemented by the ReXGlue SDK (derived from the Xenia project). |
| GPU (Xenos) | Emulated by the SDK on Vulkan. Shaders are translated while the game runs. |
| Audio, input, video | ReXGlue SDK and SDL3. |

The code generated from the game is produced on your own machine from your own copy of the game. It is excluded
from this repository by `.gitignore`.

## Project status

Early stage. **Milestone: the game now boots and runs on Android**, tested on a device with a Qualcomm Snapdragon 8s Gen 4.

- **Known issues:** performance is around 10 FPS after the game starts; using an imported Turnip driver can cause
  severe graphical corruption.
- **Next:** improve performance and graphics compatibility, then continue testing on Android.

## Requirements

**To build**

- Linux x86-64, Clang 20, CMake 3.25 or later, Ninja, Git, Python 3.8 or later.
- About 20 GB of free disk space and 12 GB of RAM or more (the generated code is large).
- For Android: Java 17 or later, the Android SDK with NDK 28.2.13676358 and CMake 3.30.5.

**To play**

- Android 8.0 or later on an ARM64 processor, with a GPU that supports Vulkan 1.1 or later.
- Tested device: Qualcomm Snapdragon 8s Gen 4.
- Your own copy of Resident Evil 5 for Xbox 360, dumped from your own disc.

## Building from source

1. **Put your own `default.xex`** into `app/`. Extract it from your disc image with an Xbox 360 extraction tool.
   The file is git-ignored.

2. **Build the ReXGlue CLI** from `sdk/`:

   ```sh
   cmake -S sdk -B sdk/out/linux-amd64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20
   cmake --build sdk/out/linux-amd64 --target rexglue
   ```

3. **Run code generation.** This writes the C++ into `app/generated` (git-ignored) and logs to `app/codegen.log`:

   ```sh
   REXGLUE=$PWD/sdk/out/linux-amd64/rexglue tools/codegen.sh app
   ```

   The first run usually reports unresolved functions. That is expected at this stage. See the manifest
   (`app/re5_manifest.toml`) for the function boundaries already added.

4. **Build the game** once code generation completes:

   ```sh
   cmake -S app -B app/out/linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 -DREXSDK_DIR="$PWD/sdk"
   cmake --build app/out/linux
   ```

## Running on Android

The game now boots and runs on Android, tested on Qualcomm Snapdragon 8s Gen 4. Performance is currently around
10 FPS after startup, and imported Turnip drivers may cause severe graphical corruption. Use your own game files;
the project does not distribute game builds or content.

## Repository layout

| Path | Contents |
|---|---|
| `app/` | The game's application: the app skeleton (`src/`), code generation manifest and configuration |
| `android/` | The Android application: launcher, game activity, touch controls, native glue |
| `sdk/` | The ReXGlue SDK |
| `tools/` | Scripts for this project (code generation wrapper) |

## Contributing

Bug reports and pull requests are welcome. Please:

- never attach or link game files, dumps, generated code, or builds;
- do not ask for game files or builds: such requests will be closed;
- describe your setup (OS, GPU, Android version if relevant) when reporting a problem.

## Credits

- **[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)** by Tom Clay: the recompiler and runtime this project
  is built on.
- **[Xenia](https://xenia.jp)** by Ben Vanik and the Xenia contributors: the Xbox 360 research the SDK's kernel
  and GPU emulation derive from.
- The Android launcher is adapted from a GPL-3.0 project by [Gabrick75](https://github.com/gabrick75). See
  [NOTICE](NOTICE).
- The authors of the libraries listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Legal

- **No game content in the repository.** This repository contains only original source code and open-source code
  under the licenses listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). It does not contain, and will
  never contain, the game's executable, data files, assets, audio, video, artwork, logos or code generated from
  the game.
- **Own copy required.** You must own a legitimate copy of the game and use your own dump of it. This project
  does not provide, and does not explain how to obtain, copies of the game.
- **No Microsoft code.** This project does not include code from the Xbox development kit or other proprietary
  Microsoft software.
- **Trademarks.** Resident Evil is a trademark of Capcom Co., Ltd. Xbox and Xbox 360 are trademarks of the
  Microsoft group of companies. All other trademarks belong to their respective owners. They are used here only
  to identify the game and platform this software is compatible with; no endorsement is implied.
- **Not for commercial use or piracy.** Do not sell this project or builds of it, and do not use it to distribute
  or play unauthorized copies of the game.
- **No warranty.** This software is provided "as is", without warranty of any kind, as stated in sections 15 and
  16 of the GPL-3.0. Use it at your own risk.

## License

RE5 Recompiled is free software, licensed under the [GNU General Public License v3.0](LICENSE). The ReXGlue SDK in
`sdk/` and this project's changes to it are licensed under the BSD-3-Clause license. Third-party components keep
their own licenses, listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
