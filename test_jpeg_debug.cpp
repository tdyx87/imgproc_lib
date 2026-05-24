// 调试 JPEG 保存
#include <iostream>
#include <fstream>
#include "imgproc/image_codec.hpp"

int main() {
    std::cout << "Testing JPEG save..." << std::endl;

    // 创建一个简单的 BGRA32 图像
    imgproc::ImageBuffer img;
    img.width = 100;
    img.height = 50;
    img.format = imgproc::PixelFormat::BGRA32;
    img.stride = img.width * 4;
    img.data.resize(img.stride * img.height);

    // 填充白色背景
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            int idx = y * img.stride + x * 4;
            img.data[idx] = 255;     // B
            img.data[idx + 1] = 255; // G
            img.data[idx + 2] = 255; // R
            img.data[idx + 3] = 255; // A
        }
    }

    // 在中心画一个黑色矩形
    for (int y = 20; y < 30; ++y) {
        for (int x = 30; x < 70; ++x) {
            int idx = y * img.stride + x * 4;
            img.data[idx] = 0;     // B
            img.data[idx + 1] = 0; // G
            img.data[idx + 2] = 0; // R
            img.data[idx + 3] = 255; // A
        }
    }

    std::cout << "Created image: " << img.width << "x" << img.height 
              << " format=" << (int)img.format << std::endl;

    // 使用 CrossCodec 保存为 JPEG
    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        std::cerr << "Failed to create codec" << std::endl;
        return 1;
    }

    // 先保存为 BMP 验证
    bool ok = codec->saveToBmpFile(img, "test_debug.bmp");
    std::cout << "Save BMP: " << (ok ? "OK" : "FAIL") << std::endl;

    // 保存为 JPEG
    ok = codec->saveToJpegFile(img, "test_debug.jpg", 90);
    std::cout << "Save JPEG: " << (ok ? "OK" : "FAIL") << std::endl;

    // 检查 JPEG 文件
    std::ifstream file("test_debug.jpg", std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        auto size = file.tellg();
        std::cout << "JPEG file size: " << size << " bytes" << std::endl;
        file.close();
    } else {
        std::cerr << "JPEG file not created!" << std::endl;
    }

    return 0;
}
