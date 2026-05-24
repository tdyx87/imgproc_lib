# imgproc_lib - C++ 图像处理库

一个功能完整的 C++ 图像处理库，支持格式转换、二维码读取、文本渲染和多种压缩算法，提供 Windows API 和跨平台两套实现方案。

## 功能特性

### 1. 图像格式转换 (PNG/BMP/JPEG → JPEG)
- 支持 PNG、BMP、JPEG 格式读取
- 输出 JPEG 格式（可调质量 1-100）
- 同时支持**文件路径**和**内存缓冲区**操作
- 自动检测图像类型（通过文件头魔数）

### 2. 二维码读取 (QR Code)
- 基于 ZXing-C++ 的二维码解码
- 输出标准 BMP 1-bit 格式的二维码位图
- 支持从文件或内存读取
- 支持多二维码识别

### 3. 文本转图片
- 支持三种输出格式：**1-bit BMP**、**RGB**、**JPEG**
- 支持自定义字体、字号、前景色、背景色
- 支持抗锯齿渲染
- 支持中文等多语言文本

### 4. 压缩算法
| 算法 | 类型 | 适用场景 |
|------|------|----------|
| **RLE** | 无损 | 大面积相同色块、简单图形、1-bit 图像 |
| **DeltaRow** | 无损 | 相邻行相似的图像（渐变、扫描文档、截图） |
| **JPEG** | 有损 | 照片、复杂图像，支持 q=1~100 质量调节 |

### 5. 双实现方案

| 特性 | Windows API (GDI+) | 跨平台 (libjpeg-turbo/libpng/FreeType) |
|------|-------------------|----------------------------------------|
| 平台支持 | 仅 Windows | 全平台 |
| XP 兼容 | 是 (GDI+ 1.0) | 是 |
| JPEG 编解码速度 | 中等 | 快 (libjpeg-turbo SIMD 优化) |
| PNG 支持 | 有限 | 完整 |
| BMP 1-bit | 支持 | 完整 |
| 文本渲染 | 好 (系统字体) | 好 (需字体文件) |
| CJK 支持 | 原生 | 需提供字体文件 |
| 二进制体积 | 小 (系统 DLL) | 较大 (~2-5MB 静态链接) |
| 依赖 | Windows SDK | libpng, libjpeg-turbo, freetype |

## 项目结构

```
imgproc_lib/
├── CMakeLists.txt              # 主构建文件 (VS2017 + XP 支持)
├── conanfile.txt               # Conan 依赖管理
├── cmake/
│   └── imgproc_config.cmake.in # CMake 包配置模板
├── include/imgproc/
│   ├── imgproc.hpp             # 总头文件
│   ├── types.hpp               # 核心类型定义
│   ├── image_codec.hpp         # 格式转换接口
│   ├── qrcode_reader.hpp       # 二维码读取接口
│   ├── text_renderer.hpp       # 文本渲染接口
│   ├── compression.hpp         # 压缩接口
│   └── platform/
│       ├── win_codec.hpp       # Windows API 实现
│       └── cross_codec.hpp     # 跨平台实现
├── src/
│   ├── image_codec.cpp         # 格式转换实现
│   ├── qrcode_reader.cpp       # 二维码读取实现
│   ├── text_renderer.cpp       # 文本渲染实现
│   ├── compression.cpp         # 压缩算法实现
│   └── platform/
│       ├── win_codec.cpp       # GDI+ 实现
│       └── cross_codec.cpp     # 跨平台实现
├── bench/
│   ├── CMakeLists.txt
│   └── benchmark.cpp           # 综合基准测试
├── tests/
│   ├── CMakeLists.txt
│   ├── test_image_codec.cpp    # 编解码测试
│   ├── test_qrcode.cpp         # 二维码测试
│   ├── test_text_renderer.cpp  # 文本渲染测试
│   └── test_compression.cpp    # 压缩算法测试
└── examples/
    ├── CMakeLists.txt
    └── demo.cpp                # 命令行示例工具
```

## 构建说明

### 前置条件

- CMake >= 3.15
- Visual Studio 2017 (v141 工具集)
- Conan 包管理器
- (可选) v141_xp 工具集用于 XP 支持

### 使用 Conan 安装依赖

```bash
cd imgproc_lib
conan install . --build=missing -s build_type=Release
```

### 构建步骤

```bash
# 配置 (VS2017, Release)
cmake -B build -G "Visual Studio 15 2017" -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake

# 构建
cmake --build build --config Release

# 运行测试
cd build && ctest -C Release --output-on-failure

# 运行基准测试
./build/bench/Release/imgproc_bench.exe
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_SHARED_LIBS` | OFF | 构建动态库 (DLL) |
| `BUILD_TESTS` | ON | 构建测试 |
| `BUILD_BENCHMARKS` | ON | 构建基准测试 |
| `BUILD_EXAMPLES` | ON | 构建示例 |
| `USE_WINDOWS_API` | ON | 启用 Windows API 实现 |

### XP 目标平台

使用 v141_xp 工具集：
```bash
cmake -B build -G "Visual Studio 15 2017 Win64" -T v141_xp -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake
```

## 使用示例

### 图像格式转换

```cpp
#include "imgproc/imgproc.hpp"

// 文件转换: PNG/BMP/JPEG → JPEG
imgproc::convertToJpeg("input.png", "output.jpg", 85, true);  // 使用 Windows API
imgproc::convertToJpeg("input.bmp", "output.jpg", 85, false); // 使用跨平台

// 内存转换
std::vector<uint8_t> fileData = /* 读取文件 */;
std::vector<uint8_t> jpegOut;
imgproc::convertToJpegFromMemory(fileData.data(), fileData.size(),
                                  imgproc::ImageType::PNG, jpegOut, 90);

// 使用接口 (更灵活)
auto codec = imgproc::createCrossCodec();
imgproc::ImageBuffer img;
codec->loadFromFile("photo.png", img);
codec->saveToJpegFile(img, "photo.jpg", 85);
codec->saveToBmpFile(img, "photo.bmp");
```

### 二维码读取

```cpp
#include "imgproc/imgproc.hpp"

// 从文件读取
auto result = imgproc::readQRCode("qrcode.png");
if (result.success) {
    printf("QR Content: %s\n", result.text.c_str());
    // result.bitmap1bit 包含 1-bit BMP 格式的二维码图像
}

// 从内存读取
auto result = imgproc::readQRCodeFromMemory(data, size);
```

### 文本渲染

```cpp
#include "imgproc/imgproc.hpp"

imgproc::TextRenderOptions opts;
opts.text = "Hello World 你好世界";
opts.fontSize = 32;
opts.fgColor = 0x000000;  // 黑色
opts.bgColor = 0xFFFFFF;  // 白色
opts.antiAlias = true;

// 渲染到文件
imgproc::renderTextToFile(opts, "output.bmp", true);  // Windows API
imgproc::renderTextToFile(opts, "output.bmp", false); // 跨平台

// 渲染到内存
imgproc::ImageBuffer textImg;
imgproc::renderTextToMemory(opts, textImg, false);
```

### 压缩

```cpp
#include "imgproc/imgproc.hpp"

// RLE 压缩
auto rleResult = imgproc::compressRLE(data, size, width, height, imgproc::PixelFormat::RGB24);

// DeltaRow 压缩
auto deltaResult = imgproc::compressDeltaRow(data, size, width, height, imgproc::PixelFormat::RGB24);

// JPEG 压缩
auto jpegResult = imgproc::compressJPEG(data, size, width, height, imgproc::PixelFormat::RGB24, 75);

// 解压
std::vector<uint8_t> decompressed;
int w, h;
imgproc::PixelFormat fmt;
imgproc::decompressRLE(rleResult.data.data(), rleResult.data.size(), decompressed, w, h, fmt);

// 使用接口
auto compressor = imgproc::createRLECompression();
auto result = compressor->compress(data, size, width, height, fmt);
```

### 命令行工具

```bash
# 图像转换
imgproc_demo.exe convert input.png output.jpg 85

# 读取二维码
imgproc_demo.exe qrcode qrcode.png

# 文本渲染
imgproc_demo.exe render "Hello World" output.bmp 24

# 压缩
imgproc_demo.exe compress input.bmp output.bin rle
imgproc_demo.exe compress input.bmp output.bin delta
imgproc_demo.exe compress input.bmp output.bin jpeg

# 查看图像信息
imgproc_demo.exe info photo.jpg
```

## 基准测试

运行综合基准测试：

```bash
./imgproc_bench.exe
```

基准测试包含以下模块：

1. **图像编解码对比** - Windows API vs 跨平台，不同分辨率 (64x64 ~ 2048x2048)
2. **压缩算法对比** - RLE / DeltaRow / JPEG (q=50/75/90)
3. **文本渲染对比** - GDI+ vs FreeType，不同文本长度
4. **JPEG 质量级别** - q=10 到 q=100 的速度/大小权衡
5. **压缩场景分析** - 纯色、渐变、随机数据等场景推荐

### 压缩算法选择指南

| 场景 | 推荐算法 | 原因 |
|------|----------|------|
| 纯色/简单图形 | RLE | 极高压缩比，编解码速度最快 |
| 渐变/扫描文档 | DeltaRow | 行间差异小，无损压缩效果好 |
| 照片/复杂图像 | JPEG q=75 | 有损但压缩比高，视觉效果好 |
| 需要无损 + 复杂图像 | DeltaRow | 无损，对连续变化数据效果好 |
| 1-bit 黑白图像 | RLE | 行程编码对二值图像效果极佳 |
| 追求最小体积 | JPEG q=50 | 有损但体积最小 |
| 追求最高质量 | JPEG q=90+ | 接近无损，体积较大 |

## 依赖

| 库 | 版本 | 用途 | 许可证 |
|----|------|------|--------|
| libpng | 1.6.40 | PNG 读写 | zlib |
| libjpeg-turbo | 2.1.5 | JPEG 编解码 | BSD |
| zxing-cpp | 2.1.0 | 二维码解码 | Apache 2.0 |
| freetype | 2.13.0 | 文本渲染 | FTL/GPL |

## 许可证

MIT License
