cmake_minimum_required(VERSION 3.15)
project(imgproc_lib VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 选项
# ============================================================
option(BUILD_SHARED_LIBS "Build shared library (DLL)" OFF)
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_BENCHMARKS "Build benchmark executables" ON)
option(BUILD_EXAMPLES "Build example executables" ON)
option(USE_WINDOWS_API "Enable Windows API implementation" ON)

# ============================================================
# C++ 标准
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: 使用 UTF-8 编译（源文件包含中文注释）
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================
# Windows XP 支持 (v141_xp 工具集)
# ============================================================
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601 -DWINVER=0x0601)
    add_definitions(-DNTDDI_VERSION=0x06010000)
endif()

# ============================================================
# 导出宏
# ============================================================
include(GenerateExportHeader)

# ============================================================
# 查找依赖 (Conan)
# ============================================================
find_package(ZXing REQUIRED)
find_package(PNG REQUIRED)
find_package(libjpeg-turbo REQUIRED)
find_package(Freetype REQUIRED)
find_package(Zint REQUIRED)

# ============================================================
# 库源文件
# ============================================================
set(LIB_SOURCES
    src/image_codec.cpp
    src/qrcode_reader.cpp
    src/text_renderer.cpp
    src/compression.cpp
    src/image_transform.cpp
    src/imgproc_c_api.cpp
    src/barcode_generator.cpp
    src/platform/cross_codec.cpp
    third_party/qrcodegen/qrcodegen.cpp
)

# Lua 绑定（可选）
option(BUILD_LUA_BINDINGS "Build Lua bindings" OFF)
if(BUILD_LUA_BINDINGS)
    list(APPEND LIB_SOURCES src/imgproc_lua.cpp)
    
    # 查找 Lua 库
    # 方法1: 使用 CMake 的 FindLua 模块
    find_package(Lua QUIET)
    if(LUA_FOUND)
        message(STATUS "Found Lua via FindLua: ${LUA_LIBRARIES}")
        list(APPEND LIB_PUBLIC_INCLUDES ${LUA_INCLUDE_DIR})
        list(APPEND LIB_PUBLIC_LIBS ${LUA_LIBRARIES})
    else()
        # 方法2: 手动查找（Conan 或系统安装）
        find_path(LUA_INCLUDE_DIR lua.h
            PATHS 
                ${CMAKE_PREFIX_PATH}/include
                ${CONAN_INCLUDE_DIRS}
                /usr/include/lua5.4
                /usr/include/lua5.3
                /usr/local/include
        )
        find_library(LUA_LIBRARY 
            NAMES lua lua54 lua53 lua5.4 lua5.3
            PATHS
                ${CMAKE_PREFIX_PATH}/lib
                ${CONAN_LIB_DIRS}
                /usr/lib
                /usr/local/lib
        )
        if(LUA_INCLUDE_DIR AND LUA_LIBRARY)
            message(STATUS "Found Lua manually: ${LUA_LIBRARY}")
            set(LUA_FOUND TRUE)
            list(APPEND LIB_PUBLIC_INCLUDES ${LUA_INCLUDE_DIR})
            list(APPEND LIB_PUBLIC_LIBS ${LUA_LIBRARY})
        else()
            message(FATAL_ERROR "Lua not found. Please install Lua or set LUA_INCLUDE_DIR and LUA_LIBRARY manually.")
        endif()
    endif()
    
    # 安装 Lua 脚本
    install(FILES 
        lua/test_imgproc.lua 
        lua/demo.lua 
        lua/README.md
        DESTINATION share/imgproc/lua
    )
    
    # 添加 Lua 测试
    if(BUILD_TESTING)
        find_program(LUA_EXECUTABLE lua lua5.3 lua5.4 luajit
            HINTS ${LUA_BINDIR} ENV PATH
        )
        if(LUA_EXECUTABLE)
            add_test(NAME lua_unit_test
                COMMAND ${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/lua/test_imgproc.lua ${CMAKE_CURRENT_BINARY_DIR}/lua_output
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )
            message(STATUS "Lua tests enabled: ${LUA_EXECUTABLE}")
        else()
            message(WARNING "Lua executable not found, Lua tests disabled")
        endif()
    endif()
endif()

if(USE_WINDOWS_API AND WIN32)
    list(APPEND LIB_SOURCES src/platform/win_codec.cpp)
endif()

# ============================================================
# 构建库
# ============================================================
add_library(imgproc_lib ${LIB_SOURCES})

# 动态库导出宏定义
target_compile_definitions(imgproc_lib
    PRIVATE
        $<$<BOOL:${BUILD_SHARED_LIBS}>:IMGPROC_BUILD_DLL>
    PUBLIC
        $<$<BOOL:${BUILD_SHARED_LIBS}>:IMGPROC_USE_DLL>
)

target_include_directories(imgproc_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen
)

# 链接依赖
target_link_libraries(imgproc_lib
    PUBLIC
        PNG::PNG
        libjpeg-turbo::libjpeg-turbo
        ZXing::ZXing
        Freetype::Freetype
        Zint::Zint
)

if(USE_WINDOWS_API AND WIN32)
    target_compile_definitions(imgproc_lib PUBLIC USE_WINDOWS_API=1)
    target_link_libraries(imgproc_lib
        PUBLIC
            gdiplus
            ole32
            uuid
    )
endif()

# 生成导出头文件
generate_export_header(imgproc_lib
    BASE_NAME imgproc
    EXPORT_MACRO_NAME IMGPROC_EXPORT
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/imgproc/export.hpp
)
target_include_directories(imgproc_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

# ============================================================
# 子目录
# ============================================================
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================
# 安装
# ============================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS imgproc_lib
    EXPORT imgproc_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/imgproc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/imgproc/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgproc
)

install(EXPORT imgproc_targets
    FILE imgproc_targets.cmake
    NAMESPACE imgproc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgproc
)

configure_package_config_file(
    cmake/imgproc_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/imgproc-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgproc
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/imgproc-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/imgproc-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/imgproc-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgproc
)
