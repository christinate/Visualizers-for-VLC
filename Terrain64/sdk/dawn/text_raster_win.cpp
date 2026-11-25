// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include <windows.h>
#include <dwrite.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace visualizers {namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* operation){if(FAILED(hr))throw std::runtime_error(operation);}
struct ComSession {bool initialized=false;ComSession(){const HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(hr!=RPC_E_CHANGED_MODE)check(hr,"Cannot initialize native text COM session");initialized=SUCCEEDED(hr);}~ComSession(){if(initialized)CoUninitialize();}};
std::wstring wide(const std::string& text){const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(n<=0)throw std::invalid_argument("Font text is not valid UTF-8");std::wstring value(n,0);if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),value.data(),n)!=n)throw std::runtime_error("Cannot convert native font text");return value;}
}
RasterizedText rasterizeText(std::string_view utf8,const TextRasterOptions& options){
    raster_detail::validate(options);const auto text=raster_detail::normalizeText(utf8);if(text.empty())return {};const auto family=raster_detail::fontFamily(options.family);const auto characters=wide(text),name=wide(family);ComSession session;
    ComPtr<IDWriteFactory> fonts;check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(fonts.GetAddressOf())),"Cannot create DirectWrite factory");
    ComPtr<IDWriteTextFormat> format;check(fonts->CreateTextFormat(name.c_str(),nullptr,DWRITE_FONT_WEIGHT(options.weight),DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,float(options.fontPixels*options.supersample),L"",&format),"Cannot create native text format");check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP),"Cannot configure single-line text");
    ComPtr<IDWriteTextLayout> layout;check(fonts->CreateTextLayout(characters.data(),UINT32(characters.size()),format.Get(),16384,16384,&layout),"Cannot shape native text");DWRITE_TEXT_METRICS tm{};check(layout->GetMetrics(&tm),"Cannot measure native text");check(layout->SetMaxWidth(std::max(1.f,tm.widthIncludingTrailingWhitespace)),"Cannot fit native text width");check(layout->SetMaxHeight(std::max(1.f,tm.height)),"Cannot fit native text height");check(layout->GetMetrics(&tm),"Cannot remeasure native text");
    DWRITE_OVERHANG_METRICS over{};check(layout->GetOverhangMetrics(&over),"Cannot measure native glyph bounds");UINT32 count=0;layout->GetLineMetrics(nullptr,0,&count);if(count!=1)throw std::runtime_error("Native text did not remain a single line");DWRITE_LINE_METRICS line{};check(layout->GetLineMetrics(&line,1,&count),"Cannot measure native baseline");TextRasterMetrics metrics{over.left,double(tm.layoutWidth)+over.right,double(line.baseline)+over.top,double(tm.layoutHeight)+over.bottom-line.baseline,tm.widthIncludingTrailingWhitespace,0,options.supersample,family};
    if(std::all_of(text.begin(),text.end(),[](char c){return c==' ';}))metrics.left=metrics.right=metrics.ascent=metrics.descent=0;
    auto result=raster_detail::allocate(options,metrics);ComPtr<IWICImagingFactory> imaging;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging)),"Cannot create native bitmap factory");ComPtr<IWICBitmap> bitmap;check(imaging->CreateBitmap(result.image.width,result.image.height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap),"Cannot allocate native text bitmap");
    ComPtr<ID2D1Factory> drawing;check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawing.GetAddressOf()),"Cannot create native text drawing factory");const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);ComPtr<ID2D1RenderTarget> target;check(drawing->CreateWicBitmapRenderTarget(bitmap.Get(),properties,&target),"Cannot create native text drawing target");target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);ComPtr<ID2D1SolidColorBrush> brush;check(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),&brush),"Cannot create native text brush");
    target->BeginDraw();target->Clear(D2D1::ColorF(0,0,0,0));target->DrawTextLayout(D2D1::Point2F(float(result.metrics.padding+result.metrics.left),float(result.metrics.padding+result.metrics.ascent-line.baseline)),layout.Get(),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);check(target->EndDraw(),"Native glyph rendering failed");
    ComPtr<IWICBitmapLock> locked;check(bitmap->Lock(nullptr,WICBitmapLockRead,&locked),"Cannot read native text bitmap");UINT stride=0,bytes=0;BYTE* data=nullptr;check(locked->GetStride(&stride),"Cannot read native text stride");check(locked->GetDataPointer(&bytes,&data),"Cannot access native text pixels");if(!data||stride<result.image.width*4||uint64_t(stride)*(result.image.height-1)+result.image.width*4>bytes)throw std::runtime_error("Invalid native text bitmap layout");
    for(uint32_t y=0;y<result.image.height;++y)for(uint32_t x=0;x<result.image.width;++x){const uint8_t a=data[size_t(y)*stride+x*4+3];const size_t n=(size_t(y)*result.image.width+x)*4;result.image.rgba[n]=result.image.rgba[n+1]=result.image.rgba[n+2]=a?255:0;result.image.rgba[n+3]=a;}return result;
}
}
