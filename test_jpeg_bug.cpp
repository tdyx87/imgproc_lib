// 测试文字渲染保存为 JPEG 是否变黑
#include <iostream>
#include <fstream>
#include "imgproc/text_renderer.hpp"
#include "imgproc/image_codec.hpp"

int main() {
    std::cout << "Testing text render to JPEG..." << std::endl;

    // 渲染文字
    imgproc::TextRenderOptions opts;
    opts.text = "Hello World";
    opts.fontSize = 48;
    opts.fgColor = 0x000000;  // 黑色文字
    opts.bgColor = 0xFFFFFF;  // 白色背景
    opts.width = 400;
    opts.height = 100;

    imgproc::ImageBuffer img;
    bool ok = imgproc::renderTextToMemory(opts, img, true);  // 使用 Windows API
    if (!ok) {
        std::cerr << "Text render failed" << std::endl;
        return 1;
    }

    std::cout << "Rendered: " << img.width << "x" << img.height 
              << " format=" << (int)img.format << " stride=" << img.stride << std::endl;

    // 检查前几个像素
    std::cout << "First 4 pixels (BGRA): ";
    for (int i = 0; i < 16 && i < img.data.size(); ++i) {
        std::cout << (int)img.data[i] << " ";
    }
    std::cout << std::endl;

    // 保存为 BMP (应该正常)
    auto codec = imgproc::createWinCodec();
    ok = codec->saveToBmpFile(img, "test_text.bmp");
    std::cout << "Save BMP: " << (ok ? "OK" : "FAIL") << std::endl;

    // 保存为 PNG (应该正常)
    ok = codec->saveToPngFile(img, "test_text.png");
    std::cout << "Save PNG: " << (ok ? "OK" : "FAIL") << std::endl;

    // 保存为 JPEG (可能变黑)
    ok = codec->saveToJpegFile(img, "test_text.jpg", 90);
    std::cout << "Save JPEG: " << (ok ? "OK" : "FAIL") << std::endl;

    // 读取 JPEG 检查
    imgproc::ImageBuffer jpegImg;
    ok = codec->loadFromFile("test_text.jpg", jpegImg);
    if (ok) {
        std::cout << "Loaded JPEG: " << jpegImg.width << "x" << jpegImg.height 
                  << " format=" << (int)jpegImg.format << std::endl;
        
        // 检查是否全黑
        int blackPixels = 0;
        int totalPixels = jpegImg.width * jpegImg.height;
        for (int y = 0; y < jpegImg.height; ++y) {
            for (int x = 0; x < jpegImg.width; ++x) {
                int idx = y * jpegImg.stride + x * 4;
                int r = jpegImg.data[idx + 2];
                int g = jpegImg.data[idx + 1];
                int b = jpegImg.data[idx];
                if (r < 10 && g < 10 && b < 10) {
                    blackPixels++;
                }
            }
        }
        std::cout << "Black pixels: " << blackPixels << "/" << totalPixels 
                  << " (" << (100 * blackPixels / totalPixels) << "%)" << std::endl;
        
        if (blackPixels > totalPixels * 0.9) {
            std::cerr << "BUG CONFIRMED: JPEG is mostly black!" << std::endl;
            return 1;
        }
    }

    std::cout << "Test passed!" << std::endl;
    return 0;
}
