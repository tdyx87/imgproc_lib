#include "imgproc/qrcode_generator.hpp"
#include "imgproc/image_codec.hpp"

#include <ZXing/MultiFormatWriter.h>
#include <ZXing/BitMatrix.h>
#include <ZXing/BarcodeFormat.h>

#include <cstring>

namespace imgproc {

bool generateQRCode(const QRCodeGenerateOptions& opts, ImageBuffer& out) {
    auto gen = createQRCodeGenerator();
    if (!gen) return false;
    return gen->generate(opts, out);
}

bool generateQRCodeToFile(const QRCodeGenerateOptions& opts, const std::string& path) {
    auto gen = createQRCodeGenerator();
    if (!gen) return false;
    return gen->generateToFile(opts, path);
}

std::unique_ptr<IQRCodeGenerator> createQRCodeGenerator() {
    class QRCodeGeneratorImpl : public IQRCodeGenerator {
    public:
        bool generate(const QRCodeGenerateOptions& opts, ImageBuffer& out) override {
            try {
                using namespace ZXing;

                // 创建 QR Code 编码器
                MultiFormatWriter writer(BarcodeFormat::QRCode);
                writer.setMargin(opts.margin);
                writer.setEccLevel(opts.eccLevel);
                writer.setEncoding(CharacterSet::UTF8);

                // 编码为 BitMatrix
                BitMatrix matrix = writer.encode(opts.text, opts.width, opts.height);

                int w = matrix.width();
                int h = matrix.height();

                // 转换为 BGRA32 ImageBuffer
                out.width = w;
                out.height = h;
                out.format = PixelFormat::BGRA32;
                out.stride = w * 4;
                out.data.resize(static_cast<size_t>(out.stride) * h);

                // 解析前景色和背景色
                uint8_t fgR = opts.fgColor & 0xFF;
                uint8_t fgG = (opts.fgColor >> 8) & 0xFF;
                uint8_t fgB = (opts.fgColor >> 16) & 0xFF;
                uint8_t bgR = opts.bgColor & 0xFF;
                uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
                uint8_t bgB = (opts.bgColor >> 16) & 0xFF;

                for (int y = 0; y < h; ++y) {
                    for (int x = 0; x < w; ++x) {
                        size_t idx = static_cast<size_t>(y * out.stride + x * 4);
                        if (matrix.get(x, y)) {
                            // 前景色 (黑色模块)
                            out.data[idx] = fgB;
                            out.data[idx + 1] = fgG;
                            out.data[idx + 2] = fgR;
                            out.data[idx + 3] = 0xFF;
                        } else {
                            // 背景色 (白色模块)
                            out.data[idx] = bgB;
                            out.data[idx + 1] = bgG;
                            out.data[idx + 2] = bgR;
                            out.data[idx + 3] = 0xFF;
                        }
                    }
                }

                return true;
            } catch (const std::exception& e) {
                (void)e;
                return false;
            }
        }

        bool generateToFile(const QRCodeGenerateOptions& opts, const std::string& path) override {
            ImageBuffer img;
            if (!generate(opts, img)) return false;

#ifdef _WIN32
            auto codec = createWinCodec();
#else
            auto codec = createCrossCodec();
#endif
            if (!codec) return false;
            return codec->saveToFile(img, path);
        }
    };

    return std::make_unique<QRCodeGeneratorImpl>();
}

} // namespace imgproc
