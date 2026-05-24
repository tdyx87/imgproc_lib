#pragma once
#include "imgproc/types.hpp"

#ifdef _WIN32
namespace imgproc {

class WinImageCodec : public IImageCodec {
public:
    WinImageCodec();
    ~WinImageCodec() override;

    bool loadFromFile(const std::string& path, ImageBuffer& out) override;
    bool loadFromMemory(const uint8_t* data, size_t size, ImageType type, ImageBuffer& out) override;
    bool saveToJpegFile(const ImageBuffer& img, const std::string& path, int quality = 85) override;
    bool saveToJpegMemory(const ImageBuffer& img, std::vector<uint8_t>& out, int quality = 85) override;
    bool saveToBmpFile(const ImageBuffer& img, const std::string& path) override;
    bool saveToBmpMemory(const ImageBuffer& img, std::vector<uint8_t>& out) override;
    bool saveToPngFile(const ImageBuffer& img, const std::string& path) override;
    bool saveToPngMemory(const ImageBuffer& img, std::vector<uint8_t>& out) override;
    bool saveToFile(const ImageBuffer& img, const std::string& path, int jpegQuality = 85) override;

private:
    bool saveToJpeg(const ImageBuffer& img, const std::string& path, std::vector<uint8_t>* memOut, int quality);
    bool saveToBmp(const ImageBuffer& img, const std::string& path, std::vector<uint8_t>* memOut);
};

class WinTextRenderer : public ITextRenderer {
public:
    WinTextRenderer();
    ~WinTextRenderer() override;

    bool renderToFile(const TextRenderOptions& opts, const std::string& path) override;
    bool renderToMemory(const TextRenderOptions& opts, ImageBuffer& out) override;
};

} // namespace imgproc
#endif
