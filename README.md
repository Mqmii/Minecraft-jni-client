JNI Cheat/Dll with ImGui overlay and runtime hooks.
Works on 1.21.11 and 1.21.11 Fabric fabric-loader-0.18.4-1.21.11

## Features

- Fast Break
- Fast Place
- Auto Sprint
- Anti Knockback
- TriggerBot
- Tracer

## Controls

- `INSERT`: Toggle menu
- `DELETE`: Exit/unload

## Build

Requirements:

- CMake 3.20 or newer
- A Windows C++ toolchain compatible with CMake
  - MinGW-w64
  - Visual Studio 2022 Build Tools or a full Visual Studio installation
- A JDK installation available to CMake so `find_package(JNI REQUIRED)` can resolve JNI headers and libraries

Recommended shell:

- A shell where your MinGW toolchain is available in `PATH`
- `Developer PowerShell for Visual Studio`
- `x64 Native Tools Command Prompt for VS`

Configure and build:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If `cmake` is not recognized, CMake is either not installed or not available in `PATH`.

Notes:

- On multi-config generators such as Visual Studio, `--config Release` is required for the build step.
- On single-config generators, `CMAKE_BUILD_TYPE=Release` controls the build type during configuration.
- The resulting DLL is written under the selected build directory.
- If you use MinGW, make sure CMake is configuring against the same MinGW toolchain you intend to build with.
