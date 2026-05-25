#Requires -Version 5.1
<#
.SYNOPSIS
    imgproc_lib unified build script

.DESCRIPTION
    Build script with options to control what gets built.
    All output goes to build/ directory.

.EXAMPLE
    .\build.ps1                          # Default: Release + static lib + tests + examples
    .\build.ps1 -Config Debug            # Debug build
    .\build.ps1 -Shared                  # Dynamic library (DLL)
    .\build.ps1 -NoTest -NoExample       # Library only
    .\build.ps1 -Lua                     # Enable Lua bindings
    .\build.ps1 -Clean                   # Clean and rebuild
    .\build.ps1 -Test                    # Build and run tests
    .\build.ps1 -Install -Prefix ./dist  # Install to directory
    .\build.ps1 -CrossPlatform           # Disable Windows API
    .\build.ps1 -OHOS -OHOSNdk "C:\ohos-sdk\native"  # Cross-compile for HarmonyOS
    .\build.ps1 -Verbose                 # Verbose build output
#>

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [switch]$Shared,
    [switch]$NoTest,
    [switch]$NoExample,
    [switch]$NoBenchmark,
    [switch]$Lua,
    [switch]$CrossPlatform,
    [switch]$OHOS,
    [string]$OHOSNdk = "",

    [switch]$Clean,
    [switch]$Test,
    [switch]$Install,
    [string]$Prefix = "",
    [switch]$Verbose,

    [string]$Generator = "",
    [string]$Arch = "x64",
    [string]$Toolset = ""
)

$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"
$usePreset = $false

function Write-Info($msg) {
    Write-Host "[INFO] $msg" -ForegroundColor Cyan
}

function Write-Ok($msg) {
    Write-Host "[OK]   $msg" -ForegroundColor Green
}

function Write-Err($msg) {
    Write-Host "[ERR]  $msg" -ForegroundColor Red
}

function Write-Warn($msg) {
    Write-Host "[WARN] $msg" -ForegroundColor Yellow
}

function Find-OHOS-Ndk {
    <#
    .SYNOPSIS
        Auto-detect OHOS NDK installation path.
    .DESCRIPTION
        Searches common installation paths for HarmonyOS NDK:
        1. DevEco Studio default paths
        2. CommandConfig tool output
        3. Registry (Windows)
        4. Huawei SDK Manager paths
    .OUTPUTS
        NDK path string, or empty string if not found.
    #>
    $candidates = @()

    # --- Windows ---
    if ($IsWindows -or ($env:OS -eq "Windows_NT")) {
        # DevEco Studio default install paths
        $localSdk = "$env:LOCALAPPDATA\Huawei\Sdk\ohos-sdk"
        if (Test-Path $localSdk) { $candidates += $localSdk }

        $programSdk = "${env:ProgramFiles}\Huawei\Sdk\ohos-sdk"
        if (Test-Path $programSdk) { $candidates += $programSdk }

        $programSdkX86 = "${env:ProgramFiles(x86)}\Huawei\Sdk\ohos-sdk"
        if (Test-Path $programSdkX86) { $candidates += $programSdkX86 }

        # User-configured SDK path (from hdc/hvigor config)
        $userSdk = "$env:USERPROFILE\Huawei\Sdk\ohos-sdk"
        if (Test-Path $userSdk) { $candidates += $userSdk }

        # Try hdc tool (usually in NDK or SDK)
        $hdcCmd = Get-Command "hdc" -ErrorAction SilentlyContinue
        if ($hdcCmd) {
            $hdcDir = Split-Path (Resolve-Path $hdcCmd.Source -ErrorAction SilentlyContinue) -ErrorAction SilentlyContinue
            # hdc is in {sdk}/native/hdc/ or {ndk}/default/openharmony/hdc/
            $sdkFromHdc = (Get-Item $hdcDir).Parent.Parent.FullName
            if (Test-Path "$sdkFromHdc\native\build\cmake\ohos.toolchain.cmake") {
                $candidates += "$sdkFromHdc\native"
            }
        }

        # Try CommandConfig tool (DevEco Studio bundled)
        $cmdConfig = Get-Command "command-config" -ErrorAction SilentlyContinue
        if (-not $cmdConfig) {
            $cmdConfigPaths = @(
                "$env:LOCALAPPDATA\Huawei\Sdk\hmscore\command-line-tools\command-config",
                "$env:ProgramFiles\Huawei\Sdk\hmscore\command-line-tools\command-config",
                "$env:ProgramFiles(x86)\Huawei\Sdk\hmscore\command-line-tools\command-config"
            )
            foreach ($p in $cmdConfigPaths) {
                if (Test-Path $p) {
                    $cmdConfig = Get-Item $p
                    break
                }
            }
        }
        if ($cmdConfig) {
            try {
                $sdkOutput = & $cmdConfig.Source sdk --noheader 2>$null
                if ($sdkOutput -match "Sdk\s*:\s*(.+)") {
                    $sdkPath = $Matches[1].Trim()
                    if (Test-Path "$sdkPath\ohos-sdk\native") {
                        $candidates += "$sdkPath\ohos-sdk\native"
                    } elseif (Test-Path "$sdkPath\native") {
                        $candidates += "$sdkPath\native"
                    }
                }
            } catch { }
        }
    }

    # --- Linux / macOS ---
    else {
        $linuxPaths = @(
            "$env:HOME/Huawei/Sdk/ohos-sdk",
            "$env:HOME/harmony-sdk/ohos-sdk",
            "/usr/local/Huawei/Sdk/ohos-sdk",
            "/opt/Huawei/Sdk/ohos-sdk"
        )
        foreach ($p in $linuxPaths) {
            if (Test-Path $p) { $candidates += $p }
        }

        $hdcCmd = Get-Command "hdc" -ErrorAction SilentlyContinue
        if ($hdcCmd) {
            $hdcDir = Split-Path (Resolve-Path $hdcCmd.Source -ErrorAction SilentlyContinue) -ErrorAction SilentlyContinue
            $sdkFromHdc = (Get-Item $hdcDir).Parent.Parent.FullName
            if (Test-Path "$sdkFromHdc/native/build/cmake/ohos.toolchain.cmake") {
                $candidates += "$sdkFromHdc/native"
            }
        }
    }

    # --- Validate candidates: must contain ohos.toolchain.cmake ---
    foreach ($dir in $candidates) {
        $toolchain = Join-Path $dir "native\build\cmake\ohos.toolchain.cmake"
        if (-not (Test-Path $toolchain)) {
            $toolchain = Join-Path $dir "build\cmake\ohos.toolchain.cmake"
        }
        if (Test-Path $toolchain) {
            return $dir
        }
    }

    return ""
}

# ============================================================
# Prerequisites
# ============================================================

$conanCmd = Get-Command "conan" -ErrorAction SilentlyContinue
if (-not $conanCmd) {
    Write-Err "Conan not found. Run: pip install conan"
    exit 1
}

$cmakeCmd = Get-Command "cmake" -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    Write-Err "CMake not found. Download from https://cmake.org"
    exit 1
}

# ============================================================
# Clean
# ============================================================

if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Info "Cleaning build directory: $BuildDir"
        Remove-Item $BuildDir -Recurse -Force
    }
}

# ============================================================
# Step 1: Install dependencies
# ============================================================

if ($OHOS) {
    Write-Info "HarmonyOS build mode"

    # Priority: -OHOSNdk param > OHOS_NDK_HOME env > auto-detect
    if (-not $OHOSNdk -and -not $env:OHOS_NDK_HOME) {
        $env:OHOS_NDK_HOME = Find-OHOS-Ndk
    }
    if ($OHOSNdk) {
        $env:OHOS_NDK_HOME = $OHOSNdk
    }
    if (-not $env:OHOS_NDK_HOME) {
        Write-Err "OHOS NDK not found."
        Write-Host ""
        Write-Host "To build for HarmonyOS, install DevEco Studio:" -ForegroundColor Yellow
        Write-Host "  1. Download: https://developer.harmonyos.com/cn/develop/deveco-studio" -ForegroundColor Cyan
        Write-Host "  2. Install and launch DevEco Studio" -ForegroundColor Cyan
        Write-Host "  3. Open SDK Manager and install 'HarmonyOS SDK' (Native development)" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Or set OHOS_NDK_HOME manually:" -ForegroundColor Yellow
        Write-Host "  $env:OHOS_NDK_HOME = 'C:\Program Files\Huawei\Sdk\ohos-sdk\native'" -ForegroundColor Cyan
        exit 1
    }
    Write-Ok "OHOS NDK: $env:OHOS_NDK_HOME"

    # Use Conan with OHOS profile to install dependencies
    $ohosProfile = Join-Path $ScriptDir "profiles\ohos-arm64"
    if (Test-Path $ohosProfile) {
        Write-Info "Installing dependencies via Conan (OHOS profile)..."
        $conanArgs = @(
            "install",
            (Join-Path $ScriptDir "conanfile.py"),
            "--output-folder=$BuildDir",
            "--profile", $ohosProfile,
            "--build=missing",
            "-c", "user.ohos:ndk_path=$env:OHOS_NDK_HOME"
        )
        if ($Verbose) {
            $conanArgs += "--verbose"
        }
        & conan @conanArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Conan install (OHOS) failed"
            exit 1
        }
        Write-Ok "Dependencies installed (OHOS)"
    } else {
        Write-Warn "OHOS profile not found at $ohosProfile, skipping Conan"
        Write-Warn "Install dependencies manually or use vcpkg"
    }
} else {
    Write-Info "Installing dependencies via Conan..."

    $conanArgs = @(
        "install",
        (Join-Path $ScriptDir "conanfile.txt"),
        "--output-folder=$BuildDir",
        "--build=missing",
        "-s", "compiler.cppstd=17"
    )

    if ($Verbose) {
        $conanArgs += "--verbose"
    }

    & conan @conanArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Conan install failed"
        exit 1
    }
    Write-Ok "Dependencies installed"
}

# ============================================================
# Step 2: CMake configure
# ============================================================

Write-Info "Configuring (Config=$Config, Arch=$Arch)..."

# OHOS mode: use Conan-generated toolchain (includes OHOS NDK settings)
if ($OHOS) {
    $conanToolchain = Join-Path $BuildDir "conan_toolchain.cmake"
    if (Test-Path $conanToolchain) {
        $cmakeArgs = @(
            "-S", $ScriptDir,
            "-B", $BuildDir,
            "-DCMAKE_TOOLCHAIN_FILE=$conanToolchain",
            "-DCMAKE_BUILD_TYPE=$Config",
            "-DCMAKE_CXX_STANDARD=17",
            "-DUSE_WINDOWS_API=OFF",
            "-DBUILD_TESTS=OFF",
            "-DBUILD_BENCHMARKS=OFF",
            "-DBUILD_EXAMPLES=OFF"
        )
    } else {
        # Fallback: use OHOS NDK toolchain directly
        $ohosToolchain = Join-Path $env:OHOS_NDK_HOME "build/cmake/ohos.toolchain.cmake"
        if (-not (Test-Path $ohosToolchain)) {
            Write-Err "OHOS toolchain not found: $ohosToolchain"
            exit 1
        }
        $cmakeArgs = @(
            "-S", $ScriptDir,
            "-B", $BuildDir,
            "-DCMAKE_TOOLCHAIN_FILE=$ohosToolchain",
            "-DCMAKE_BUILD_TYPE=$Config",
            "-DCMAKE_CXX_STANDARD=17",
            "-DUSE_WINDOWS_API=OFF",
            "-DBUILD_TESTS=OFF",
            "-DBUILD_BENCHMARKS=OFF",
            "-DBUILD_EXAMPLES=OFF"
        )
    }

    if ($Shared)     { $cmakeArgs += "-DBUILD_SHARED_LIBS=ON" }
    if ($Lua)        { $cmakeArgs += "-DBUILD_LUA_BINDINGS=ON" }

    Push-Location $ScriptDir
    & cmake @cmakeArgs
    Pop-Location
    if ($LASTEXITCODE -ne 0) {
        Write-Err "CMake configure failed"
        exit 1
    }
    Write-Ok "Configure complete (OHOS)"
    goto :build_step
}

# Determine preset name based on platform and options
$presetName = $null
$usePreset = $false

if (-not $Generator) {
    if ($IsWindows -or ($env:OS -eq "Windows_NT")) {
        if ($CrossPlatform) {
            $presetName = "windows-msvc-cross"
        } elseif ($Shared) {
            $presetName = "windows-msvc-shared"
        } else {
            $presetName = "windows-msvc"
        }
    } elseif ($IsLinux) {
        if ($Shared) {
            $presetName = "linux-gcc-shared"
        } else {
            $presetName = "linux-gcc"
        }
    } elseif ($IsMacOS) {
        if ($Shared) {
            $presetName = "macos-clang-shared"
        } else {
            $presetName = "macos-clang"
        }
    }

    # Check if project-level preset exists
    $projectPresetPath = Join-Path $ScriptDir "CMakePresets.json"
    if ($presetName -and (Test-Path $projectPresetPath)) {
        $usePreset = $true
    } else {
        $presetName = $null
    }
}

if ($usePreset) {
    $cmakeArgs = @("--preset", $presetName)
    # Preset inherits toolchain and build type; only add overrides
    if ($NoTest)     { $cmakeArgs += "-DBUILD_TESTS=OFF" }
    if ($NoExample)  { $cmakeArgs += "-DBUILD_EXAMPLES=OFF" }
    if ($NoBenchmark){ $cmakeArgs += "-DBUILD_BENCHMARKS=OFF" }
    if ($Lua)        { $cmakeArgs += "-DBUILD_LUA_BINDINGS=ON" }
    if ($Toolset)    { $cmakeArgs += "-T", $Toolset }
} else {
    $cmakeArgs = @(
        "-S", $ScriptDir,
        "-B", $BuildDir,
        "-DCMAKE_TOOLCHAIN_FILE=$BuildDir/conan_toolchain.cmake",
        "-DCMAKE_BUILD_TYPE=$Config",
        "-DCMAKE_CXX_STANDARD=17"
    )

    if ($Generator) {
        $cmakeArgs += "-G", $Generator
    } else {
        $hasVS = $false
        if ($env:VisualStudioVersion) { $hasVS = $true }
        $clCmd = Get-Command "cl.exe" -ErrorAction SilentlyContinue
        if ($clCmd) { $hasVS = $true }
        if ($hasVS) {
            $cmakeArgs += "-G", "Visual Studio 17 2022"
            $cmakeArgs += "-A", $Arch
        }
    }

    if ($Toolset)    { $cmakeArgs += "-T", $Toolset }
    if ($Shared)     { $cmakeArgs += "-DBUILD_SHARED_LIBS=ON" }
    if ($NoTest)     { $cmakeArgs += "-DBUILD_TESTS=OFF" }
    if ($NoExample)  { $cmakeArgs += "-DBUILD_EXAMPLES=OFF" }
    if ($NoBenchmark){ $cmakeArgs += "-DBUILD_BENCHMARKS=OFF" }
    if ($Lua)        { $cmakeArgs += "-DBUILD_LUA_BINDINGS=ON" }
    if ($CrossPlatform) { $cmakeArgs += "-DUSE_WINDOWS_API=OFF" }
}

# Conan 2.x generates CMakeUserPresets.json that includes build/CMakePresets.json,
# which can conflict with our project-level presets. Remove it.
$conanUserPreset = Join-Path $ScriptDir "CMakeUserPresets.json"
if (Test-Path $conanUserPreset) {
    Remove-Item $conanUserPreset -Force
}

Push-Location $ScriptDir
& cmake @cmakeArgs
Pop-Location
if ($LASTEXITCODE -ne 0) {
    Write-Err "CMake configure failed"
    exit 1
}
Write-Ok "Configure complete"

# ============================================================
# Step 3: Build
# ============================================================
:build_step

Write-Info "Building ($Config)..."

$buildArgs = @(
    "--build", $BuildDir,
    "--config", $Config,
    "--", "/m"
)

if ($Verbose) {
    $buildArgs = @(
        "--build", $BuildDir,
        "--config", $Config
    )
}

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Err "Build failed"
    exit 1
}
Write-Ok "Build complete"

# ============================================================
# Step 4: Run tests
# ============================================================

if ($Test) {
    Write-Info "Running tests..."

    $testDir = Join-Path $BuildDir "tests\$Config"
    if (-not (Test-Path $testDir)) {
        $testDir = Join-Path $BuildDir "tests"
    }

    $passed = 0
    $failed = 0

    Get-ChildItem -Path $testDir -Filter "test_*.exe" | ForEach-Object {
        $name = $_.BaseName
        Write-Host "  Run: $name" -NoNewline -ForegroundColor Gray
        $output = & $_.FullName 2>&1
        $exitCode = $LASTEXITCODE

        if ($exitCode -eq 0) {
            Write-Host " PASS" -ForegroundColor Green
            $passed++
        } else {
            Write-Host " FAIL" -ForegroundColor Red
            $failed++
            $output | Select-Object -Last 3 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        }
    }

    Write-Host ""
    Write-Info "Test results: $passed passed, $failed failed"

    if ($failed -gt 0) {
        Write-Err "Some tests failed"
        exit 1
    }
}

# ============================================================
# Step 5: Install
# ============================================================

if ($Install) {
    if ($Prefix) {
        $installPrefix = $Prefix
    } else {
        $installPrefix = Join-Path $BuildDir "install"
    }

    Write-Info "Installing to: $installPrefix"

    $installArgs = @(
        "--install", $BuildDir,
        "--config", $Config,
        "--prefix", $installPrefix
    )

    & cmake @installArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Install failed"
        exit 1
    }
    Write-Ok "Installed to: $installPrefix"
}

# ============================================================
# Summary
# ============================================================

$libTypeStr = "Static (.lib)"
if ($Shared) {
    $libTypeStr = "Dynamic (DLL)"
}

$testStr = "ON"
if ($NoTest) {
    $testStr = "OFF"
}

$exampleStr = "ON"
if ($NoExample) {
    $exampleStr = "OFF"
}

$luaStr = "OFF"
if ($Lua) {
    $luaStr = "ON"
}

$winStr = "ON"
if ($OHOS) {
    $winStr = "OFF (HarmonyOS)"
} elseif ($CrossPlatform) {
    $winStr = "OFF (Cross-platform)"
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Build Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Config:    $Config"
Write-Host "  Library:   $libTypeStr"
Write-Host "  Tests:     $testStr"
Write-Host "  Examples:  $exampleStr"
Write-Host "  Lua:       $luaStr"
Write-Host "  Windows:   $winStr"
Write-Host "  Output:    $BuildDir"
Write-Host "========================================" -ForegroundColor Cyan
