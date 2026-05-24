@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

:: ============================================================
:: imgproc_lib 统一编译脚本 (Batch 版本)
:: ============================================================

:: 默认配置
set "CONFIG=Release"
set "ARCH=x64"
set "GENERATOR="
set "TOOLSET="
set "PREFIX="

:: 开关选项 (0=关闭, 1=开启)
set "SHARED=0"
set "NOTEST=0"
set "NOEXAMPLE=0"
set "NOBENCH=0"
set "LUA=0"
set "CROSS=0"
set "CLEAN=0"
set "TEST=0"
set "INSTALL=0"
set "VERBOSE=0"

:: 解析参数
:parse_args
if "%~1"=="" goto :main

if /I "%~1"=="-Config" (
    set "CONFIG=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-Arch" (
    set "ARCH=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-Generator" (
    set "GENERATOR=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-Toolset" (
    set "TOOLSET=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-Prefix" (
    set "PREFIX=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-Shared" (
    set "SHARED=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-NoTest" (
    set "NOTEST=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-NoExample" (
    set "NOEXAMPLE=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-NoBenchmark" (
    set "NOBENCH=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Lua" (
    set "LUA=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-CrossPlatform" (
    set "CROSS=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Clean" (
    set "CLEAN=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Test" (
    set "TEST=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Install" (
    set "INSTALL=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Verbose" (
    set "VERBOSE=1"
    shift
    goto :parse_args
)
if /I "%~1"=="-Help" goto :help
if /I "%~1"=="-h" goto :help
if /I "%~1"=="/?" goto :help

echo [ERR] Unknown option: %~1
goto :help

:: ============================================================
:: 主流程
:: ============================================================
:main
set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"

echo.
echo ========================================
echo   imgproc_lib Build Script
echo ========================================
echo.

:: 检查工具
where conan >nul 2>nul
if errorlevel 1 (
    echo [ERR] Conan not found. Run: pip install conan
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERR] CMake not found. Download from https://cmake.org
    exit /b 1
)

:: 清理
if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo [INFO] Cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

:: ============================================================
:: Step 1: Conan 安装依赖
:: ============================================================
echo [INFO] Installing dependencies via Conan...

set "CONAN_ARGS=install "%SCRIPT_DIR%conanfile.txt" --output-folder="%BUILD_DIR%" --build=missing -s compiler.cppstd=17"

if "%VERBOSE%"=="1" (
    set "CONAN_ARGS=%CONAN_ARGS% --verbose"
)

conan %CONAN_ARGS%
if errorlevel 1 (
    echo [ERR] Conan install failed
    exit /b 1
)
echo [OK] Dependencies installed

:: ============================================================
:: Step 2: CMake 配置
:: ============================================================
echo [INFO] Configuring (Config=%CONFIG%, Arch=%ARCH%)...

set "CMAKE_ARGS="
set "USE_PRESET=0"

:: 优先使用项目级 preset
if not "%GENERATOR%"=="" (
    goto :manual_config
)

if exist "%SCRIPT_DIR%CMakePresets.json" (
    set "USE_PRESET=1"
    if "%CROSS%"=="1" (
        set "PRESET_NAME=windows-msvc-cross"
    ) else if "%SHARED%"=="1" (
        set "PRESET_NAME=windows-msvc-shared"
    ) else (
        set "PRESET_NAME=windows-msvc"
    )
    set "CMAKE_ARGS=--preset %PRESET_NAME%"
    if "%NOTEST%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_TESTS=OFF"
    if "%NOEXAMPLE%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_EXAMPLES=OFF"
    if "%NOBENCH%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_BENCHMARKS=OFF"
    if "%LUA%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_LUA_BINDINGS=ON"
    if not "%TOOLSET%"=="" set "CMAKE_ARGS=%CMAKE_ARGS% -T %TOOLSET%"
    goto :do_configure
)

:manual_config
if exist "%BUILD_DIR%\CMakePresets.json" (
    set "CMAKE_ARGS=--preset conan-default -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_CXX_STANDARD=17"
    
    if "%SHARED%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=ON"
    if "%NOTEST%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_TESTS=OFF"
    if "%NOEXAMPLE%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_EXAMPLES=OFF"
    if "%NOBENCH%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_BENCHMARKS=OFF"
    if "%LUA%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_LUA_BINDINGS=ON"
    if "%CROSS%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DUSE_WINDOWS_API=OFF"
    if not "%TOOLSET%"=="" set "CMAKE_ARGS=%CMAKE_ARGS% -T %TOOLSET%"
) else (
    if not "%GENERATOR%"=="" (
        set "CMAKE_ARGS=-G "%GENERATOR%" -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%BUILD_DIR%\conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_CXX_STANDARD=17"
    ) else (
        set "CMAKE_ARGS=-G "Visual Studio 17 2022" -A %ARCH% -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%BUILD_DIR%\conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_CXX_STANDARD=17"
    )
    
    if "%SHARED%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=ON"
    if "%NOTEST%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_TESTS=OFF"
    if "%NOEXAMPLE%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_EXAMPLES=OFF"
    if "%NOBENCH%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_BENCHMARKS=OFF"
    if "%LUA%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_LUA_BINDINGS=ON"
    if "%CROSS%"=="1" set "CMAKE_ARGS=%CMAKE_ARGS% -DUSE_WINDOWS_API=OFF"
    if not "%TOOLSET%"=="" set "CMAKE_ARGS=%CMAKE_ARGS% -T %TOOLSET%"
)

:do_configure
cmake %CMAKE_ARGS%
if errorlevel 1 (
    echo [ERR] CMake configure failed
    exit /b 1
)
echo [OK] Configure complete

:: ============================================================
:: Step 3: 编译
:: ============================================================
echo [INFO] Building (%CONFIG%)...

if "%VERBOSE%"=="1" (
    cmake --build "%BUILD_DIR%" --config %CONFIG%
) else (
    cmake --build "%BUILD_DIR%" --config %CONFIG% -- /m
)

if errorlevel 1 (
    echo [ERR] Build failed
    exit /b 1
)
echo [OK] Build complete

:: ============================================================
:: Step 4: 运行测试
:: ============================================================
if "%TEST%"=="1" (
    echo [INFO] Running tests...
    
    set "TEST_DIR=%BUILD_DIR%\tests\%CONFIG%"
    if not exist "!TEST_DIR!" set "TEST_DIR=%BUILD_DIR%\tests"
    
    set "PASSED=0"
    set "FAILED=0"
    
    for %%f in ("!TEST_DIR!\test_*.exe") do (
        set "TEST_NAME=%%~nf"
        <nul set /p ="  Run: !TEST_NAME! "
        "%%f" >nul 2>&1
        if errorlevel 1 (
            echo FAIL
            set /a FAILED+=1
        ) else (
            echo PASS
            set /a PASSED+=1
        )
    )
    
    echo.
    echo [INFO] Test results: %PASSED% passed, %FAILED% failed
    
    if %FAILED% gtr 0 (
        echo [ERR] Some tests failed
        exit /b 1
    )
)

:: ============================================================
:: Step 5: 安装
:: ============================================================
if "%INSTALL%"=="1" (
    if "%PREFIX%"=="" (
        set "INSTALL_PREFIX=%BUILD_DIR%\install"
    ) else (
        set "INSTALL_PREFIX=%PREFIX%"
    )
    
    echo [INFO] Installing to: !INSTALL_PREFIX!
    
    cmake --install "%BUILD_DIR%" --config %CONFIG% --prefix "!INSTALL_PREFIX!"
    if errorlevel 1 (
        echo [ERR] Install failed
        exit /b 1
    )
    echo [OK] Installed to: !INSTALL_PREFIX!
)

:: ============================================================
:: 汇总
:: ============================================================
echo.
echo ========================================
echo   Build Complete
echo ========================================

if "%SHARED%"=="1" (
    echo   Library:   Dynamic (DLL)
) else (
    echo   Library:   Static (.lib)
)

if "%NOTEST%"=="1" (
    echo   Tests:     OFF
) else (
    echo   Tests:     ON
)

if "%NOEXAMPLE%"=="1" (
    echo   Examples:  OFF
) else (
    echo   Examples:  ON
)

if "%LUA%"=="1" (
    echo   Lua:       ON
) else (
    echo   Lua:       OFF
)

if "%CROSS%"=="1" (
    echo   Windows:   OFF (Cross-platform)
) else (
    echo   Windows:   ON
)

echo   Config:    %CONFIG%
echo   Output:    %BUILD_DIR%
echo ========================================

exit /b 0

:: ============================================================
:: 帮助信息
:: ============================================================
:help
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   -Config ^<Release^|Debug^|RelWithDebInfo^|MinSizeRel^>  Build configuration (default: Release)
echo   -Arch ^<x64^|Win32^|ARM64^>                              Target architecture (default: x64)
echo   -Generator ^<name^>                                      CMake generator
echo   -Toolset ^<v141^|v141_xp^|...^>                          MSVC toolset
echo   -Prefix ^<path^>                                         Install prefix
echo   -Shared                                                 Build shared library (DLL)
echo   -NoTest                                                 Skip building tests
echo   -NoExample                                              Skip building examples
echo   -NoBenchmark                                            Skip building benchmarks
echo   -Lua                                                    Enable Lua bindings
echo   -CrossPlatform                                          Disable Windows API
echo   -Clean                                                  Clean build directory first
echo   -Test                                                   Run tests after build
echo   -Install                                                Install after build
echo   -Verbose                                                Verbose output
echo   -Help, -h, /?                                           Show this help
echo.
echo Examples:
echo   build.bat                              # Default build
echo   build.bat -Config Debug                # Debug build
echo   build.bat -Shared                      # Build DLL
echo   build.bat -NoTest -NoExample           # Library only
echo   build.bat -Clean -Test                 # Clean rebuild and test
echo   build.bat -Install -Prefix C:\install  # Install to directory
echo.
exit /b 0
