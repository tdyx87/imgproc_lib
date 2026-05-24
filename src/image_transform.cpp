#include "imgproc/image_transform.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace imgproc {

// 获取像素值（边界处理：clamp）
static uint8_t getPixelClamped(const ImageBuffer& img, int x, int y, int channel) {
    x = std::max(0, std::min(x, img.width - 1));
    y = std::max(0, std::min(y, img.height - 1));

    int bpp = 4; // 默认 BGRA32
    switch (img.format) {
        case PixelFormat::Grayscale8: bpp = 1; break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: bpp = 3; break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32: bpp = 4; break;
        default: bpp = 4; break;
    }

    size_t idx = static_cast<size_t>(y) * img.stride + x * bpp + channel;
    if (idx < img.data.size()) {
        return img.data[idx];
    }
    return 0;
}

// 双线性插值
static uint8_t bilinearInterpolate(const ImageBuffer& img, float x, float y, int channel) {
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = std::min(x0 + 1, img.width - 1);
    int y1 = std::min(y0 + 1, img.height - 1);

    float fx = x - x0;
    float fy = y - y0;

    uint8_t p00 = getPixelClamped(img, x0, y0, channel);
    uint8_t p01 = getPixelClamped(img, x1, y0, channel);
    uint8_t p10 = getPixelClamped(img, x0, y1, channel);
    uint8_t p11 = getPixelClamped(img, x1, y1, channel);

    float val = (1 - fx) * (1 - fy) * p00 +
                fx * (1 - fy) * p01 +
                (1 - fx) * fy * p10 +
                fx * fy * p11;

    return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, val)));
}

// 最近邻插值
static uint8_t nearestInterpolate(const ImageBuffer& img, float x, float y, int channel) {
    int xi = static_cast<int>(std::round(x));
    int yi = static_cast<int>(std::round(y));
    return getPixelClamped(img, xi, yi, channel);
}

bool resizeImage(const ImageBuffer& src, ImageBuffer& dst,
                 int newWidth, int newHeight,
                 Interpolation interp) {
    if (src.data.empty() || newWidth <= 0 || newHeight <= 0) {
        return false;
    }

    // 确定输出格式和通道数
    int srcBpp = 4, dstBpp = 4;
    PixelFormat dstFormat = PixelFormat::BGRA32;

    switch (src.format) {
        case PixelFormat::Grayscale8:
            srcBpp = 1; dstBpp = 1; dstFormat = PixelFormat::Grayscale8;
            break;
        case PixelFormat::RGB24:
            srcBpp = 3; dstBpp = 3; dstFormat = PixelFormat::RGB24;
            break;
        case PixelFormat::BGR24:
            srcBpp = 3; dstBpp = 3; dstFormat = PixelFormat::BGR24;
            break;
        case PixelFormat::RGBA32:
            srcBpp = 4; dstBpp = 4; dstFormat = PixelFormat::RGBA32;
            break;
        case PixelFormat::BGRA32:
            srcBpp = 4; dstBpp = 4; dstFormat = PixelFormat::BGRA32;
            break;
        default:
            // 不支持的格式，转换为 BGRA32
            dstFormat = PixelFormat::BGRA32;
            srcBpp = 4; dstBpp = 4;
            break;
    }

    // 分配输出缓冲区
    dst.width = newWidth;
    dst.height = newHeight;
    dst.format = dstFormat;
    dst.stride = newWidth * dstBpp;
    dst.data.resize(static_cast<size_t>(dst.stride) * newHeight);

    // 缩放比例
    float scaleX = static_cast<float>(src.width) / newWidth;
    float scaleY = static_cast<float>(src.height) / newHeight;

    // 对每个像素进行插值
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            // 源图像中的对应位置（中心对齐）
            float srcX = (x + 0.5f) * scaleX - 0.5f;
            float srcY = (y + 0.5f) * scaleY - 0.5f;

            size_t dstIdx = static_cast<size_t>(y) * dst.stride + x * dstBpp;

            for (int c = 0; c < dstBpp; ++c) {
                if (interp == Interpolation::Nearest) {
                    dst.data[dstIdx + c] = nearestInterpolate(src, srcX, srcY, c);
                } else {
                    // 默认使用双线性
                    dst.data[dstIdx + c] = bilinearInterpolate(src, srcX, srcY, c);
                }
            }
        }
    }

    return true;
}

bool cropImage(const ImageBuffer& src, ImageBuffer& dst,
               int x, int y, int width, int height) {
    if (src.data.empty() || width <= 0 || height <= 0) {
        return false;
    }

    // 边界检查
    if (x < 0 || y < 0 || x + width > src.width || y + height > src.height) {
        return false;
    }

    // 确定通道数
    int bpp = 4;
    switch (src.format) {
        case PixelFormat::Grayscale8: bpp = 1; break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: bpp = 3; break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32: bpp = 4; break;
        default: bpp = 4; break;
    }

    dst.width = width;
    dst.height = height;
    dst.format = src.format;
    dst.stride = width * bpp;
    dst.data.resize(static_cast<size_t>(dst.stride) * height);

    // 逐行复制
    for (int row = 0; row < height; ++row) {
        size_t srcIdx = static_cast<size_t>(y + row) * src.stride + x * bpp;
        size_t dstIdx = static_cast<size_t>(row) * dst.stride;
        std::memcpy(dst.data.data() + dstIdx, src.data.data() + srcIdx, width * bpp);
    }

    return true;
}

bool rotateImage(const ImageBuffer& src, ImageBuffer& dst,
                 double angle, bool expand,
                 Interpolation interp) {
    if (src.data.empty()) return false;

    // 转换为弧度
    double rad = angle * M_PI / 180.0;
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);

    int srcW = src.width;
    int srcH = src.height;

    int dstW, dstH;
    double offsetX, offsetY;

    if (expand) {
        // 计算旋转后的边界框
        double corners[4][2] = {
            {0, 0},
            {srcW, 0},
            {srcW, srcH},
            {0, srcH}
        };

        double minX = 1e10, minY = 1e10, maxX = -1e10, maxY = -1e10;
        for (int i = 0; i < 4; ++i) {
            double rx = corners[i][0] * cosA - corners[i][1] * sinA;
            double ry = corners[i][0] * sinA + corners[i][1] * cosA;
            minX = std::min(minX, rx);
            minY = std::min(minY, ry);
            maxX = std::max(maxX, rx);
            maxY = std::max(maxY, ry);
        }

        dstW = static_cast<int>(std::ceil(maxX - minX));
        dstH = static_cast<int>(std::ceil(maxY - minY));
        offsetX = -minX;
        offsetY = -minY;
    } else {
        dstW = srcW;
        dstH = srcH;
        offsetX = 0;
        offsetY = 0;
    }

    // 确定通道数
    int bpp = 4;
    switch (src.format) {
        case PixelFormat::Grayscale8: bpp = 1; break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: bpp = 3; break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32: bpp = 4; break;
        default: bpp = 4; break;
    }

    dst.width = dstW;
    dst.height = dstH;
    dst.format = src.format;
    dst.stride = dstW * bpp;
    dst.data.resize(static_cast<size_t>(dst.stride) * dstH);

    // 中心点
    double srcCx = srcW / 2.0;
    double srcCy = srcH / 2.0;
    double dstCx = dstW / 2.0;
    double dstCy = dstH / 2.0;

    // 反向映射：从目标像素找到源像素
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            // 目标坐标相对于中心
            double dx = x - dstCx + offsetX;
            double dy = y - dstCy + offsetY;

            // 反向旋转
            double srcX = dx * cosA + dy * sinA + srcCx;
            double srcY = -dx * sinA + dy * cosA + srcCy;

            size_t dstIdx = static_cast<size_t>(y) * dst.stride + x * bpp;

            // 检查源像素是否在有效范围内
            if (srcX >= 0 && srcX < srcW && srcY >= 0 && srcY < srcH) {
                for (int c = 0; c < bpp; ++c) {
                    if (interp == Interpolation::Nearest) {
                        dst.data[dstIdx + c] = nearestInterpolate(src, static_cast<float>(srcX), static_cast<float>(srcY), c);
                    } else {
                        dst.data[dstIdx + c] = bilinearInterpolate(src, static_cast<float>(srcX), static_cast<float>(srcY), c);
                    }
                }
            } else {
                // 超出范围填充黑色/透明
                for (int c = 0; c < bpp; ++c) {
                    dst.data[dstIdx + c] = 0;
                }
            }
        }
    }

    return true;
}

bool flipImage(const ImageBuffer& src, ImageBuffer& dst, FlipMode mode) {
    if (src.data.empty()) return false;

    // 确定通道数
    int bpp = 4;
    switch (src.format) {
        case PixelFormat::Grayscale8: bpp = 1; break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: bpp = 3; break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32: bpp = 4; break;
        default: bpp = 4; break;
    }

    dst.width = src.width;
    dst.height = src.height;
    dst.format = src.format;
    dst.stride = src.stride;
    dst.data.resize(src.data.size());

    bool flipH = (mode == FlipMode::Horizontal || mode == FlipMode::Both);
    bool flipV = (mode == FlipMode::Vertical || mode == FlipMode::Both);

    for (int y = 0; y < src.height; ++y) {
        int srcY = flipV ? (src.height - 1 - y) : y;
        for (int x = 0; x < src.width; ++x) {
            int srcX = flipH ? (src.width - 1 - x) : x;

            size_t srcIdx = static_cast<size_t>(srcY) * src.stride + srcX * bpp;
            size_t dstIdx = static_cast<size_t>(y) * dst.stride + x * bpp;

            for (int c = 0; c < bpp; ++c) {
                dst.data[dstIdx + c] = src.data[srcIdx + c];
            }
        }
    }

    return true;
}

} // namespace imgproc
