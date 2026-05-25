# imgproc_lib - C++ Image Processing Library

A full-featured C++17 image processing library with format conversion, QR/barcode generation & reading, text rendering, and compression algorithms. Supports Windows, Linux, and HarmonyOS.

## Features

- **Image Codec**: PNG/BMP/JPEG read/write, auto-detection by magic bytes
- **QR Code**: Generate and read QR codes, support for logo overlay
- **Barcode**: Generate and read 14 barcode formats (Code128, EAN-13, UPC-A, etc.)
- **Text Rendering**: Multi-language text to image (CJK support)
- **Compression**: RLE, DeltaRow, JPEG with quality control
- **Image Transform**: Resize, crop, rotate, flip
- **Dual Implementation**: Windows API (GDI+) and cross-platform (libjpeg-turbo/libpng/FreeType)
- **C API**: Complete C FFI interface for NDK/FFI integration

## Project Structure

```
imgproc_lib/
├── CMakeLists.txt              # Main build file
├── CMakePresets.json           # CMake presets (Windows)
├── conanfile.txt               # Conan dependencies (simple)
├── conanfile.py                # Conan recipe (OHOS cross-compile)
├── profiles/
│   └── ohos-arm64              # Conan OHOS profile
├── build.ps1                   # PowerShell build script
├── build.bat                   # Batch build script
├── include/imgproc/            # Public headers
├── src/                        # Source code
│   └── platform/
│       ├── win_codec.cpp       # Windows GDI+ implementation
│       └── cross_codec.cpp     # Cross-platform implementation
├── tests/                      # Unit tests
├── bench/                      # Benchmarks
├── examples/                   # Demo CLI tool
└── third_party/qrcodegen/      # QR code generator (nanoproject)
```

---

## Build Environment Setup

### Prerequisites (All Platforms)

| Tool | Minimum Version | Install |
|------|----------------|---------|
| **CMake** | >= 3.15 | [cmake.org](https://cmake.org/download/) |
| **Conan 2.x** | >= 2.0 | `pip install conan` |
| **C++ Compiler** | C++17 support | Platform-specific (see below) |

### Step 1: Install Conan

```bash
pip install conan
conan profile detect --force
```

Verify:
```bash
conan --version    # Should be 2.x
```

---

## Windows Build

### Prerequisites

- **Visual Studio 2022** (includes MSVC v143 toolset)
- **Windows SDK** (comes with VS2022)
- **Ninja** (optional, for faster builds)

### Quick Build (Recommended)

```powershell
# Default: Release + static lib + tests + examples
.\build.ps1

# Clean rebuild + run tests
.\build.ps1 -Clean -Test

# Debug build
.\build.ps1 -Config Debug

# Build shared library (DLL)
.\build.ps1 -Shared

# Library only (no tests/examples/benchmarks)
.\build.ps1 -NoTest -NoExample -NoBenchmark

# Verbose output
.\build.ps1 -Verbose
```

### Build Options (build.ps1)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `-Config` | Release | Build configuration: Release, Debug, RelWithDebInfo, MinSizeRel |
| `-Shared` | OFF | Build shared library (DLL) |
| `-NoTest` | OFF | Skip building tests |
| `-NoExample` | OFF | Skip building examples |
| `-NoBenchmark` | OFF | Skip building benchmarks |
| `-CrossPlatform` | OFF | Disable Windows API, use cross-platform impl only |
| `-Clean` | OFF | Clean build directory before building |
| `-Test` | OFF | Run tests after build |
| `-Install` | OFF | Install to directory |
| `-Prefix <path>` | build/install | Install prefix |
| `-Verbose` | OFF | Verbose output |

### Manual CMake Build

```powershell
# 1. Install dependencies
conan install . --output-folder=build --build=missing -s compiler.cppstd=17

# 2. Remove Conan-generated preset (avoids conflict with project preset)
Remove-Item -Force CMakeUserPresets.json, build/CMakePresets.json -ErrorAction SilentlyContinue

# 3. Configure
cmake --preset windows-msvc

# 4. Build
cmake --build build --config Release --parallel

# 5. Test
ctest --preset windows-release --output-on-failure
```

### CMake Presets (Windows)

```
windows-msvc           # Default: VS2022, x64, static
windows-msvc-debug     # Debug configuration
windows-msvc-release   # Release configuration
windows-msvc-shared    # Shared library (DLL)
windows-msvc-cross     # Cross-platform only (no Windows API)
windows-ninja          # Ninja generator (faster builds)
ci-windows             # CI optimized (tests ON, examples OFF)
```

### Build with Visual Studio

Open `build/imgproc_lib.sln` in Visual Studio and build from IDE.

---

## Linux Build

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y ninja-build

# Fedora
sudo dnf install -y ninja-build cmake
```

### Build

```bash
# 1. Install dependencies
conan install . --output-folder=build --build=missing -s compiler.cppstd=17

# 2. Configure
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17 \
  -G Ninja

# 3. Build
cmake --build build --parallel

# 4. Test
ctest --test-dir build --output-on-failure
```

---

## HarmonyOS (OHOS) Build

### Prerequisites

1. **DevEco Studio** (includes OHOS NDK) - see installation below
2. **Conan 2.x** (`pip install conan`)
3. **Ninja** (included in OHOS NDK)

### Step 1: Install DevEco Studio

#### Windows

1. **Download**: https://developer.harmonyos.com/cn/develop/deveco-studio
2. **Install**: Run the installer with default options
3. **Launch DevEco Studio** and complete initial setup
4. **Install HarmonyOS SDK**:
   - Open `File → Settings → SDK`
   - Select `HarmonyOS` tab
   - Check `Native` (C/C++ development)
   - Click `Apply` to download and install

#### macOS

1. **Download**: https://developer.harmonyos.com/cn/develop/deveco-studio
2. **Install**: Drag DevEco Studio to Applications
3. **Launch** and complete setup
4. **Install SDK** via `Preferences → SDK → HarmonyOS → Native`

#### Linux

1. **Download** the Linux version from the same page
2. **Extract** and run `bin/deveco-studio.sh`
3. **Install SDK** via settings

### Step 2: Verify NDK Installation

The build script will auto-detect NDK path. To verify:

```powershell
# PowerShell - check if NDK is found
.\build.ps1 -OHOS
# If NDK not found, you'll see installation instructions

# Manual check
Test-Path "$env:LOCALAPPDATA\Huawei\Sdk\ohos-sdk\native\build\cmake\ohos.toolchain.cmake"
```

If auto-detection fails, set manually:

```powershell
# PowerShell (current session)
$env:OHOS_NDK_HOME = "C:\Program Files\Huawei\Sdk\ohos-sdk\native"

# PowerShell (permanent)
[Environment]::SetEnvironmentVariable("OHOS_NDK_HOME", "C:\Program Files\Huawei\Sdk\ohos-sdk\native", "User")
```

```bash
# Bash/Linux/macOS
export OHOS_NDK_HOME=~/Huawei/Sdk/ohos-sdk/native
```

### Step 3: One-Click Build

```powershell
# Build with auto-detected NDK path
.\build.ps1 -OHOS -Clean

# Specify NDK path explicitly
.\build.ps1 -OHOS -OHOSNdk "C:\Program Files\Huawei\Sdk\ohos-sdk\native" -Clean

# Build shared library (.so)
.\build.ps1 -OHOS -Shared -Clean

# Verbose output (useful for first build)
.\build.ps1 -OHOS -Clean -Verbose
```

### Step 4: Manual Build

```powershell
# 1. Install dependencies via Conan (cross-compile with OHOS profile)
conan install . --output-folder=build --profile profiles/ohos-arm64 --build=missing

# 2. Configure (uses Conan-generated toolchain)
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17 \
  -DUSE_WINDOWS_API=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_BENCHMARKS=OFF \
  -DBUILD_EXAMPLES=OFF

# 3. Build
cmake --build build --parallel
```

### Build Output

```
build/
├── libimgproc_lib.a    # Static library
├── libimgproc_lib.so   # Shared library (if -Shared)
└── include/            # Public headers (for integration)
```

### Integrate into DevEco Studio

1. Copy `build/libimgproc_lib.so` to your project's `libs/<abi>/` directory
2. Copy `include/imgproc/` to your project's `include/` directory
3. In `CMakeLists.txt`:
   ```cmake
   target_link_libraries(entry SHARED imgproc_lib)
   target_include_directories(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
   ```
4. Use the C API from NAPI/ArkTS layer (see `include/imgproc/imgproc_c_api.h`)

### OHOS Conan Profile

The file `profiles/ohos-arm64` defines:

```ini
[settings]
os=OHOS
arch=arm64
compiler=clang
compiler.cppstd=17

[buildenv]
CC=$OHOS_NDK_HOME/llvm/bin/clang
CXX=$OHOS_NDK_HOME/llvm/bin/clang++
```

---

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | Build shared library (.dll/.so) |
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_BENCHMARKS` | ON | Build benchmarks |
| `BUILD_EXAMPLES` | ON | Build example CLI tool |
| `BUILD_LUA_BINDINGS` | OFF | Build Lua bindings |
| `USE_WINDOWS_API` | ON (Windows) / OFF (others) | Enable Windows GDI+ implementation |

---

## Dependencies

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| libpng | 1.6.38 | PNG read/write | zlib |
| libjpeg-turbo | 2.1.5 | JPEG encode/decode | BSD |
| zxing-cpp | 2.1.0 | QR/barcode decode | Apache 2.0 |
| freetype | 2.13.0 | Text rendering | FTL/GPL |
| zint | 2.10.0 | Barcode generation | GPL |
| lua | 5.4.4 | Scripting (optional) | MIT |

All dependencies are managed by Conan. First build downloads and compiles them automatically.

---

## License

MIT License
