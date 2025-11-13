# Building EDN.C on Windows

This guide covers building EDN.C on Windows using various toolchains.

## Prerequisites

EDN.C requires a C11-compliant compiler with SIMD support. Choose one of the following:

### Option 1: Visual Studio (MSVC) - Recommended
- **Visual Studio 2019 or later** (Community Edition is free)
- Includes CMake and all necessary build tools
- Download: https://visualstudio.microsoft.com/downloads/

### Option 2: MinGW-w64 (GCC for Windows)
- **MinGW-w64** with GCC 7.0 or later
- Download: https://www.mingw-w64.org/downloads/
- Recommended distribution: MSYS2 (https://www.msys2.org/)

### Option 3: Clang for Windows
- **LLVM/Clang** 10.0 or later
- Download: https://releases.llvm.org/download.html

---

## Building with CMake (All Compilers)

CMake provides the easiest cross-platform build experience.

### 1. Install CMake
Download and install CMake from https://cmake.org/download/ (version 3.10 or later).

### 2. Build the Library

Open a terminal (PowerShell, Command Prompt, or Developer Command Prompt for VS):

```powershell
# Create build directory
mkdir build
cd build

# Configure (auto-detects compiler)
cmake ..

# Build
cmake --build . --config Release

# Run tests
ctest -C Release
```

For a specific compiler:
```powershell
# MSVC (Visual Studio)
cmake .. -G "Visual Studio 16 2019" -A x64

# MinGW
cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc

# Ninja (fast, works with any compiler)
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

---

## Building with Visual Studio (MSVC)

### Using Developer Command Prompt

1. Open **Developer Command Prompt for VS 2019** (or later)
2. Navigate to the project directory
3. Build:

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Using Visual Studio IDE

1. Open Visual Studio
2. Select **File → Open → CMake...**
3. Open `CMakeLists.txt` from the project root
4. Visual Studio will automatically configure the project
5. Build: **Build → Build All** (or press `Ctrl+Shift+B`)
6. Run tests from the Test Explorer

---

## Building with MinGW (MSYS2)

MSYS2 provides a Unix-like environment on Windows with GCC.

### 1. Install MSYS2
Download from https://www.msys2.org/ and install.

### 2. Install Build Tools
Open the MSYS2 terminal and run:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make
```

### 3. Build
Open **MINGW64** terminal (not MSYS2!) and run:

```bash
cd /path/to/edn.c
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

Or use the traditional Makefile:
```bash
make
make test
```

---

## Building with Make (MinGW/Clang)

If you have `make` available (via MSYS2 or standalone):

```bash
# Build library
make

# Build and run tests
make test

# Build benchmarks
make bench

# Clean
make clean
```

**Note**: The `Makefile` is primarily designed for Unix/macOS. For Windows, CMake is recommended.

---

## SIMD Support on Windows

EDN.C automatically detects and uses SIMD instructions:

### x86_64 (Intel/AMD)
- **SSE4.2** is used by default (available on all modern x86_64 CPUs)
- MSVC: Enabled with `/arch:AVX` (includes SSE4.2)
- GCC/Clang: Enabled with `-msse4.2`

### ARM64 (Windows on ARM)
- **NEON** is enabled by default on ARM64
- MSVC: Automatic
- GCC/Clang: Automatic with `-march=armv8-a`

### Fallback
- Scalar (non-SIMD) code is automatically used on unsupported architectures

---

## Platform-Specific Notes

### MSVC Compiler Differences
EDN.C handles MSVC-specific differences automatically:
- Uses `_BitScanForward` instead of `__builtin_ctz` for bit scanning
- Uses `<intrin.h>` for all intrinsics (instead of separate headers)
- Compatible with `/W4` warning level

### Memory Safety
- **Debug builds** on GCC/Clang include AddressSanitizer and UndefinedBehaviorSanitizer
- MSVC users can enable `/analyze` for static analysis

### Path Separators
- Use forward slashes `/` or double backslashes `\\\\` in paths
- CMake handles path conversion automatically

---

## Troubleshooting

### "SSE intrinsics not found"
- **MSVC**: Update to Visual Studio 2019 or later
- **MinGW**: Ensure you're using MinGW-w64 (not old MinGW)

### "CMake not found"
- Add CMake to your PATH, or use full path: `C:\Program Files\CMake\bin\cmake.exe`

### "Compiler not found"
- **MSVC**: Run from "Developer Command Prompt for VS"
- **MinGW**: Use MINGW64 terminal (not MSYS2 terminal)

### Tests fail with "cannot open file"
- Windows may require `.\test\test_name.exe` instead of `./test/test_name`
- Use CMake's `ctest` for automatic test discovery

---

## Integration into Your Project

### Static Library
After building, link against `edn.lib` (MSVC) or `libedn.a` (MinGW):

**CMake**:
```cmake
target_link_libraries(your_app edn)
```

**MSVC Command Line**:
```cmd
cl your_app.c /I path\to\edn.c\include edn.lib
```

**MinGW/GCC**:
```bash
gcc your_app.c -I path/to/edn.c/include -L path/to/edn.c/build -ledn
```

### Include Header
```c
#include <edn.h>
```

---

## Building for Release

For production builds, use Release configuration:

**CMake**:
```powershell
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**MSVC**:
```cmd
cmake --build . --config Release
```

This enables full optimizations (`/O2` on MSVC, `-O2` on GCC/Clang).

---

## Getting Help

- **Issues**: https://github.com/yourusername/edn.c/issues
- **Documentation**: See `README.md` and `docs/DESIGN.md`
- **Examples**: See `examples/` directory

---

## Performance Notes

On Windows, EDN.C achieves comparable performance to Unix/macOS:
- **SIMD-accelerated** parsing for hot paths
- **Zero-copy** design minimizes allocations
- **Arena allocator** reduces allocation overhead

Benchmark on Windows with:
```powershell
.\build\bench_integration.exe
```

Typical performance: **1-30 ns/op** for common operations on modern CPUs.
