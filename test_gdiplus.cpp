// 直接测试 GDI+ JPEG 保存
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

int getEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
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

int main() {
    // 初始化 GDI+
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartupOutput output;
    ULONG_PTR token;
    Gdiplus::GdiplusStartup(&token, &input, &output);
    
    std::cout << "GDI+ initialized" << std::endl;
    
    // 创建一个简单的位图 (100x50, 白色背景)
    int width = 100, height = 50;
    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
    
    // 填充白色
    Gdiplus::Graphics g(&bitmap);
    g.Clear(Gdiplus::Color(255, 255, 255, 255));
    
    // 画一个黑色矩形
    Gdiplus::SolidBrush blackBrush(Gdiplus::Color(255, 0, 0, 0));
    g.FillRectangle(&blackBrush, 30, 20, 40, 10);
    
    std::cout << "Bitmap created: " << bitmap.GetWidth() << "x" << bitmap.GetHeight() << std::endl;
    
    // 获取 JPEG encoder
    CLSID clsid;
    int result = getEncoderClsid(L"image/jpeg", &clsid);
    if (result < 0) {
        std::cerr << "JPEG encoder not found!" << std::endl;
        Gdiplus::GdiplusShutdown(token);
        return 1;
    }
    std::cout << "JPEG encoder found, index=" << result << std::endl;
    
    // 设置质量
    ULONG quality = 90;
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;
    
    // 保存
    const WCHAR* path = L"test_direct.jpg";
    Gdiplus::Status status = bitmap.Save(path, &clsid, &encoderParams);
    
    if (status == Gdiplus::Ok) {
        std::cout << "JPEG saved successfully to test_direct.jpg" << std::endl;
    } else {
        std::cerr << "Save failed, status=" << status << std::endl;
    }
    
    Gdiplus::GdiplusShutdown(token);
    return (status == Gdiplus::Ok) ? 0 : 1;
}
