#include "imgproc/platform/win_codec.hpp"

#ifdef USE_WINDOWS_API

// 必须在 GDI+ 头文件之前包含标准库
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <codecvt>
#include <locale>
#include <memory>
#include <algorithm>

// 包含 Windows 和 GDI+ 头文件
#include <windows.h>

// GDI+ 定义了 PixelFormat 为 INT 的 typedef，与我们的 imgproc::PixelFormat 冲突
// 在包含 gdiplus.h 之前不做特殊处理，之后用命名空间区分
#include <gdiplus.h>
#include <objbase.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

namespace imgproc {

// 在此命名空间内，PixelFormat 指向 imgproc::PixelFormat（我们的枚举）
// GDI+ 的 PixelFormat 在全局命名空间，需要用 ::PixelFormat 或 Gdiplus::PixelFormat 访问

namespace {

// GDI+ 初始化
struct GdiplusInit {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token;
    Gdiplus::Status status;

    GdiplusInit() : status(Gdiplus::GenericError) {
        input.GdiplusVersion = 1;
        input.DebugEventCallback = nullptr;
        input.SuppressBackgroundThread = FALSE;
        input.SuppressExternalCodecs = FALSE;
        status = Gdiplus::GdiplusStartup(&token, &input, nullptr);
    }

    ~GdiplusInit() {
        if (status == Gdiplus::Ok) {
            Gdiplus::GdiplusShutdown(token);
        }
    }

    bool isOk() const { return status == Gdiplus::Ok; }
};

static GdiplusInit& getGdiplus() {
    static GdiplusInit instance;
    return instance;
}

std::wstring toWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring wstr(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
    return wstr;
}

int getEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    auto* pImageCodecInfo = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
    if (!pImageCodecInfo) return -1;

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[i].Clsid;
            free(pImageCodecInfo);
            return i;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// 将我们的 PixelFormat 枚举转换为 GDI+ 的 PixelFormat (INT)
// 注意：参数使用 imgproc::PixelFormat 以避免与全局 ::PixelFormat 冲突
static int oursToGdiplusPixelFormat(imgproc::PixelFormat fmt) {
    switch (fmt) {
        case imgproc::PixelFormat::RGB24:      return PixelFormat24bppRGB;
        case imgproc::PixelFormat::RGBA32:     return PixelFormat32bppARGB;
        case imgproc::PixelFormat::BGR24:      return PixelFormat24bppRGB;
        case imgproc::PixelFormat::BGRA32:     return PixelFormat32bppARGB;
        default:                               return PixelFormat24bppRGB;
    }
}

// 将 ImageBuffer 数据拷贝到 BGRA32 对齐缓冲区
static void prepareGdiplusBitmapData(
    const ImageBuffer& img, std::vector<uint8_t>& outData, int& outStride)
{
    outStride = img.width * 4;
    outStride = (outStride + 3) & ~3; // 4 字节对齐
    outData.resize(static_cast<size_t>(outStride) * img.height, 0);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t dstIdx = static_cast<size_t>(y) * outStride + x * 4;
            size_t srcIdx = static_cast<size_t>(y) * img.stride + x * 4;

            switch (img.format) {
                case imgproc::PixelFormat::BGR24:
                    srcIdx = static_cast<size_t>(y) * img.stride + x * 3;
                    outData[dstIdx]     = img.data[srcIdx];     // B
                    outData[dstIdx + 1] = img.data[srcIdx + 1]; // G
                    outData[dstIdx + 2] = img.data[srcIdx + 2]; // R
                    outData[dstIdx + 3] = 0xFF;
                    break;
                case imgproc::PixelFormat::RGB24:
                    srcIdx = static_cast<size_t>(y) * img.stride + x * 3;
                    outData[dstIdx]     = img.data[srcIdx + 2]; // B
                    outData[dstIdx + 1] = img.data[srcIdx + 1]; // G
                    outData[dstIdx + 2] = img.data[srcIdx];     // R
                    outData[dstIdx + 3] = 0xFF;
                    break;
                case imgproc::PixelFormat::BGRA32:
                    outData[dstIdx]     = img.data[srcIdx];     // B
                    outData[dstIdx + 1] = img.data[srcIdx + 1]; // G
                    outData[dstIdx + 2] = img.data[srcIdx + 2]; // R
                    outData[dstIdx + 3] = img.data[srcIdx + 3]; // A
                    break;
                case imgproc::PixelFormat::RGBA32:
                    outData[dstIdx]     = img.data[srcIdx + 2]; // B
                    outData[dstIdx + 1] = img.data[srcIdx + 1]; // G
                    outData[dstIdx + 2] = img.data[srcIdx];     // R
                    outData[dstIdx + 3] = img.data[srcIdx + 3]; // A
                    break;
                case imgproc::PixelFormat::Grayscale8:
                    srcIdx = static_cast<size_t>(y) * img.stride + x;
                    outData[dstIdx]     = img.data[srcIdx];
                    outData[dstIdx + 1] = img.data[srcIdx];
                    outData[dstIdx + 2] = img.data[srcIdx];
                    outData[dstIdx + 3] = 0xFF;
                    break;
                case imgproc::PixelFormat::Indexed8:
                    if (!img.palette.empty()) {
                        srcIdx = static_cast<size_t>(y) * img.stride + x;
                        uint8_t idx = img.data[srcIdx];
                        if (idx * 4 + 3 < img.palette.size()) {
                            outData[dstIdx]     = img.palette[idx * 4];     // B
                            outData[dstIdx + 1] = img.palette[idx * 4 + 1]; // G
                            outData[dstIdx + 2] = img.palette[idx * 4 + 2]; // R
                            outData[dstIdx + 3] = img.palette[idx * 4 + 3]; // A
                        }
                    }
                    break;
                case imgproc::PixelFormat::Indexed1:
                    if (!img.palette.empty()) {
                        int byteIdx = x / 8;
                        int bitIdx = 7 - (x % 8);
                        srcIdx = static_cast<size_t>(y) * img.stride + byteIdx;
                        uint8_t idx = (img.data[srcIdx] >> bitIdx) & 1;
                        if (idx * 4 + 3 < img.palette.size()) {
                            outData[dstIdx]     = img.palette[idx * 4];     // B
                            outData[dstIdx + 1] = img.palette[idx * 4 + 1]; // G
                            outData[dstIdx + 2] = img.palette[idx * 4 + 2]; // R
                            outData[dstIdx + 3] = img.palette[idx * 4 + 3]; // A
                        }
                    }
                    break;
                default:
                    outData[dstIdx]     = 0xFF;
                    outData[dstIdx + 1] = 0xFF;
                    outData[dstIdx + 2] = 0xFF;
                    outData[dstIdx + 3] = 0xFF;
                    break;
            }
        }
    }
}

// 创建 GDI+ Bitmap 并写入像素数据
static Gdiplus::Bitmap* createGdiplusBitmap(const ImageBuffer& img) {
    std::vector<uint8_t> bmpData;
    int bmpStride = 0;
    prepareGdiplusBitmapData(img, bmpData, bmpStride);

    // 使用 LockBits 方式创建 Bitmap（兼容所有 Windows SDK 版本）
    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(img.width, img.height, PixelFormat32bppARGB);
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        return nullptr;
    }

    Gdiplus::Rect rect(0, 0, img.width, img.height);
    Gdiplus::BitmapData bmpDataInfo;
    if (bitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpDataInfo) != Gdiplus::Ok) {
        delete bitmap;
        return nullptr;
    }

    for (int y = 0; y < img.height; ++y) {
        std::memcpy(
            static_cast<uint8_t*>(bmpDataInfo.Scan0) + y * bmpDataInfo.Stride,
            bmpData.data() + y * bmpStride,
            bmpStride);
    }

    bitmap->UnlockBits(&bmpDataInfo);
    return bitmap;
}

} // anonymous namespace

// ============================================================
// WinImageCodec
// ============================================================

WinImageCodec::WinImageCodec() {
    auto& gdi = getGdiplus();
    if (!gdi.isOk()) {
        throw std::runtime_error("GDI+ initialization failed");
    }
}

WinImageCodec::~WinImageCodec() = default;

bool WinImageCodec::loadFromFile(const std::string& path, ImageBuffer& out) {
    try {
        std::wstring wpath = toWideString(path);
        Gdiplus::Image image(wpath.c_str());
        if (image.GetLastStatus() != Gdiplus::Ok) return false;

        out.width = image.GetWidth();
        out.height = image.GetHeight();

        // 始终解码为 BGRA32 格式
        out.format = imgproc::PixelFormat::BGRA32;
        out.stride = out.width * 4;
        out.data.resize(static_cast<size_t>(out.stride) * out.height, 0);

        // 创建 BGRA32 Bitmap，然后绘制源图像
        Gdiplus::Bitmap bitmap(out.width, out.height, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.DrawImage(&image, 0, 0, out.width, out.height);

        // 锁定位图获取像素数据
        Gdiplus::Rect rect(0, 0, out.width, out.height);
        Gdiplus::BitmapData bmpData;
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Gdiplus::Ok) {
            return false;
        }

        for (int y = 0; y < out.height; ++y) {
            std::memcpy(
                out.data.data() + y * out.stride,
                static_cast<uint8_t*>(bmpData.Scan0) + y * bmpData.Stride,
                out.stride);
        }

        bitmap.UnlockBits(&bmpData);
        return true;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::loadFromMemory(const uint8_t* data, size_t size, ImageType type, ImageBuffer& out) {
    (void)type; // GDI+ 自动检测格式
    try {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!hMem) return false;

        void* pMem = GlobalLock(hMem);
        if (!pMem) {
            GlobalFree(hMem);
            return false;
        }
        std::memcpy(pMem, data, size);
        GlobalUnlock(hMem);

        IStream* pStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
            GlobalFree(hMem);
            return false;
        }

        Gdiplus::Image image(pStream);
        pStream->Release();

        if (image.GetLastStatus() != Gdiplus::Ok) return false;

        out.width = image.GetWidth();
        out.height = image.GetHeight();

        out.format = imgproc::PixelFormat::BGRA32;
        out.stride = out.width * 4;
        out.data.resize(static_cast<size_t>(out.stride) * out.height, 0);

        Gdiplus::Bitmap bitmap(out.width, out.height, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.DrawImage(&image, 0, 0, out.width, out.height);

        Gdiplus::Rect rect(0, 0, out.width, out.height);
        Gdiplus::BitmapData bmpData;
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Gdiplus::Ok) {
            return false;
        }

        for (int y = 0; y < out.height; ++y) {
            std::memcpy(
                out.data.data() + y * out.stride,
                static_cast<uint8_t*>(bmpData.Scan0) + y * bmpData.Stride,
                out.stride);
        }

        bitmap.UnlockBits(&bmpData);
        return true;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::saveToJpeg(const ImageBuffer& img, const std::string& path,
                               std::vector<uint8_t>* memOut, int quality) {
    try {
        CLSID clsid;
        if (getEncoderClsid(L"image/jpeg", &clsid) < 0) return false;

        Gdiplus::Bitmap* bitmap = createGdiplusBitmap(img);
        if (!bitmap) return false;

        // 设置 JPEG 质量
        Gdiplus::EncoderParameters encoderParams;
        encoderParams.Count = 1;
        encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
        encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        encoderParams.Parameter[0].NumberOfValues = 1;
        encoderParams.Parameter[0].Value = &quality;

        Gdiplus::Status status = Gdiplus::Ok;

        if (memOut) {
            IStream* pStream = nullptr;
            if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &pStream))) {
                delete bitmap;
                return false;
            }

            status = bitmap->Save(pStream, &clsid, &encoderParams);

            if (status == Gdiplus::Ok) {
                LARGE_INTEGER liZero = {0};
                ULARGE_INTEGER liSize;
                pStream->Seek(liZero, STREAM_SEEK_END, &liSize);
                pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);

                memOut->resize(static_cast<size_t>(liSize.QuadPart));
                ULONG bytesRead = 0;
                pStream->Read(memOut->data(), static_cast<ULONG>(memOut->size()), &bytesRead);
                memOut->resize(bytesRead);
            }

            pStream->Release();
        } else {
            std::wstring wpath = toWideString(path);
            status = bitmap->Save(wpath.c_str(), &clsid, &encoderParams);
        }

        delete bitmap;
        return status == Gdiplus::Ok;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::saveToJpegFile(const ImageBuffer& img, const std::string& path, int quality) {
    return saveToJpeg(img, path, nullptr, quality);
}

bool WinImageCodec::saveToJpegMemory(const ImageBuffer& img, std::vector<uint8_t>& out, int quality) {
    return saveToJpeg(img, std::string(), &out, quality);
}

bool WinImageCodec::saveToBmp(const ImageBuffer& img, const std::string& path,
                              std::vector<uint8_t>* memOut) {
    try {
        CLSID clsid;
        if (getEncoderClsid(L"image/bmp", &clsid) < 0) return false;

        Gdiplus::Bitmap* bitmap = createGdiplusBitmap(img);
        if (!bitmap) return false;

        Gdiplus::Status status = Gdiplus::Ok;

        if (memOut) {
            IStream* pStream = nullptr;
            if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &pStream))) {
                delete bitmap;
                return false;
            }

            status = bitmap->Save(pStream, &clsid, nullptr);

            if (status == Gdiplus::Ok) {
                LARGE_INTEGER liZero = {0};
                ULARGE_INTEGER liSize;
                pStream->Seek(liZero, STREAM_SEEK_END, &liSize);
                pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);

                memOut->resize(static_cast<size_t>(liSize.QuadPart));
                ULONG bytesRead = 0;
                pStream->Read(memOut->data(), static_cast<ULONG>(memOut->size()), &bytesRead);
                memOut->resize(bytesRead);
            }

            pStream->Release();
        } else {
            std::wstring wpath = toWideString(path);
            status = bitmap->Save(wpath.c_str(), &clsid, nullptr);
        }

        delete bitmap;
        return status == Gdiplus::Ok;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::saveToBmpFile(const ImageBuffer& img, const std::string& path) {
    return saveToBmp(img, path, nullptr);
}

bool WinImageCodec::saveToBmpMemory(const ImageBuffer& img, std::vector<uint8_t>& out) {
    return saveToBmp(img, std::string(), &out);
}

bool WinImageCodec::saveToPngFile(const ImageBuffer& img, const std::string& path) {
    // 使用 GDI+ 保存 PNG
    try {
        CLSID clsid;
        if (getEncoderClsid(L"image/png", &clsid) < 0) return false;

        Gdiplus::Bitmap* bitmap = createGdiplusBitmap(img);
        if (!bitmap) return false;

        Gdiplus::Status status = bitmap->Save(std::wstring(path.begin(), path.end()).c_str(), &clsid, nullptr);
        delete bitmap;
        return status == Gdiplus::Ok;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::saveToPngMemory(const ImageBuffer& img, std::vector<uint8_t>& out) {
    // 使用 GDI+ 保存 PNG 到内存
    try {
        CLSID clsid;
        if (getEncoderClsid(L"image/png", &clsid) < 0) return false;

        Gdiplus::Bitmap* bitmap = createGdiplusBitmap(img);
        if (!bitmap) return false;

        IStream* pStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &pStream))) {
            delete bitmap;
            return false;
        }

        Gdiplus::Status status = bitmap->Save(pStream, &clsid, nullptr);

        if (status == Gdiplus::Ok) {
            LARGE_INTEGER liZero = {0};
            ULARGE_INTEGER liSize;
            pStream->Seek(liZero, STREAM_SEEK_END, &liSize);
            pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);

            out.resize(static_cast<size_t>(liSize.QuadPart));
            ULONG bytesRead = 0;
            pStream->Read(out.data(), static_cast<ULONG>(out.size()), &bytesRead);
            out.resize(bytesRead);
        }

        pStream->Release();
        delete bitmap;
        return status == Gdiplus::Ok;
    } catch (...) {
        return false;
    }
}

bool WinImageCodec::saveToFile(const ImageBuffer& img, const std::string& path, int jpegQuality) {
    // 根据文件扩展名选择格式
    auto dotPos = path.rfind('.');
    if (dotPos == std::string::npos) {
        return saveToBmpFile(img, path);
    }

    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "jpg" || ext == "jpeg") {
        return saveToJpegFile(img, path, jpegQuality);
    } else if (ext == "png") {
        return saveToPngFile(img, path);
    } else {
        return saveToBmpFile(img, path);
    }
}

// ============================================================
// 图像格式转换辅助函数 (与 cross_codec.cpp 共享逻辑)
// ============================================================

static void convertToRGB24(imgproc::ImageBuffer& img) {
    if (img.format != imgproc::PixelFormat::BGRA32) return;

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
    img.format = imgproc::PixelFormat::BGR24;
}

static void convertToIndexed(imgproc::ImageBuffer& img, int bits, uint32_t fgColor, uint32_t bgColor) {
    if (img.format != imgproc::PixelFormat::BGRA32) return;

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
        case 1: img.format = imgproc::PixelFormat::Indexed1; break;
        case 4: img.format = imgproc::PixelFormat::Indexed4; break;
        case 8: img.format = imgproc::PixelFormat::Indexed8; break;
    }
}

// ============================================================
// WinTextRenderer
// ============================================================

WinTextRenderer::WinTextRenderer() {
    getGdiplus();
}

WinTextRenderer::~WinTextRenderer() = default;

bool WinTextRenderer::renderToFile(const TextRenderOptions& opts, const std::string& path) {
    ImageBuffer out;
    if (!renderToMemory(opts, out)) return false;

    auto codec = std::unique_ptr<IImageCodec>(new WinImageCodec());
    return codec->saveToBmpFile(out, path);
}

bool WinTextRenderer::renderToMemory(const TextRenderOptions& opts, ImageBuffer& out) {
    try {
        int dpi = (opts.dpi > 0) ? opts.dpi : 96;
        float scale = static_cast<float>(dpi) / 96.0f;
        int scaledFontSize = static_cast<int>(opts.fontSize * scale + 0.5f);
        Gdiplus::REAL pointSize = static_cast<Gdiplus::REAL>(scaledFontSize) * 72.0f / 96.0f;

        // 优先使用黑体，如果不存在则回退到微软雅黑
        std::unique_ptr<Gdiplus::FontFamily> pFontFamily;
        Gdiplus::FontFamily simhei(L"\u9ed1\u4f53");
        Gdiplus::FontFamily yahei(L"Microsoft YaHei");

        if (simhei.IsAvailable()) {
            pFontFamily.reset(new Gdiplus::FontFamily(L"\u9ed1\u4f53"));
        } else if (yahei.IsAvailable()) {
            pFontFamily.reset(new Gdiplus::FontFamily(L"Microsoft YaHei"));
        } else {
            pFontFamily.reset(new Gdiplus::FontFamily());
        }

        Gdiplus::Font font(pFontFamily.get(), pointSize,
                          Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

        // 测量单行高度
        Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
        Gdiplus::Graphics measureG(&measureBmp);
        measureG.SetTextRenderingHint(opts.antiAlias
            ? Gdiplus::TextRenderingHintAntiAliasGridFit
            : Gdiplus::TextRenderingHintSingleBitPerPixel);
        measureG.SetPageUnit(Gdiplus::UnitPixel);

        // 获取字体行高
        int fontLineHeight = scaledFontSize;
        if (opts.lineHeight > 0) {
            fontLineHeight = static_cast<int>(opts.lineHeight * scale + 0.5f);
        } else {
            Gdiplus::RectF lineBounds;
            measureG.MeasureString(L"Ay", 2, &font, Gdiplus::PointF(0, 0), &lineBounds);
            fontLineHeight = static_cast<int>(lineBounds.Height) + 2;
        }

        // 将文本按 \n 分割为多行，并处理自动换行
        std::wstring wtext = toWideString(opts.text);
        std::vector<std::wstring> lines;

        // 按 \n 分割
        std::wstring currentLine;
        for (size_t i = 0; i < wtext.size(); ++i) {
            if (wtext[i] == L'\n') {
                lines.push_back(currentLine);
                currentLine.clear();
            } else {
                currentLine += wtext[i];
            }
        }
        lines.push_back(currentLine);

        // 如果设置了 maxWidth，对每行进行自动换行
        if (opts.maxWidth > 0) {
            std::vector<std::wstring> wrappedLines;
            int maxW = static_cast<int>(opts.maxWidth * scale + 0.5f);

            for (const auto& line : lines) {
                if (line.empty()) {
                    wrappedLines.push_back(L"");
                    continue;
                }

                std::wstring current;
                for (size_t i = 0; i < line.size(); ++i) {
                    current += line[i];
                    Gdiplus::RectF bounds;
                    measureG.MeasureString(current.c_str(), -1, &font,
                                            Gdiplus::PointF(0, 0), &bounds);
                    if (bounds.Width > maxW && current.size() > 1) {
                        // 回退一个字符
                        current.pop_back();
                        wrappedLines.push_back(current);
                        current = line[i];
                    }
                }
                wrappedLines.push_back(current);
            }
            lines = std::move(wrappedLines);
        }

        // 计算总尺寸
        int maxLineWidth = 0;
        for (const auto& line : lines) {
            Gdiplus::RectF bounds;
            measureG.MeasureString(line.c_str(), -1, &font,
                                    Gdiplus::PointF(0, 0), &bounds);
            int lw = static_cast<int>(bounds.Width) + 4;
            if (lw > maxLineWidth) maxLineWidth = lw;
        }

        int totalHeight = static_cast<int>(lines.size()) * fontLineHeight + 4;

        out.width = (opts.width > 0) ? opts.width : maxLineWidth;
        out.height = (opts.height > 0) ? opts.height : totalHeight;
        out.format = imgproc::PixelFormat::BGRA32;
        out.stride = out.width * 4;
        out.data.resize(static_cast<size_t>(out.stride) * out.height, 0xFF);

        // 填充背景色
        uint8_t bgB = (opts.bgColor >> 16) & 0xFF;
        uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
        uint8_t bgR = opts.bgColor & 0xFF;

        // 绘制文本
        Gdiplus::Bitmap bitmap(out.width, out.height, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.SetTextRenderingHint(opts.antiAlias
            ? Gdiplus::TextRenderingHintAntiAliasGridFit
            : Gdiplus::TextRenderingHintSingleBitPerPixel);

        // 先绘制背景
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(0xFF, bgR, bgG, bgB));
        g.FillRectangle(&bgBrush, 0, 0, out.width, out.height);

        // 绘制文本
        Gdiplus::SolidBrush brush(Gdiplus::Color(0xFF,
            opts.fgColor & 0xFF,
            (opts.fgColor >> 8) & 0xFF,
            (opts.fgColor >> 16) & 0xFF));

        // 对齐方式
        Gdiplus::StringFormat strFmt;
        if (opts.alignment == 1) {
            strFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        } else if (opts.alignment == 2) {
            strFmt.SetAlignment(Gdiplus::StringAlignmentFar);
        }

        Gdiplus::RectF layoutRect(2.0f, 2.0f,
                                   static_cast<Gdiplus::REAL>(out.width - 4),
                                   static_cast<Gdiplus::REAL>(out.height - 4));

        // 构建完整文本（用 \n 连接）
        std::wstring fullText;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) fullText += L'\n';
            fullText += lines[i];
        }

        g.DrawString(fullText.c_str(), -1, &font, layoutRect, &strFmt, &brush);

        // 锁定位图获取渲染结果
        Gdiplus::Rect rect(0, 0, out.width, out.height);
        Gdiplus::BitmapData bmpData;
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) == Gdiplus::Ok) {
            for (int y = 0; y < out.height; ++y) {
                std::memcpy(
                    out.data.data() + y * out.stride,
                    static_cast<uint8_t*>(bmpData.Scan0) + y * bmpData.Stride,
                    out.stride);
            }
            bitmap.UnlockBits(&bmpData);
        }

        // 根据 bitsPerPixel 转换为指定格式
        if (opts.bitsPerPixel == 1 || opts.bitsPerPixel == 4 || opts.bitsPerPixel == 8) {
            convertToIndexed(out, opts.bitsPerPixel, opts.fgColor, opts.bgColor);
        } else if (opts.bitsPerPixel == 24) {
            convertToRGB24(out);
        }

        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================
// 工厂函数
// ============================================================

std::unique_ptr<IImageCodec> createWinCodec() {
    return std::unique_ptr<IImageCodec>(new WinImageCodec());
}

std::unique_ptr<ITextRenderer> createWinTextRenderer() {
    return std::unique_ptr<ITextRenderer>(new WinTextRenderer());
}

} // namespace imgproc

#endif // USE_WINDOWS_API
