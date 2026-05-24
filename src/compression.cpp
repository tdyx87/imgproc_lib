#include "imgproc/compression.hpp"
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <algorithm>

extern "C" {
#include <jpeglib.h>
}

namespace imgproc {

namespace {

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) / 1000.0;
}

} // anonymous namespace

// ============================================================
// RLE 压缩
// ============================================================

CompressionResult compressRLE(const uint8_t* data, size_t size,
                              int width, int height, PixelFormat format) {
    CompressionResult result;
    result.type = CompressionType::RLE;

    auto t0 = nowMs();

    // RLE 格式: [width(4)][height(4)][format(4)][count(4)][value]...
    // 简单的行程编码: 重复字节用 [count][value] 表示
    result.data.reserve(12 + size);

    // 写入头信息
    auto writeU32 = [&](uint32_t v) {
        result.data.push_back(static_cast<uint8_t>(v & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };

    writeU32(static_cast<uint32_t>(width));
    writeU32(static_cast<uint32_t>(height));
    writeU32(static_cast<uint32_t>(format));

    size_t i = 0;
    while (i < size) {
        uint8_t current = data[i];
        size_t count = 1;

        while (i + count < size && data[i + count] == current && count < 255) {
            ++count;
        }

        result.data.push_back(static_cast<uint8_t>(count));
        result.data.push_back(current);
        i += count;
    }

    auto t1 = nowMs();
    result.elapsedMs = t1 - t0;
    result.compressionRatio = (size > 0) ? static_cast<double>(size) / result.data.size() : 0.0;

    return result;
}

bool decompressRLE(const uint8_t* data, size_t size,
                   std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format) {
    if (size < 12) return false;

    auto readU32 = [&](size_t offset) -> uint32_t {
        return static_cast<uint32_t>(data[offset])
             | (static_cast<uint32_t>(data[offset + 1]) << 8)
             | (static_cast<uint32_t>(data[offset + 2]) << 16)
             | (static_cast<uint32_t>(data[offset + 3]) << 24);
    };

    width = static_cast<int>(readU32(0));
    height = static_cast<int>(readU32(4));
    format = static_cast<PixelFormat>(readU32(8));

    out.clear();
    out.reserve(size * 2); // 预估解压后大小

    size_t i = 12;
    while (i + 1 < size) {
        uint8_t count = data[i];
        uint8_t value = data[i + 1];
        out.insert(out.end(), count, value);
        i += 2;
    }

    return true;
}

std::unique_ptr<ICompression> createRLECompression() {
    class RLECompressionImpl : public ICompression {
    public:
        CompressionResult compress(const uint8_t* data, size_t size,
                                   int width, int height, PixelFormat format) override {
            return imgproc::compressRLE(data, size, width, height, format);
        }

        bool decompress(const uint8_t* compressed, size_t compressedSize,
                        std::vector<uint8_t>& out, int& width, int& height,
                        PixelFormat& format) override {
            return imgproc::decompressRLE(compressed, compressedSize, out, width, height, format);
        }
    };

    return std::unique_ptr<ICompression>(new RLECompressionImpl());
}

// ============================================================
// Delta Row 压缩
// ============================================================

CompressionResult compressDeltaRow(const uint8_t* data, size_t size,
                                   int width, int height, PixelFormat format) {
    CompressionResult result;
    result.type = CompressionType::DeltaRow;

    auto t0 = nowMs();

    if (height <= 0 || width <= 0 || size == 0) {
        result.elapsedMs = nowMs() - t0;
        return result;
    }

    int stride = static_cast<int>(size) / height;

    // 头信息: [width(4)][height(4)][format(4)][stride(4)]
    auto writeU32 = [&](uint32_t v) {
        result.data.push_back(static_cast<uint8_t>(v & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };

    writeU32(static_cast<uint32_t>(width));
    writeU32(static_cast<uint32_t>(height));
    writeU32(static_cast<uint32_t>(format));
    writeU32(static_cast<uint32_t>(stride));

    // 第一行原样存储
    result.data.insert(result.data.end(), data, data + stride);

    // 后续行存储与前一行差异
    for (int row = 1; row < height; ++row) {
        const uint8_t* prev = data + (row - 1) * stride;
        const uint8_t* curr = data + row * stride;

        for (int col = 0; col < stride; ++col) {
            result.data.push_back(static_cast<uint8_t>(curr[col] - prev[col]));
        }
    }

    auto t1 = nowMs();
    result.elapsedMs = t1 - t0;
    result.compressionRatio = (size > 0) ? static_cast<double>(size) / result.data.size() : 0.0;

    return result;
}

bool decompressDeltaRow(const uint8_t* data, size_t size,
                        std::vector<uint8_t>& out, int& width, int& height,
                        PixelFormat& format) {
    if (size < 16) return false;

    auto readU32 = [&](size_t offset) -> uint32_t {
        return static_cast<uint32_t>(data[offset])
             | (static_cast<uint32_t>(data[offset + 1]) << 8)
             | (static_cast<uint32_t>(data[offset + 2]) << 16)
             | (static_cast<uint32_t>(data[offset + 3]) << 24);
    };

    width = static_cast<int>(readU32(0));
    height = static_cast<int>(readU32(4));
    format = static_cast<PixelFormat>(readU32(8));
    int stride = static_cast<int>(readU32(12));

    if (stride <= 0 || height <= 0 || size < static_cast<size_t>(16 + stride * height)) {
        return false;
    }

    out.resize(static_cast<size_t>(stride) * height);

    // 第一行原样复制
    std::memcpy(out.data(), data + 16, stride);

    // 后续行恢复
    for (int row = 1; row < height; ++row) {
        const uint8_t* prev = out.data() + (row - 1) * stride;
        uint8_t* curr = out.data() + row * stride;
        const uint8_t* delta = data + 16 + row * stride;

        for (int col = 0; col < stride; ++col) {
            curr[col] = static_cast<uint8_t>(prev[col] + delta[col]);
        }
    }

    return true;
}

std::unique_ptr<ICompression> createDeltaRowCompression() {
    class DeltaRowCompressionImpl : public ICompression {
    public:
        CompressionResult compress(const uint8_t* data, size_t size,
                                   int width, int height, PixelFormat format) override {
            return imgproc::compressDeltaRow(data, size, width, height, format);
        }

        bool decompress(const uint8_t* compressed, size_t compressedSize,
                        std::vector<uint8_t>& out, int& width, int& height,
                        PixelFormat& format) override {
            return imgproc::decompressDeltaRow(compressed, compressedSize, out, width, height, format);
        }
    };

    return std::unique_ptr<ICompression>(new DeltaRowCompressionImpl());
}

// ============================================================
// JPEG 压缩
// ============================================================

namespace {

int bytesPerPixel(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::Grayscale8: return 1;
        case PixelFormat::RGB24:      return 3;
        case PixelFormat::RGBA32:     return 4;
        case PixelFormat::BGR24:      return 3;
        case PixelFormat::BGRA32:     return 4;
        default: return 3;
    }
}

} // anonymous namespace

CompressionResult compressJPEG(const uint8_t* data, size_t size,
                               int width, int height, PixelFormat format,
                               int quality) {
    CompressionResult result;
    result.type = CompressionType::JPEG;

    auto t0 = nowMs();

    if (width <= 0 || height <= 0 || size == 0) {
        result.elapsedMs = nowMs() - t0;
        return result;
    }

    int bpp = bytesPerPixel(format);
    int inputStride = width * bpp;

    // 使用 libjpeg-turbo 进行内存压缩
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // 设置输出到内存
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

    cinfo.image_width = static_cast<JDIMENSION>(width);
    cinfo.image_height = static_cast<JDIMENSION>(height);
    cinfo.input_components = 3; // JPEG 只支持 RGB/灰度
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    // 写入头信息: [width(4)][height(4)][format(4)]
    auto writeU32 = [&](uint32_t v) {
        result.data.push_back(static_cast<uint8_t>(v & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        result.data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };

    writeU32(static_cast<uint32_t>(width));
    writeU32(static_cast<uint32_t>(height));
    writeU32(static_cast<uint32_t>(format));

    std::vector<JSAMPROW> rowPointers(height);
    std::vector<uint8_t> rgbRow(static_cast<size_t>(width) * 3);

    while (cinfo.next_scanline < cinfo.image_height) {
        int y = static_cast<int>(cinfo.next_scanline);
        const uint8_t* srcRow = data + y * inputStride;

        // 转换为 RGB
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            switch (format) {
                case PixelFormat::RGB24:
                    r = srcRow[x * 3];
                    g = srcRow[x * 3 + 1];
                    b = srcRow[x * 3 + 2];
                    break;
                case PixelFormat::BGR24:
                    b = srcRow[x * 3];
                    g = srcRow[x * 3 + 1];
                    r = srcRow[x * 3 + 2];
                    break;
                case PixelFormat::RGBA32:
                    r = srcRow[x * 4];
                    g = srcRow[x * 4 + 1];
                    b = srcRow[x * 4 + 2];
                    break;
                case PixelFormat::BGRA32:
                    b = srcRow[x * 4];
                    g = srcRow[x * 4 + 1];
                    r = srcRow[x * 4 + 2];
                    break;
                case PixelFormat::Grayscale8:
                    r = g = b = srcRow[x];
                    break;
                default:
                    r = g = b = srcRow[x * 3];
                    break;
            }
            rgbRow[x * 3] = r;
            rgbRow[x * 3 + 1] = g;
            rgbRow[x * 3 + 2] = b;
        }

        rowPointers[y] = rgbRow.data();
        jpeg_write_scanlines(&cinfo, &rowPointers[y], 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    // 将 JPEG 数据追加到结果中
    result.data.insert(result.data.end(), jpegBuf, jpegBuf + jpegSize);

    if (jpegBuf) {
        free(jpegBuf);
    }

    auto t1 = nowMs();
    result.elapsedMs = t1 - t0;
    result.compressionRatio = (size > 0) ? static_cast<double>(size) / result.data.size() : 0.0;

    return result;
}

bool decompressJPEG(const uint8_t* data, size_t size,
                    std::vector<uint8_t>& out, int& width, int& height,
                    PixelFormat& format) {
    if (size < 12) return false;

    auto readU32 = [&](size_t offset) -> uint32_t {
        return static_cast<uint32_t>(data[offset])
             | (static_cast<uint32_t>(data[offset + 1]) << 8)
             | (static_cast<uint32_t>(data[offset + 2]) << 16)
             | (static_cast<uint32_t>(data[offset + 3]) << 24);
    };

    width = static_cast<int>(readU32(0));
    height = static_cast<int>(readU32(4));
    format = static_cast<PixelFormat>(readU32(8));

    if (width <= 0 || height <= 0) return false;

    const uint8_t* jpegData = data + 12;
    size_t jpegDataSize = size - 12;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<uint8_t*>(jpegData), static_cast<unsigned long>(jpegDataSize));

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int bpp = bytesPerPixel(format);
    out.resize(static_cast<size_t>(width) * height * bpp);

    std::vector<JSAMPROW> rowPointers(1);
    std::vector<uint8_t> rgbRow(static_cast<size_t>(cinfo.output_width) * cinfo.output_components);

    while (cinfo.output_scanline < cinfo.output_height) {
        rowPointers[0] = rgbRow.data();
        jpeg_read_scanlines(&cinfo, rowPointers.data(), 1);

        int y = static_cast<int>(cinfo.output_scanline - 1);
        for (JDIMENSION x = 0; x < cinfo.output_width; ++x) {
            uint8_t r = rgbRow[x * 3];
            uint8_t g = rgbRow[x * 3 + 1];
            uint8_t b = rgbRow[x * 3 + 2];

            switch (format) {
                case PixelFormat::RGB24:
                    out[y * width * 3 + x * 3] = r;
                    out[y * width * 3 + x * 3 + 1] = g;
                    out[y * width * 3 + x * 3 + 2] = b;
                    break;
                case PixelFormat::BGR24:
                    out[y * width * 3 + x * 3] = b;
                    out[y * width * 3 + x * 3 + 1] = g;
                    out[y * width * 3 + x * 3 + 2] = r;
                    break;
                case PixelFormat::Grayscale8:
                    out[y * width + x] = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                    break;
                default:
                    out[y * width * 3 + x * 3] = r;
                    out[y * width * 3 + x * 3 + 1] = g;
                    out[y * width * 3 + x * 3 + 2] = b;
                    break;
            }
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return true;
}

std::unique_ptr<ICompression> createJPEGCompression(int quality) {
    class JPEGCompressionImpl : public ICompression {
    public:
        explicit JPEGCompressionImpl(int q) : quality_(q) {}

        CompressionResult compress(const uint8_t* data, size_t size,
                                   int width, int height, PixelFormat format) override {
            return imgproc::compressJPEG(data, size, width, height, format, quality_);
        }

        bool decompress(const uint8_t* compressed, size_t compressedSize,
                        std::vector<uint8_t>& out, int& width, int& height,
                        PixelFormat& format) override {
            return imgproc::decompressJPEG(compressed, compressedSize, out, width, height, format);
        }

    private:
        int quality_;
    };

    return std::unique_ptr<ICompression>(new JPEGCompressionImpl(quality));
}

} // namespace imgproc
