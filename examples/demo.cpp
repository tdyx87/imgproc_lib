#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "imgproc/imgproc.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [args...]" << std::endl;
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  convert <input> <output> [quality]  - Convert image to JPEG" << std::endl;
    std::cout << "  qrcode <image>                       - Read QR code from image" << std::endl;
    std::cout << "  qrgen <text> <output> [size]          - Generate QR code" << std::endl;
    std::cout << "  render <text> <output> [fontSize]    - Render text to image" << std::endl;
    std::cout << "  compress <input> <output> <type>     - Compress image" << std::endl;
    std::cout << "  info <image>                         - Show image information" << std::endl;
}

static void cmdConvert(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: convert <input> <output> [quality]" << std::endl;
        std::cerr << "  Output format is determined by file extension: .jpg/.jpeg, .png, .bmp" << std::endl;
        return;
    }

    std::string input = argv[2];
    std::string output = argv[3];
    int quality = (argc > 4) ? std::atoi(argv[4]) : 85;

    std::cout << "Converting " << input << " -> " << output
              << " (quality=" << quality << ")" << std::endl;

    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        std::cerr << "Failed to create codec." << std::endl;
        return;
    }

    imgproc::ImageBuffer img;
    if (!codec->loadFromFile(input, img)) {
        std::cerr << "Failed to load image: " << input << std::endl;
        return;
    }

    if (codec->saveToFile(img, output, quality)) {
        std::cout << "Conversion successful." << std::endl;
    } else {
        std::cerr << "Conversion failed." << std::endl;
    }
}

static void cmdQRCode(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: qrcode <image>" << std::endl;
        return;
    }

    std::string imagePath = argv[2];
    std::cout << "Reading QR code from " << imagePath << std::endl;

    auto result = imgproc::readQRCode(imagePath);
    if (result.success) {
        std::cout << "QR Code content: " << result.text << std::endl;
        std::cout << "QR Version: " << result.qrVersion << std::endl;
        std::cout << "Error Correction Level: " << result.errorCorrectionLevel << std::endl;
    } else {
        std::cerr << "Failed to read QR code." << std::endl;
    }
}

static void cmdQRGen(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: qrgen <text> <output> [size] [--ecc <0-3>] [--margin <n>]" << std::endl;
        std::cerr << "  ecc: 0=L(7%) 1=M(15%) 2=Q(25%) 3=H(30%)" << std::endl;
        return;
    }

    std::string text = argv[2];
    std::string output = argv[3];
    int size = 256;
    int eccLevel = 1;
    int margin = 4;

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ecc" && i + 1 < argc) {
            eccLevel = std::atoi(argv[++i]);
        } else if (arg == "--margin" && i + 1 < argc) {
            margin = std::atoi(argv[++i]);
        } else {
            size = std::atoi(argv[i]);
        }
    }

    std::cout << "Generating QR code: \"" << text << "\" -> " << output
              << " (size=" << size << ", ecc=" << eccLevel << ", margin=" << margin << ")" << std::endl;

    imgproc::QRCodeGenerateOptions opts;
    opts.text = text;
    opts.width = size;
    opts.height = size;
    opts.eccLevel = eccLevel;
    opts.margin = margin;

    if (imgproc::generateQRCodeToFile(opts, output)) {
        std::cout << "QR code generated successfully." << std::endl;
    } else {
        std::cerr << "Failed to generate QR code." << std::endl;
    }
}

static void cmdRender(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: render <text> <output> [fontSize] [--dpi <value>] [--bits <1|4|8|24|32>]" << std::endl;
        std::cerr << "  Output format is determined by file extension: .jpg/.jpeg, .png, .bmp" << std::endl;
        std::cerr << "  --bits: output bit depth (1/4/8=indexed, 24=RGB, 32=RGBA)" << std::endl;
        return;
    }

    std::string text = argv[2];
    std::string output = argv[3];
    int fontSize = 24;
    int dpi = 96;
    int bits = 0;  // 0 = auto (default BGRA32)

    // 解析可选参数
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dpi" && i + 1 < argc) {
            dpi = std::atoi(argv[++i]);
        } else if (arg == "--bits" && i + 1 < argc) {
            bits = std::atoi(argv[++i]);
        } else {
            fontSize = std::atoi(argv[i]);
        }
    }

    std::cout << "Rendering text to " << output
              << " (fontSize=" << fontSize << ", dpi=" << dpi << ", bits=" << bits << ")" << std::endl;

    imgproc::TextRenderOptions opts;
    opts.text = text;
    opts.fontSize = fontSize;
    opts.dpi = dpi;
    opts.bitsPerPixel = bits;

    // 渲染到内存
    imgproc::ImageBuffer rendered;
    if (!imgproc::renderTextToMemory(opts, rendered)) {
        std::cerr << "Render failed." << std::endl;
        return;
    }

    // 打印调色板信息（如果是索引格式）
    if (!rendered.palette.empty()) {
        int paletteEntries = rendered.palette.size() / 4;
        std::cout << "Palette entries: " << paletteEntries << std::endl;
        std::cout << "Palette (BGRA):" << std::endl;
        for (int i = 0; i < std::min(paletteEntries, 16); ++i) {
            uint8_t r = rendered.palette[i * 4];
            uint8_t g = rendered.palette[i * 4 + 1];
            uint8_t b = rendered.palette[i * 4 + 2];
            uint8_t a = rendered.palette[i * 4 + 3];
            std::cout << "  [" << i << "] R=" << (int)r << " G=" << (int)g
                      << " B=" << (int)b << " A=" << (int)a << std::endl;
        }
        if (paletteEntries > 16) {
            std::cout << "  ... (" << (paletteEntries - 16) << " more entries)" << std::endl;
        }
    }

    // 使用智能保存，根据扩展名选择格式
    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        std::cerr << "Failed to create codec." << std::endl;
        return;
    }

    if (codec->saveToFile(rendered, output)) {
        std::cout << "Render successful." << std::endl;
    } else {
        std::cerr << "Save failed." << std::endl;
    }
}

static void cmdCompress(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: compress <input> <output> <rle|delta|jpeg>" << std::endl;
        return;
    }

    std::string input = argv[2];
    std::string output = argv[3];
    std::string type = argv[4];

    // 读取输入图像
    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        std::cerr << "Failed to create codec." << std::endl;
        return;
    }

    imgproc::ImageBuffer img;
    if (!codec->loadFromFile(input, img)) {
        std::cerr << "Failed to load image: " << input << std::endl;
        return;
    }

    imgproc::CompressionResult compResult;

    if (type == "rle") {
        compResult = imgproc::compressRLE(img.data.data(), img.data.size(),
                                           img.width, img.height, img.format);
    } else if (type == "delta") {
        compResult = imgproc::compressDeltaRow(img.data.data(), img.data.size(),
                                                img.width, img.height, img.format);
    } else if (type == "jpeg") {
        compResult = imgproc::compressJPEG(img.data.data(), img.data.size(),
                                            img.width, img.height, img.format);
    } else {
        std::cerr << "Unknown compression type: " << type << std::endl;
        return;
    }

    // 保存压缩数据
    std::ofstream outFile(output, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output: " << output << std::endl;
        return;
    }
    outFile.write(reinterpret_cast<const char*>(compResult.data.data()),
                  compResult.data.size());

    std::cout << "Compression complete:" << std::endl;
    std::cout << "  Original size: " << img.data.size() << " bytes" << std::endl;
    std::cout << "  Compressed size: " << compResult.data.size() << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << compResult.compressionRatio << std::endl;
    std::cout << "  Elapsed: " << compResult.elapsedMs << " ms" << std::endl;
}

static void cmdInfo(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: info <image>" << std::endl;
        return;
    }

    std::string input = argv[2];
    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        std::cerr << "Failed to create codec." << std::endl;
        return;
    }

    imgproc::ImageBuffer img;
    if (!codec->loadFromFile(input, img)) {
        std::cerr << "Failed to load image: " << input << std::endl;
        return;
    }

    std::cout << "Image Information:" << std::endl;
    std::cout << "  Width: " << img.width << std::endl;
    std::cout << "  Height: " << img.height << std::endl;
    std::cout << "  Stride: " << img.stride << std::endl;
    std::cout << "  Data size: " << img.dataSize() << " bytes" << std::endl;

    const char* formatName = "Unknown";
    switch (img.format) {
        case imgproc::PixelFormat::Grayscale8: formatName = "Grayscale8"; break;
        case imgproc::PixelFormat::RGB24:      formatName = "RGB24"; break;
        case imgproc::PixelFormat::RGBA32:     formatName = "RGBA32"; break;
        case imgproc::PixelFormat::BGR24:      formatName = "BGR24"; break;
        case imgproc::PixelFormat::BGRA32:     formatName = "BGRA32"; break;
        case imgproc::PixelFormat::Indexed1:   formatName = "Indexed1"; break;
        case imgproc::PixelFormat::Indexed4:   formatName = "Indexed4"; break;
        case imgproc::PixelFormat::Indexed8:   formatName = "Indexed8"; break;
    }
    std::cout << "  Format: " << formatName << std::endl;

    if (!img.palette.empty()) {
        std::cout << "  Palette size: " << img.palette.size() << " bytes" << std::endl;
    }
}

// 将宽字符参数转换为 UTF-8
static std::vector<std::string> wargsToUtf8(int argc, wchar_t* wargv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) {
            args.push_back("");
            continue;
        }
        std::string ustr(ulen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &ustr[0], ulen, nullptr, nullptr);
        args.push_back(ustr);
    }
    return args;
}

int wmain(int argc, wchar_t* argv[]) {
    // 将宽字符参数转换为 UTF-8
    auto utf8Args = wargsToUtf8(argc, argv);
    std::vector<char*> utf8Argv;
    for (auto& a : utf8Args) {
        utf8Argv.push_back(const_cast<char*>(a.c_str()));
    }
    argc = static_cast<int>(utf8Argv.size());
    argv = nullptr; // 不再使用原始 wargv

    if (argc < 2) {
        printUsage(utf8Argv[0]);
        return 1;
    }

    try {
        std::string cmd = utf8Argv[1];

        if (cmd == "convert") {
            cmdConvert(argc, utf8Argv.data());
        } else if (cmd == "qrcode") {
            cmdQRCode(argc, utf8Argv.data());
        } else if (cmd == "qrgen") {
            cmdQRGen(argc, utf8Argv.data());
        } else if (cmd == "render") {
            cmdRender(argc, utf8Argv.data());
        } else if (cmd == "compress") {
            cmdCompress(argc, utf8Argv.data());
        } else if (cmd == "info") {
            cmdInfo(argc, utf8Argv.data());
        } else {
            std::cerr << "Unknown command: " << cmd << std::endl;
            printUsage(utf8Argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;
        return 1;
    }

    return 0;
}
