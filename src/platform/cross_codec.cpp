#include "imgproc/platform/cross_codec.hpp"
#include "imgproc/image_codec.hpp"
#include <png.h>
#include <jpeglib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <algorithm>

namespace imgproc {

namespace {

int bytesPerPixel(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::Grayscale8: return 1;
        case PixelFormat::RGB24:      return 3;
        case PixelFormat::RGBA32:     return 4;
        case PixelFormat::BGR24:      return 3;
        case PixelFormat::BGRA32:     return 4;
        case PixelFormat::Indexed1:   return 0;
        case PixelFormat::Indexed4:   return 0;
        case PixelFormat::Indexed8:   return 0;
        default: return 3;
    }
}

// RGB <-> BGR 转换
void rgbToBgr(uint8_t* data, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; ++i) {
        std::swap(data[i * 3], data[i * 3 + 2]);
    }
}

void bgrToRgb(uint8_t* data, size_t pixelCount) {
    rgbToBgr(data, pixelCount); // 对称操作
}

// RGBA <-> BGRA 转换
void rgbaToBgra(uint8_t* data, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; ++i) {
        std::swap(data[i * 4], data[i * 4 + 2]);
    }
}

void bgraToRgba(uint8_t* data, size_t pixelCount) {
    rgbaToBgra(data, pixelCount); // 对称操作
}

} // anonymous namespace

// ============================================================
// CrossImageCodec
// ============================================================

CrossImageCodec::CrossImageCodec() = default;
CrossImageCodec::~CrossImageCodec() = default;

bool CrossImageCodec::loadFromFile(const std::string& path, ImageBuffer& out) {
    // 先读取文件到内存
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    if (!file) return false;

    // 检测类型
    ImageType type = detectImageType(fileData.data(), fileData.size());
    if (type == ImageType::Unknown) return false;

    return loadFromMemory(fileData.data(), fileData.size(), type, out);
}

bool CrossImageCodec::loadFromMemory(const uint8_t* data, size_t size,
                                     ImageType type, ImageBuffer& out) {
    if (type == ImageType::PNG) {
        return loadPng(data, size, out);
    } else if (type == ImageType::JPEG) {
        return loadJpeg(data, size, out);
    } else if (type == ImageType::BMP) {
        return loadBmp(data, size, out);
    }
    return false;
}

bool CrossImageCodec::loadPng(const uint8_t* data, size_t size, ImageBuffer& out) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }

    // 设置自定义读取函数
    auto readData = reinterpret_cast<const char*>(data);
    png_set_read_fn(png, const_cast<char*>(readData),
        [](png_structp pngPtr, png_bytep outBytes, png_size_t bytesToRead) {
            auto** dataPtr = reinterpret_cast<const char**>(png_get_io_ptr(pngPtr));
            std::memcpy(outBytes, *dataPtr, bytesToRead);
            *dataPtr += bytesToRead;
        });

    png_read_info(png, info);

    out.width = static_cast<int>(png_get_image_width(png, info));
    out.height = static_cast<int>(png_get_image_height(png, info));
    int colorType = png_get_color_type(png, info);
    int bitDepth = png_get_bit_depth(png, info);

    // 转换为标准格式
    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    out.format = PixelFormat::RGBA32;
    out.stride = out.width * 4;
    out.data.resize(static_cast<size_t>(out.stride) * out.height);

    std::vector<png_bytep> rowPointers(out.height);
    for (int y = 0; y < out.height; ++y) {
        rowPointers[y] = out.data.data() + y * out.stride;
    }

    png_read_image(png, rowPointers.data());
    png_destroy_read_struct(&png, &info, nullptr);

    // libpng 输出 RGBA, 转换为 BGRA
    rgbaToBgra(out.data.data(), static_cast<size_t>(out.width) * out.height);

    return true;
}

bool CrossImageCodec::loadJpeg(const uint8_t* data, size_t size, ImageBuffer& out) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<uint8_t*>(data), static_cast<unsigned long>(size));

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    out.width = static_cast<int>(cinfo.output_width);
    out.height = static_cast<int>(cinfo.output_height);
    out.format = PixelFormat::BGR24;
    out.stride = out.width * 3;
    out.data.resize(static_cast<size_t>(out.stride) * out.height);

    std::vector<JSAMPROW> rowPointers(out.height);
    for (int y = 0; y < out.height; ++y) {
        rowPointers[y] = out.data.data() + y * out.stride;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &rowPointers[cinfo.output_scanline], 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    // libjpeg 输出 RGB, 转换为 BGR
    rgbToBgr(out.data.data(), static_cast<size_t>(out.width) * out.height);

    return true;
}

bool CrossImageCodec::loadBmp(const uint8_t* data, size_t size, ImageBuffer& out) {
    if (size < 54) return false; // BMP 头最小 54 字节

    // 解析 BMP 文件头
    uint32_t pixelOffset = *reinterpret_cast<const uint32_t*>(data + 10);
    int32_t bmpWidth = *reinterpret_cast<const int32_t*>(data + 18);
    int32_t bmpHeight = *reinterpret_cast<const int32_t*>(data + 22);
    uint16_t bitsPerPixel = *reinterpret_cast<const uint16_t*>(data + 28);
    uint32_t compression = *reinterpret_cast<const uint32_t*>(data + 30);

    if (bmpHeight < 0) bmpHeight = -bmpHeight; // 自顶向下

    out.width = bmpWidth;
    out.height = bmpHeight;

    if (bitsPerPixel == 24) {
        out.format = PixelFormat::BGR24;
        out.stride = ((bmpWidth * 3 + 3) / 4) * 4;
        out.data.resize(static_cast<size_t>(out.stride) * bmpHeight);

        for (int y = 0; y < bmpHeight; ++y) {
            // BMP 从底部开始存储
            int srcRow = (bmpHeight - 1 - y);
            size_t srcOffset = pixelOffset + srcRow * out.stride;
            if (srcOffset + out.stride > size) return false;

            std::memcpy(out.data.data() + y * out.stride,
                       data + srcOffset, out.stride);
        }
    } else if (bitsPerPixel == 32) {
        out.format = PixelFormat::BGRA32;
        out.stride = bmpWidth * 4;
        out.data.resize(static_cast<size_t>(out.stride) * bmpHeight);

        for (int y = 0; y < bmpHeight; ++y) {
            int srcRow = (bmpHeight - 1 - y);
            size_t srcOffset = pixelOffset + srcRow * out.stride;
            if (srcOffset + out.stride > size) return false;

            std::memcpy(out.data.data() + y * out.stride,
                       data + srcOffset, out.stride);
        }
    } else if (bitsPerPixel == 8) {
        out.format = PixelFormat::Indexed8;
        out.stride = ((bmpWidth + 3) / 4) * 4;
        out.data.resize(static_cast<size_t>(out.stride) * bmpHeight);

        // 读取调色板 (位于文件头之后, 像素数据之前)
        uint32_t paletteOffset = 54; // BITMAPINFOHEADER 大小
        uint32_t paletteSize = pixelOffset - paletteOffset;
        if (paletteSize > 0 && paletteOffset + paletteSize <= size) {
            out.palette.resize(paletteSize);
            std::memcpy(out.palette.data(), data + paletteOffset, paletteSize);
        }

        for (int y = 0; y < bmpHeight; ++y) {
            int srcRow = (bmpHeight - 1 - y);
            size_t srcOffset = pixelOffset + srcRow * out.stride;
            if (srcOffset + out.stride > size) return false;

            std::memcpy(out.data.data() + y * out.stride,
                       data + srcOffset, out.stride);
        }
    } else if (bitsPerPixel == 1) {
        out.format = PixelFormat::Indexed1;
        out.stride = ((bmpWidth + 31) / 32) * 4;
        out.data.resize(static_cast<size_t>(out.stride) * bmpHeight);

        // 读取调色板
        uint32_t paletteOffset = 54;
        uint32_t paletteSize = pixelOffset - paletteOffset;
        if (paletteSize > 0 && paletteOffset + paletteSize <= size) {
            out.palette.resize(paletteSize);
            std::memcpy(out.palette.data(), data + paletteOffset, paletteSize);
        }

        for (int y = 0; y < bmpHeight; ++y) {
            int srcRow = (bmpHeight - 1 - y);
            size_t srcOffset = pixelOffset + srcRow * out.stride;
            if (srcOffset + out.stride > size) return false;

            std::memcpy(out.data.data() + y * out.stride,
                       data + srcOffset, out.stride);
        }
    } else {
        return false;
    }

    return true;
}

bool CrossImageCodec::saveToJpegFile(const ImageBuffer& img, const std::string& path, int quality) {
    std::vector<uint8_t> jpegData;
    if (!saveToJpegMemory(img, jpegData, quality)) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(jpegData.data()), jpegData.size());
    return file.good();
}

bool CrossImageCodec::saveToJpegMemory(const ImageBuffer& img, std::vector<uint8_t>& out, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

    cinfo.image_width = static_cast<JDIMENSION>(img.width);
    cinfo.image_height = static_cast<JDIMENSION>(img.height);
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    int bpp = bytesPerPixel(img.format);
    std::vector<uint8_t> rgbRow(static_cast<size_t>(img.width) * 3);

    while (cinfo.next_scanline < cinfo.image_height) {
        int y = static_cast<int>(cinfo.next_scanline);
        const uint8_t* srcRow = img.data.data() + y * img.stride;

        for (int x = 0; x < img.width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            switch (img.format) {
                case PixelFormat::RGB24:
                    r = srcRow[x * 3]; g = srcRow[x * 3 + 1]; b = srcRow[x * 3 + 2];
                    break;
                case PixelFormat::BGR24:
                    b = srcRow[x * 3]; g = srcRow[x * 3 + 1]; r = srcRow[x * 3 + 2];
                    break;
                case PixelFormat::RGBA32:
                    r = srcRow[x * 4]; g = srcRow[x * 4 + 1]; b = srcRow[x * 4 + 2];
                    break;
                case PixelFormat::BGRA32:
                    b = srcRow[x * 4]; g = srcRow[x * 4 + 1]; r = srcRow[x * 4 + 2];
                    break;
                case PixelFormat::Grayscale8:
                    r = g = b = srcRow[x];
                    break;
                case PixelFormat::Indexed8:
                    if (!img.palette.empty()) {
                        uint8_t idx = srcRow[x];
                        // 调色板格式: BGRA (每色4字节)
                        if (idx * 4 + 2 < img.palette.size()) {
                            b = img.palette[idx * 4];
                            g = img.palette[idx * 4 + 1];
                            r = img.palette[idx * 4 + 2];
                        }
                    }
                    break;
                case PixelFormat::Indexed1:
                    if (!img.palette.empty()) {
                        int byteIdx = x / 8;
                        int bitIdx = 7 - (x % 8);
                        uint8_t idx = (srcRow[byteIdx] >> bitIdx) & 1;
                        // 调色板格式: BGRA (每色4字节)
                        if (idx * 4 + 2 < img.palette.size()) {
                            b = img.palette[idx * 4];
                            g = img.palette[idx * 4 + 1];
                            r = img.palette[idx * 4 + 2];
                        }
                    }
                    break;
                default:
                    r = g = b = 0;
                    break;
            }
            rgbRow[x * 3] = r;
            rgbRow[x * 3 + 1] = g;
            rgbRow[x * 3 + 2] = b;
        }

        JSAMPROW rowPtr = rgbRow.data();
        jpeg_write_scanlines(&cinfo, &rowPtr, 1);
    }

    jpeg_finish_compress(&cinfo);
    
    // 必须在 jpeg_destroy_compress 之前复制数据
    out.assign(jpegBuf, jpegBuf + jpegSize);
    
    jpeg_destroy_compress(&cinfo);
    
    if (jpegBuf) free(jpegBuf);

    return true;
}

bool CrossImageCodec::saveToBmpFile(const ImageBuffer& img, const std::string& path) {
    std::vector<uint8_t> bmpData;
    if (!saveToBmpMemory(img, bmpData)) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(bmpData.data()), bmpData.size());
    return file.good();
}

bool CrossImageCodec::saveToBmpMemory(const ImageBuffer& img, std::vector<uint8_t>& out) {
    // 构造 BMP 文件
    int bpp = bytesPerPixel(img.format);

    // 对于索引格式
    if (img.format == PixelFormat::Indexed1 || img.format == PixelFormat::Indexed4 ||
        img.format == PixelFormat::Indexed8) {
        // 直接使用已有的 stride 和 data
        int pixelBits = (img.format == PixelFormat::Indexed1) ? 1 :
                        (img.format == PixelFormat::Indexed4) ? 4 : 8;

        uint32_t paletteSize = (img.palette.empty()) ?
            (static_cast<uint32_t>(1) << pixelBits) * 4 :
            static_cast<uint32_t>(img.palette.size());

        uint32_t pixelDataSize = static_cast<uint32_t>(img.stride) * img.height;
        uint32_t fileSize = 14 + 40 + paletteSize + pixelDataSize;

        out.resize(fileSize, 0);

        // BMP 文件头 (14 字节)
        out[0] = 'B'; out[1] = 'M';
        *reinterpret_cast<uint32_t*>(out.data() + 2) = fileSize;
        *reinterpret_cast<uint32_t*>(out.data() + 6) = 0;
        *reinterpret_cast<uint32_t*>(out.data() + 10) = 14 + 40 + paletteSize;

        // DIB 头 (BITMAPINFOHEADER, 40 字节)
        *reinterpret_cast<uint32_t*>(out.data() + 14) = 40;
        *reinterpret_cast<int32_t*>(out.data() + 18) = img.width;
        *reinterpret_cast<int32_t*>(out.data() + 22) = img.height;
        *reinterpret_cast<uint16_t*>(out.data() + 26) = 1;
        *reinterpret_cast<uint16_t*>(out.data() + 28) = static_cast<uint16_t>(pixelBits);
        *reinterpret_cast<uint32_t*>(out.data() + 30) = 0; // BI_RGB

        // 写入调色板
        if (!img.palette.empty()) {
            std::memcpy(out.data() + 54, img.palette.data(), img.palette.size());
        }

        // 写入像素数据 (BMP 从底部到顶部)
        for (int y = 0; y < img.height; ++y) {
            int srcRow = img.height - 1 - y;
            std::memcpy(out.data() + 14 + 40 + paletteSize + y * img.stride,
                       img.data.data() + srcRow * img.stride,
                       img.stride);
        }

        return true;
    }

    // 标准格式 (24-bit / 32-bit)
    if (bpp == 0) bpp = 3;

    int dstStride = ((img.width * bpp + 3) / 4) * 4;
    uint32_t pixelDataSize = static_cast<uint32_t>(dstStride) * img.height;
    uint32_t fileSize = 14 + 40 + pixelDataSize;

    out.resize(fileSize, 0);

    // BMP 文件头
    out[0] = 'B'; out[1] = 'M';
    *reinterpret_cast<uint32_t*>(out.data() + 2) = fileSize;
    *reinterpret_cast<uint32_t*>(out.data() + 6) = 0;
    *reinterpret_cast<uint32_t*>(out.data() + 10) = 54;

    // DIB 头
    *reinterpret_cast<uint32_t*>(out.data() + 14) = 40;
    *reinterpret_cast<int32_t*>(out.data() + 18) = img.width;
    *reinterpret_cast<int32_t*>(out.data() + 22) = img.height;
    *reinterpret_cast<uint16_t*>(out.data() + 26) = 1;
    *reinterpret_cast<uint16_t*>(out.data() + 28) = static_cast<uint16_t>(bpp * 8);
    *reinterpret_cast<uint32_t*>(out.data() + 30) = 0;

    // 写入像素数据
    for (int y = 0; y < img.height; ++y) {
        int srcRow = img.height - 1 - y;
        const uint8_t* src = img.data.data() + srcRow * img.stride;
        uint8_t* dst = out.data() + 54 + y * dstStride;

        if (bpp == 3) {
            // 转换为 BGR24
            for (int x = 0; x < img.width; ++x) {
                switch (img.format) {
                    case PixelFormat::RGB24:
                        dst[x * 3] = src[x * 3 + 2]; // B
                        dst[x * 3 + 1] = src[x * 3 + 1]; // G
                        dst[x * 3 + 2] = src[x * 3]; // R
                        break;
                    case PixelFormat::BGR24:
                        std::memcpy(dst + x * 3, src + x * 3, 3);
                        break;
                    case PixelFormat::RGBA32:
                        dst[x * 3] = src[x * 4 + 2];
                        dst[x * 3 + 1] = src[x * 4 + 1];
                        dst[x * 3 + 2] = src[x * 4];
                        break;
                    case PixelFormat::BGRA32:
                        std::memcpy(dst + x * 3, src + x * 4, 3);
                        break;
                    case PixelFormat::Grayscale8:
                        dst[x * 3] = src[x];
                        dst[x * 3 + 1] = src[x];
                        dst[x * 3 + 2] = src[x];
                        break;
                    case PixelFormat::Indexed8:
                        if (!img.palette.empty()) {
                            uint8_t idx = src[x];
                            if (idx * 4 + 2 < img.palette.size()) {
                                dst[x * 3] = img.palette[idx * 4];     // B
                                dst[x * 3 + 1] = img.palette[idx * 4 + 1]; // G
                                dst[x * 3 + 2] = img.palette[idx * 4 + 2]; // R
                            }
                        }
                        break;
                    case PixelFormat::Indexed1:
                        if (!img.palette.empty()) {
                            int byteIdx = x / 8;
                            int bitIdx = 7 - (x % 8);
                            uint8_t idx = (src[byteIdx] >> bitIdx) & 1;
                            if (idx * 4 + 2 < img.palette.size()) {
                                dst[x * 3] = img.palette[idx * 4];     // B
                                dst[x * 3 + 1] = img.palette[idx * 4 + 1]; // G
                                dst[x * 3 + 2] = img.palette[idx * 4 + 2]; // R
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        } else if (bpp == 4) {
            for (int x = 0; x < img.width; ++x) {
                switch (img.format) {
                    case PixelFormat::RGBA32:
                        std::memcpy(dst + x * 4, src + x * 4, 4);
                        break;
                    case PixelFormat::BGRA32:
                        std::memcpy(dst + x * 4, src + x * 4, 4);
                        break;
                    case PixelFormat::Indexed8:
                        if (!img.palette.empty()) {
                            uint8_t idx = src[x];
                            if (idx * 4 + 3 < img.palette.size()) {
                                dst[x * 4] = img.palette[idx * 4];     // B
                                dst[x * 4 + 1] = img.palette[idx * 4 + 1]; // G
                                dst[x * 4 + 2] = img.palette[idx * 4 + 2]; // R
                                dst[x * 4 + 3] = img.palette[idx * 4 + 3]; // A
                            }
                        }
                        break;
                    case PixelFormat::Indexed1:
                        if (!img.palette.empty()) {
                            int byteIdx = x / 8;
                            int bitIdx = 7 - (x % 8);
                            uint8_t idx = (src[byteIdx] >> bitIdx) & 1;
                            if (idx * 4 + 3 < img.palette.size()) {
                                dst[x * 4] = img.palette[idx * 4];     // B
                                dst[x * 4 + 1] = img.palette[idx * 4 + 1]; // G
                                dst[x * 4 + 2] = img.palette[idx * 4 + 2]; // R
                                dst[x * 4 + 3] = img.palette[idx * 4 + 3]; // A
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return true;
}

bool CrossImageCodec::saveToPngFile(const ImageBuffer& img, const std::string& path) {
    std::vector<uint8_t> pngData;
    if (!saveToPngMemory(img, pngData)) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(pngData.data()), pngData.size());
    return file.good();
}

bool CrossImageCodec::saveToPngMemory(const ImageBuffer& img, std::vector<uint8_t>& out) {
    // 确定 PNG 颜色类型和位深度
    int colorType = PNG_COLOR_TYPE_RGB;
    int bitDepth = 8;
    int bpp = 3;
    switch (img.format) {
        case PixelFormat::Grayscale8:
            colorType = PNG_COLOR_TYPE_GRAY;
            bitDepth = 8;
            bpp = 1;
            break;
        case PixelFormat::Indexed1:
            colorType = PNG_COLOR_TYPE_GRAY;
            bitDepth = 1;
            bpp = 1;
            break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
            colorType = PNG_COLOR_TYPE_RGB;
            bitDepth = 8;
            bpp = 3;
            break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            colorType = PNG_COLOR_TYPE_RGBA;
            bitDepth = 8;
            bpp = 4;
            break;
        default:
            colorType = PNG_COLOR_TYPE_RGB;
            bitDepth = 8;
            bpp = 3;
            break;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        return false;
    }

    // 内存写入回调
    struct PngMemWriter {
        std::vector<uint8_t>* data;
    } writer{&out};
    out.clear();

    png_set_write_fn(png, &writer, [](png_structp png, png_bytep data, png_size_t len) {
        auto* w = static_cast<PngMemWriter*>(png_get_io_ptr(png));
        w->data->insert(w->data->end(), data, data + len);
    }, nullptr);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_set_IHDR(png, info, img.width, img.height, bitDepth, colorType,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    if (img.format == PixelFormat::Indexed1) {
        // 1-bit 灰度：直接写入位数据（每行按字节对齐）
        int rowBytes = (img.width + 7) / 8;
        for (int y = 0; y < img.height; ++y) {
            png_write_row(png, const_cast<png_bytep>(img.data.data() + y * img.stride));
        }
    } else {
        // 准备行数据
        std::vector<uint8_t> row(static_cast<size_t>(img.width) * bpp);
        for (int y = 0; y < img.height; ++y) {
            const uint8_t* srcRow = img.data.data() + y * img.stride;

            for (int x = 0; x < img.width; ++x) {
                switch (img.format) {
                    case PixelFormat::RGB24:
                        row[x * 3] = srcRow[x * 3];
                        row[x * 3 + 1] = srcRow[x * 3 + 1];
                        row[x * 3 + 2] = srcRow[x * 3 + 2];
                        break;
                    case PixelFormat::BGR24:
                        row[x * 3] = srcRow[x * 3 + 2];
                        row[x * 3 + 1] = srcRow[x * 3 + 1];
                        row[x * 3 + 2] = srcRow[x * 3];
                        break;
                    case PixelFormat::RGBA32:
                        row[x * 4] = srcRow[x * 4];
                        row[x * 4 + 1] = srcRow[x * 4 + 1];
                        row[x * 4 + 2] = srcRow[x * 4 + 2];
                        row[x * 4 + 3] = srcRow[x * 4 + 3];
                        break;
                    case PixelFormat::BGRA32:
                        row[x * 4] = srcRow[x * 4 + 2];
                        row[x * 4 + 1] = srcRow[x * 4 + 1];
                        row[x * 4 + 2] = srcRow[x * 4];
                        row[x * 4 + 3] = srcRow[x * 4 + 3];
                        break;
                    case PixelFormat::Grayscale8:
                        row[x] = srcRow[x];
                        break;
                    case PixelFormat::Indexed8:
                        if (!img.palette.empty()) {
                            uint8_t idx = srcRow[x];
                            if (idx * 4 + 3 < img.palette.size()) {
                                row[x * 4] = img.palette[idx * 4 + 2];     // R
                                row[x * 4 + 1] = img.palette[idx * 4 + 1]; // G
                                row[x * 4 + 2] = img.palette[idx * 4];     // B
                                row[x * 4 + 3] = img.palette[idx * 4 + 3]; // A
                            }
                        }
                        break;
                    case PixelFormat::Indexed1:
                        if (!img.palette.empty()) {
                            int byteIdx = x / 8;
                            int bitIdx = 7 - (x % 8);
                            uint8_t idx = (srcRow[byteIdx] >> bitIdx) & 1;
                            if (idx * 4 + 3 < img.palette.size()) {
                                row[x * 4] = img.palette[idx * 4 + 2];     // R
                                row[x * 4 + 1] = img.palette[idx * 4 + 1]; // G
                                row[x * 4 + 2] = img.palette[idx * 4];     // B
                                row[x * 4 + 3] = img.palette[idx * 4 + 3]; // A
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            png_write_row(png, row.data());
        }
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    return true;
}

bool CrossImageCodec::saveToFile(const ImageBuffer& img, const std::string& path, int jpegQuality) {
    // 根据文件扩展名选择格式
    auto dotPos = path.rfind('.');
    if (dotPos == std::string::npos) {
        // 无扩展名，默认 BMP
        return saveToBmpFile(img, path);
    }

    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "jpg" || ext == "jpeg") {
        return saveToJpegFile(img, path, jpegQuality);
    } else if (ext == "png") {
        return saveToPngFile(img, path);
    } else {
        // 默认 BMP (包括 .bmp 和其他扩展名)
        return saveToBmpFile(img, path);
    }
}

// 前向声明辅助函数
static void convertToIndexed(ImageBuffer& img, int bits, uint32_t fgColor, uint32_t bgColor);
static void convertToRGB24(ImageBuffer& img);

// UTF-8 解码: 从字符串位置解码一个 Unicode codepoint, 返回消耗的字节数
static int utf8Decode(const std::string& str, size_t pos, uint32_t& codepoint) {
    if (pos >= str.size()) { codepoint = 0; return 0; }
    uint8_t c = static_cast<uint8_t>(str[pos]);
    if (c < 0x80) {
        codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        if (pos + 1 >= str.size()) { codepoint = 0; return 1; }
        codepoint = ((c & 0x1F) << 6) | (static_cast<uint8_t>(str[pos + 1]) & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        if (pos + 2 >= str.size()) { codepoint = 0; return 1; }
        codepoint = ((c & 0x0F) << 12)
                   | ((static_cast<uint8_t>(str[pos + 1]) & 0x3F) << 6)
                   | (static_cast<uint8_t>(str[pos + 2]) & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        if (pos + 3 >= str.size()) { codepoint = 0; return 1; }
        codepoint = ((c & 0x07) << 18)
                   | ((static_cast<uint8_t>(str[pos + 1]) & 0x3F) << 12)
                   | ((static_cast<uint8_t>(str[pos + 2]) & 0x3F) << 6)
                   | (static_cast<uint8_t>(str[pos + 3]) & 0x3F);
        return 4;
    }
    codepoint = c;
    return 1;
}

// ============================================================
// CrossTextRenderer
// ============================================================

CrossTextRenderer::CrossTextRenderer() = default;
CrossTextRenderer::~CrossTextRenderer() = default;

bool CrossTextRenderer::renderToFile(const TextRenderOptions& opts, const std::string& path) {
    ImageBuffer out;
    if (!renderToMemory(opts, out)) return false;

    CrossImageCodec codec;
    return codec.saveToBmpFile(out, path);
}

bool CrossTextRenderer::renderToMemory(const TextRenderOptions& opts, ImageBuffer& out) {
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0) return false;

    FT_Face face = nullptr;

    // 尝试加载字体：先尝试用户指定的字体，再尝试系统默认字体
    std::string fontPath = opts.fontPath;
    bool fontLoaded = false;

    if (!fontPath.empty()) {
        if (FT_New_Face(library, fontPath.c_str(), 0, &face) == 0) {
            fontLoaded = true;
        }
    }

    if (!fontLoaded) {
        // 尝试使用系统字体
#ifdef _WIN32
        const char* systemFonts[] = {
            "C:\\Windows\\Fonts\\simhei.ttf",    // 黑体 (首选, 支持中文)
            "C:\\Windows\\Fonts\\msyh.ttc",      // 微软雅黑
            "C:\\Windows\\Fonts\\simsun.ttc",     // 宋体
            "C:\\Windows\\Fonts\\arial.ttf",      // Arial
            nullptr
        };
#elif __linux__
        const char* systemFonts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            nullptr
        };
#else
        const char* systemFonts[] = {
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/SFNSMono.ttf",
            nullptr
        };
#endif
        for (int i = 0; systemFonts[i] != nullptr; ++i) {
            if (FT_New_Face(library, systemFonts[i], 0, &face) == 0) {
                fontLoaded = true;
                // 选择 Unicode charmap 以支持中文等多语言字符
                FT_Select_Charmap(face, FT_ENCODING_UNICODE);
                break;
            }
        }
    }

    if (!fontLoaded) {
        FT_Done_FreeType(library);
        return false;
    }

    // 使用 FT_Set_Char_Size 以支持 DPI 缩放
    // fontSize 视为基于 96 DPI 的设计像素大小
    // 转换为点大小: pointSize = fontSize * 72.0 / 96
    // 然后按目标 DPI 缩放: 实际像素 = pointSize * dpi / 72 = fontSize * dpi / 96
    int dpi = (opts.dpi > 0) ? opts.dpi : 96;
    double pointSize = static_cast<double>(opts.fontSize) * 72.0 / 96.0;
    FT_F26Dot6 charSize = static_cast<FT_F26Dot6>(pointSize * 64.0);
    if (FT_Set_Char_Size(face, 0, charSize, static_cast<FT_UInt>(dpi), 0) != 0) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    // 测量文本
    int lineHeight = static_cast<int>(face->size->metrics.height / 64.0);
    int ascender = static_cast<int>(face->size->metrics.ascender / 64.0);
    int descender = static_cast<int>(face->size->metrics.descender / 64.0);
    if (lineHeight <= 0) lineHeight = opts.fontSize;
    if (ascender <= 0) ascender = opts.fontSize;
    if (opts.lineHeight > 0) lineHeight = opts.lineHeight;

    // 将文本按 \n 分割为多行
    std::vector<std::string> rawLines;
    {
        std::string current;
        for (size_t i = 0; i < opts.text.size(); ++i) {
            if (opts.text[i] == '\n') {
                rawLines.push_back(current);
                current.clear();
            } else {
                current += opts.text[i];
            }
        }
        rawLines.push_back(current);
    }

    // 测量单个 UTF-8 字符的宽度
    auto measureCharWidth = [&](size_t pos) -> int {
        uint32_t cp = 0;
        utf8Decode(opts.text, pos, cp);
        FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
        if (glyphIndex == 0) return 0;
        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0) return 0;
        return face->glyph->advance.x >> 6;
    };

    // 测量一行文本的宽度（按 UTF-8 字符遍历）
    auto measureLine = [&](const std::string& line) -> int {
        int w = 0;
        for (size_t i = 0; i < line.size(); ) {
            uint32_t cp = 0;
            int bytes = utf8Decode(line, i, cp);
            if (bytes == 0) break;
            FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
            if (glyphIndex == 0) { i += bytes; continue; }
            if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0) { i += bytes; continue; }
            w += face->glyph->advance.x >> 6;
            i += bytes;
        }
        return w;
    };

    // 如果设置了 maxWidth，对每行进行自动换行
    std::vector<std::string> lines;
    if (opts.maxWidth > 0) {
        for (const auto& rawLine : rawLines) {
            if (rawLine.empty()) {
                lines.push_back("");
                continue;
            }
            std::string current;
            int currentWidth = 0;
            for (size_t i = 0; i < rawLine.size(); ) {
                uint32_t cp = 0;
                int bytes = utf8Decode(rawLine, i, cp);
                if (bytes == 0) break;
                FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
                int charWidth = 0;
                if (glyphIndex != 0 && FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) == 0) {
                    charWidth = face->glyph->advance.x >> 6;
                }
                if (currentWidth + charWidth > opts.maxWidth && !current.empty()) {
                    lines.push_back(current);
                    current.clear();
                    currentWidth = 0;
                }
                current += rawLine.substr(i, bytes);
                currentWidth += charWidth;
                i += bytes;
            }
            lines.push_back(current);
        }
    } else {
        lines = std::move(rawLines);
    }

    // 计算总尺寸
    int maxLineWidth = 0;
    for (const auto& line : lines) {
        int lw = measureLine(line);
        if (lw > maxLineWidth) maxLineWidth = lw;
    }

    int totalHeight = static_cast<int>(lines.size()) * lineHeight + 4;

    out.width = (opts.width > 0) ? opts.width : maxLineWidth + 4;
    out.height = (opts.height > 0) ? opts.height : totalHeight + 4;
    out.format = PixelFormat::BGRA32;
    out.stride = out.width * 4;
    out.data.resize(static_cast<size_t>(out.stride) * out.height, 0);

    // 填充背景色
    uint8_t bgB = (opts.bgColor >> 16) & 0xFF;
    uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
    uint8_t bgR = opts.bgColor & 0xFF;

    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            size_t idx = (y * out.stride) + x * 4;
            out.data[idx] = bgB;
            out.data[idx + 1] = bgG;
            out.data[idx + 2] = bgR;
            out.data[idx + 3] = 0xFF;
        }
    }

    // 渲染文本
    uint8_t fgR = opts.fgColor & 0xFF;
    uint8_t fgG = (opts.fgColor >> 8) & 0xFF;
    uint8_t fgB = (opts.fgColor >> 16) & 0xFF;

    int penY = ascender + 2;

    for (const auto& line : lines) {
        int lineW = measureLine(line);
        int penX = 2;

        // 对齐
        if (opts.alignment == 1) {
            // 居中
            penX = (out.width - lineW) / 2;
            if (penX < 2) penX = 2;
        } else if (opts.alignment == 2) {
            // 右对齐
            penX = out.width - lineW - 2;
            if (penX < 2) penX = 2;
        }

        for (size_t i = 0; i < line.size(); ) {
            uint32_t cp = 0;
            int bytes = utf8Decode(line, i, cp);
            if (bytes == 0) break;

            FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
            if (glyphIndex == 0) { i += bytes; continue; }

            if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER) != 0) { i += bytes; continue; }

            FT_Bitmap& bitmap = face->glyph->bitmap;
            int drawX = penX + face->glyph->bitmap_left;
            int drawY = penY - face->glyph->bitmap_top;

            for (int row = 0; row < static_cast<int>(bitmap.rows); ++row) {
                for (int col = 0; col < static_cast<int>(bitmap.width); ++col) {
                    int px = drawX + col;
                    int py = drawY + row;

                    if (px < 0 || px >= out.width || py < 0 || py >= out.height) continue;

                    uint8_t alpha = bitmap.buffer[row * bitmap.pitch + col];
                    if (alpha == 0) continue;

                    size_t idx = (py * out.stride) + px * 4;

                    if (opts.antiAlias) {
                        float a = alpha / 255.0f;
                        out.data[idx] = static_cast<uint8_t>(bgB * (1 - a) + fgB * a);
                        out.data[idx + 1] = static_cast<uint8_t>(bgG * (1 - a) + fgG * a);
                        out.data[idx + 2] = static_cast<uint8_t>(bgR * (1 - a) + fgR * a);
                    } else {
                        if (alpha > 127) {
                            out.data[idx] = fgB;
                            out.data[idx + 1] = fgG;
                            out.data[idx + 2] = fgR;
                        }
                    }
                }
            }

            penX += face->glyph->advance.x >> 6;
            i += bytes;
        }

        penY += lineHeight;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    // 根据 bitsPerPixel 转换为指定格式
    if (opts.bitsPerPixel == 1 || opts.bitsPerPixel == 4 || opts.bitsPerPixel == 8) {
        // 转换为索引格式
        convertToIndexed(out, opts.bitsPerPixel, opts.fgColor, opts.bgColor);
    } else if (opts.bitsPerPixel == 24) {
        // BGR24
        convertToRGB24(out);
    }
    // 32位或0保持BGRA32不变

    return true;
}

// 辅助函数：将 BGRA32 转换为索引格式
static void convertToIndexed(ImageBuffer& img, int bits, uint32_t fgColor, uint32_t bgColor) {
    if (img.format != PixelFormat::BGRA32) return;

    int paletteSize = 1 << bits;
    img.palette.resize(paletteSize * 4);

    // 填充调色板 (0=背景色, 1=前景色, 其他=0)
    for (int i = 0; i < paletteSize; ++i) {
        uint32_t color = (i == 0) ? bgColor : ((i == 1) ? fgColor : 0);
        img.palette[i * 4] = color & 0xFF;         // R
        img.palette[i * 4 + 1] = (color >> 8) & 0xFF;  // G
        img.palette[i * 4 + 2] = (color >> 16) & 0xFF; // B
        img.palette[i * 4 + 3] = 0xFF;             // A
    }

    int pixelsPerByte = 8 / bits;
    int mask = (1 << bits) - 1;
    int newStride = ((img.width * bits + 31) / 32) * 4; // 4字节对齐

    std::vector<uint8_t> newData(newStride * img.height, 0);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t srcIdx = y * img.stride + x * 4;
            uint8_t b = img.data[srcIdx];
            uint8_t g = img.data[srcIdx + 1];
            uint8_t r = img.data[srcIdx + 2];
            uint32_t color = (r << 16) | (g << 8) | b;

            // 找到最接近的调色板索引
            int index = (color == bgColor) ? 0 : 1;

            int byteIdx = y * newStride + x / pixelsPerByte;
            int shift = (pixelsPerByte - 1 - (x % pixelsPerByte)) * bits;
            newData[byteIdx] |= (index & mask) << shift;
        }
    }

    img.data = std::move(newData);
    img.stride = newStride;

    switch (bits) {
        case 1: img.format = PixelFormat::Indexed1; break;
        case 4: img.format = PixelFormat::Indexed4; break;
        case 8: img.format = PixelFormat::Indexed8; break;
    }
}

// 辅助函数：将 BGRA32 转换为 BGR24
static void convertToRGB24(ImageBuffer& img) {
    if (img.format != PixelFormat::BGRA32) return;

    int newStride = ((img.width * 3 + 3) / 4) * 4;
    std::vector<uint8_t> newData(newStride * img.height);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t srcIdx = y * img.stride + x * 4;
            size_t dstIdx = y * newStride + x * 3;
            newData[dstIdx] = img.data[srcIdx];     // B
            newData[dstIdx + 1] = img.data[srcIdx + 1]; // G
            newData[dstIdx + 2] = img.data[srcIdx + 2]; // R
        }
    }

    img.data = std::move(newData);
    img.stride = newStride;
    img.format = PixelFormat::BGR24;
}

// ============================================================
// 工厂函数 (跨平台实现)
// ============================================================

std::unique_ptr<IImageCodec> createCrossCodec() {
    return std::unique_ptr<IImageCodec>(new CrossImageCodec());
}

std::unique_ptr<ITextRenderer> createCrossTextRenderer() {
    return std::unique_ptr<ITextRenderer>(new CrossTextRenderer());
}

} // namespace imgproc
