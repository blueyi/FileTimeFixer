# FileTimeFixer (C++)

C++ implementation using Exiv2; supports EXIF read/write for JPEG/PNG/HEIC/RAW and similar. **Video** (MP4, MOV, etc.): reads/writes QuickTime `creation_time` via **ffprobe** and **ffmpeg** (must be on PATH). Single executable, suitable as the **main implementation** (broadest format support).

## Build

**Option A – auto env (recommended)**  
Run the environment init script for your OS, then build:

```powershell
# Windows – from repo root or cpp/
# Option 1: use the .cmd wrapper (avoids execution policy)
cpp\init_env.cmd

# Option 2: PowerShell with bypass (if you prefer running .ps1 directly)
powershell -NoProfile -ExecutionPolicy Bypass -File cpp\init_env.ps1
```
Then build:
```powershell
cd cpp\build
cmake -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" ..
cmake --build . --config Release
```

```bash
# Linux / macOS (from repo root)
./cpp/init_env.sh
cd cpp/build && cmake .. && cmake --build .
```

**Option B – manual**  
Install Exiv2 and FFmpeg yourself, then:

```bash
cd cpp
mkdir build && cd build
cmake ..
cmake --build .
```

- **Dependencies**: CMake 3.10+, C++20, [Exiv2](https://www.exiv2.org/), FFmpeg (for video metadata). The **init scripts** (`cpp/init_env.ps1` on Windows, `cpp/init_env.sh` on Linux/macOS) can install them: Windows uses vcpkg for Exiv2 and downloads a prebuilt FFmpeg zip; Linux/macOS use the system package manager (apt/dnf/pacman/brew).

- **Runtime**: On Windows, place `exiv2.dll` next to `FileTimeFixer.exe`. CMake tries to copy it at build time; if you see `exiv2.dll not found` or EXIF read/write errors:
  - **Option 1 (vcpkg)**: Copy from your vcpkg install, e.g. `vcpkg_installed/x64-windows/bin/exiv2.dll` or `installed/x64-windows/bin/exiv2.dll`, into `cpp/build/Debug/` (same folder as `FileTimeFixer.exe`).
  - **Option 2 (official build)**: Download the latest Windows 64-bit package from [Exiv2 Releases](https://github.com/Exiv2/exiv2/releases) (e.g. `exiv2-0.28.7-2022msvc-AMD64.zip`), extract and copy `exiv2.dll` into the exe directory. Use a build that matches your compiler (MSVC 2022 zip for VS2022); for MinGW, build Exiv2 with MinGW or keep using vcpkg’s DLL.
  - If you still get "Invalid argument" or "EXIF read failed", the program will fall back to **in-memory (MemIo)** EXIF read/write; files over 100MB skip MemIo.

- **Video metadata (MP4/MOV)**: For reading/writing `creation_time` in videos, **ffprobe** and **ffmpeg** must be on your PATH. If missing, videos are still processed using filename time only and file system time is set; metadata will not be written.

### CMake not found in Git Bash on Windows

After installing CMake (e.g. via winget), Git Bash may not have it in PATH. Either:

1. **Current shell** (from `cpp`): run `source env_cmake.sh`, then `cd build && cmake .. && cmake --build .`.
2. **Or** close and reopen Cursor/terminal and try `cmake --version`.
3. **Or** use PowerShell/CMD, `cd cpp\build`, then run `cmake ..` and `cmake --build .`.

## Usage

```bash
./FileTimeFixer              # Use default test folder (see kDefaultTestFolder in Main.cpp)
./FileTimeFixer <directory>
./FileTimeFixer --test       # Run tests aligned with test_spec/
```

- **If you see "abort() has been called" in Debug**: Exiv2 can hit asserts on some images in Debug. Use **Release** for real directories: `cmake --build . --config Release`, then run `Release/FileTimeFixer.exe` (Windows) or `./FileTimeFixer` (Linux default is Release).

The program sets the Windows console to UTF-8 (CP 65001) on startup. If you see garbled output, run `chcp 65001` in the terminal first.

## Tests

Test cases match the **test_spec/** at the repo root so C++ and Python behaviour stay in sync.
