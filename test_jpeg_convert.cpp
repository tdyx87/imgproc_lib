// 测试 JPEG 转换
#include <iostream>
#include "imgproc/image_codec.hpp"

int main() {
    std::cout << "Testing JPEG conversion..." << std::endl;

    // 使用 Windows codec
    auto codec = imgproc::createWinCodec();
    if (!codec) {
        std::cerr << "Failed to create codec" << std::endl;
        return 1;
    }

    // 加载 PNG
    imgproc::ImageBuffer img;
    bool ok = codec->loadFromFile("test_render.png", img);
    if (!ok) {
        std::cerr << "Failed to load PNG" << std::endl;
        return 1;
    }

    std::cout << "Loaded: " << img.width << "x" << img.height 
              << " format=" << (int)img.format << std::endl;

    // 保存为 JPEG
    ok = codec->saveToJpegFile(img, "test_render_win.jpg", 90);
    std::cout << "Save JPEG: " << (ok ? "OK" : "FAIL") << std::endl;

    // 验证
    imgproc::ImageBuffer jpegImg;
    ok = codec->loadFromFile("test_render_win.jpg", jpegImg);
    if (ok) {
        std::cout << "Loaded JPEG: " << jpegImg.width << "x" << jpegImg.height << std::endl;
        
        // 统计黑色像素
        int black = 0, white = 0, other = 0;
        for (int y = 0; y < jpegImg.height; ++y) {
            for (int x = 0; x < jpegImg.width; ++x) {
                int idx = y * jpegImg.stride + x * 4;
                int b = jpegImg.data[idx];
                int g = jpegImg.data[idx + 1];
                int r = jpegImg.data[idx + 2];
                if (r < 10 && g < 10 && b < 10) black++;
                else if (r > 245 && g > 245 && b > 245) white++;
                else other++;
            }
        }
        int total = jpegImg.width * jpegImg.height;
        std::cout << "Pixels: black=" << black << " white=" << white 
                  << " other=" << other << " total=" << total << std::endl;
    }

    return 0;
}
