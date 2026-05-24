#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

class CrossImageCodec : public IImageCodec {
public:
    CrossImageCodec();
    ~CrossImageCodec() override;

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
    bool loadPng(const uint8_t* data, size_t size, ImageBuffer& out);
    bool loadJpeg(const uint8_t* data, size_t size, ImageBuffer& out);
    bool loadBmp(const uint8_t* data, size_t size, ImageBuffer& out);
};

class CrossTextRenderer : public ITextRenderer {
public:
    CrossTextRenderer();
    ~CrossTextRenderer() override;

    bool renderToFile(const TextRenderOptions& opts, const std::string& path) override;
    bool renderToMemory(const TextRenderOptions& opts, ImageBuffer& out) override;
};

} // namespace imgproc
