// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_decode.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace visualizers {namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* operation){if(FAILED(hr))throw std::runtime_error(operation);}
struct ComSession {bool initialized=false;ComSession(){const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(hr!=RPC_E_CHANGED_MODE)check(hr,"Cannot initialize native image COM session");initialized=SUCCEEDED(hr);}~ComSession(){if(initialized)CoUninitialize();}};
unsigned orientation(IWICBitmapFrameDecode* frame){ComPtr<IWICMetadataQueryReader> metadata;if(FAILED(frame->GetMetadataQueryReader(&metadata)))return 1;
    for(auto name:{L"/app1/ifd/{ushort=274}",L"/ifd/{ushort=274}",L"/eXIf/ifd/{ushort=274}"}){PROPVARIANT value{};const auto hr=metadata->GetMetadataByName(name,&value);unsigned code=1;if(SUCCEEDED(hr)&&value.vt==VT_UI2&&value.uiVal>=1&&value.uiVal<=8)code=value.uiVal;PropVariantClear(&value);if(SUCCEEDED(hr))return code;}return 1;
}
ComPtr<IWICFormatConverter> convert(IWICImagingFactory* factory,IWICBitmapSource* source,REFWICPixelFormatGUID format){ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter),"Cannot create native image format converter");check(converter->Initialize(source,format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Cannot convert image pixel layout");return converter;}
UINT metadataNumber(IWICMetadataQueryReader* reader,const wchar_t* name){PROPVARIANT value{};check(reader->GetMetadataByName(name,&value),"Cannot read GIF canvas metadata");const auto type=value.vt;const UINT n=type==VT_UI2?value.uiVal:0;PropVariantClear(&value);if(type!=VT_UI2)throw std::runtime_error("Invalid GIF canvas metadata type");return n;}
// Virtual transparent canvas. WIC exposes a GIF frame's own rectangle; provide
// the logical screen without allocating a full-size intermediate bitmap.
class GifCanvas final:public IWICBitmapSource {
    std::atomic<ULONG> refs_{1};ComPtr<IWICBitmapSource> frame_;UINT width_,height_,left_,top_,frameWidth_=0,frameHeight_=0;std::stop_token stop_;
public:
    GifCanvas(IWICBitmapSource* frame,UINT w,UINT h,UINT left,UINT top,std::stop_token stop):frame_(frame),width_(w),height_(h),left_(left),top_(top),stop_(stop){check(frame_->GetSize(&frameWidth_,&frameHeight_),"Cannot measure GIF frame");}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value)override{if(!value)return E_POINTER;*value=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IWICBitmapSource)){*value=static_cast<IWICBitmapSource*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetSize(UINT* w,UINT* h)override{if(!w||!h)return E_POINTER;*w=width_;*h=height_;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID* value)override{if(!value)return E_POINTER;*value=GUID_WICPixelFormat32bppPBGRA;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetResolution(double* x,double* y)override{if(!x||!y)return E_POINTER;*x=*y=96;return S_OK;}
    HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette*)override{return WINCODEC_ERR_PALETTEUNAVAILABLE;}
    HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect* requested,UINT stride,UINT bytes,BYTE* data)override{
        if(!data)return E_POINTER;const WICRect r=requested?*requested:WICRect{0,0,int(width_),int(height_)};
        if(r.X<0||r.Y<0||r.Width<=0||r.Height<=0||uint64_t(r.X)+r.Width>width_||uint64_t(r.Y)+r.Height>height_||stride<uint64_t(r.Width)*4||uint64_t(stride)*(r.Height-1)+uint64_t(r.Width)*4>bytes)return E_INVALIDARG;
        for(int y=0;y<r.Height;++y){if((y&63)==0&&stop_.stop_requested())return HRESULT_FROM_WIN32(ERROR_CANCELLED);std::memset(data+size_t(y)*stride,0,size_t(r.Width)*4);}
        const UINT x0=std::max(UINT(r.X),left_),y0=std::max(UINT(r.Y),top_),x1=std::min(UINT(r.X+r.Width),left_+frameWidth_),y1=std::min(UINT(r.Y+r.Height),top_+frameHeight_);if(x0>=x1||y0>=y1)return S_OK;
        const WICRect inside{int(x0-left_),int(y0-top_),int(x1-x0),int(y1-y0)};const auto offset=UINT(uint64_t(y0-r.Y)*stride+uint64_t(x0-r.X)*4);return frame_->CopyPixels(&inside,stride,bytes-offset,data+offset);
    }
};
}
DecodedImage decodeImage(const std::filesystem::path& file,uint32_t size,std::stop_token stop){
    image_detail::validateFile(file,size,stop);image_detail::gifFirstFrame(file,stop);ComSession com;ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Cannot create native image decoder factory");ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder),"Unsupported or corrupt image");ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame),"Image contains no decodable first frame");
    DecodedImage result;result.backend="Windows Imaging Component";check(frame->GetSize(&result.sourceWidth,&result.sourceHeight),"Cannot read image dimensions");image_detail::dimensions(result.sourceWidth,result.sourceHeight,size);ComPtr<IWICBitmapSource> pixels=frame;
    UINT count=0;std::vector<ComPtr<IWICColorContext>> profiles;ComPtr<IWICColorContext> selected;const auto contexts=frame->GetColorContexts(0,nullptr,&count);
    if(SUCCEEDED(contexts)&&count){if(count>8)throw std::runtime_error("Image contains too many color profiles");profiles.resize(count);std::vector<IWICColorContext*> pointers(count);for(UINT n=0;n<count;++n){check(factory->CreateColorContext(&profiles[n]),"Cannot create source image color context");pointers[n]=profiles[n].Get();}UINT actual=0;check(frame->GetColorContexts(count,pointers.data(),&actual),"Cannot read embedded image color profiles");if(actual>count)throw std::runtime_error("Image color-profile count changed");
        for(auto wanted:{WICColorContextProfile,WICColorContextExifColorSpace}){for(UINT n=0;n<actual;++n){WICColorContextType type{};check(profiles[n]->GetType(&type),"Cannot read image profile type");if(type==wanted){if(type==WICColorContextExifColorSpace){UINT exif=0;check(profiles[n]->GetExifColorSpace(&exif),"Cannot read EXIF color space");if(exif!=1)continue;}selected=profiles[n];break;}}if(selected)break;}
    }
    if(selected){ComPtr<IWICColorContext> destination;check(factory->CreateColorContext(&destination),"Cannot create destination image color context");check(destination->InitializeFromExifColorSpace(1),"Cannot initialize sRGB image profile");ComPtr<IWICColorTransform> transform;check(factory->CreateColorTransformer(&transform),"Cannot create image color transform");check(transform->Initialize(pixels.Get(),selected.Get(),destination.Get(),GUID_WICPixelFormat32bppRGBA),"Cannot convert embedded image profile to sRGB");pixels=transform;result.sourceProfileReported=true;}
    pixels=convert(factory.Get(),pixels.Get(),GUID_WICPixelFormat32bppPBGRA);GUID container{};check(decoder->GetContainerFormat(&container),"Cannot read image container format");
    if(container==GUID_ContainerFormatGif){ComPtr<IWICMetadataQueryReader> global,local;check(decoder->GetMetadataQueryReader(&global),"Cannot read GIF logical screen");check(frame->GetMetadataQueryReader(&local),"Cannot read GIF frame offset");const auto w=metadataNumber(global.Get(),L"/logscrdesc/Width"),h=metadataNumber(global.Get(),L"/logscrdesc/Height"),left=metadataNumber(local.Get(),L"/imgdesc/Left"),top=metadataNumber(local.Get(),L"/imgdesc/Top");image_detail::dimensions(w,h,size);const bool canvasNeeded=left||top||w!=result.sourceWidth||h!=result.sourceHeight;result.sourceWidth=w;result.sourceHeight=h;if(canvasNeeded){ComPtr<IWICBitmapSource> canvas;canvas.Attach(new GifCanvas(pixels.Get(),w,h,left,top,stop));pixels=canvas;}}
    const auto exif=orientation(frame.Get());if(exif!=1){constexpr std::array<WICBitmapTransformOptions,8> transforms={WICBitmapTransformRotate0,WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate180,WICBitmapTransformFlipVertical,WICBitmapTransformOptions(WICBitmapTransformRotate270|WICBitmapTransformFlipHorizontal),WICBitmapTransformRotate90,WICBitmapTransformOptions(WICBitmapTransformRotate90|WICBitmapTransformFlipHorizontal),WICBitmapTransformRotate270};ComPtr<IWICBitmapFlipRotator> rotated;check(factory->CreateBitmapFlipRotator(&rotated),"Cannot create EXIF image orientation transform");check(rotated->Initialize(pixels.Get(),transforms[exif-1]),"Cannot orient image");pixels=rotated;}
    UINT width=0,height=0;check(pixels->GetSize(&width,&height),"Cannot read oriented image size");const auto desired=image_detail::dimensions(width,height,size);if(width!=desired[0]||height!=desired[1]){ComPtr<IWICBitmapScaler> scaled;check(factory->CreateBitmapScaler(&scaled),"Cannot create native image downsampler");check(scaled->Initialize(pixels.Get(),desired[0],desired[1],WICBitmapInterpolationModeFant),"Cannot downsample image");pixels=scaled;}
    pixels=convert(factory.Get(),pixels.Get(),GUID_WICPixelFormat32bppRGBA);result.image.width=desired[0];result.image.height=desired[1];result.image.rgba.resize(size_t(desired[0])*desired[1]*4);image_detail::cancelled(stop);check(pixels->CopyPixels(nullptr,desired[0]*4,UINT(result.image.rgba.size()),result.image.rgba.data()),"Cannot read decoded sRGB image pixels");image_detail::cancelled(stop);return result;
}
}
