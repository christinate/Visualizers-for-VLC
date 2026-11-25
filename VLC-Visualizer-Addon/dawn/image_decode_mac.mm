// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_decode.hpp"
#include <ImageIO/ImageIO.h>
#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <stdexcept>
namespace visualizers {namespace {
template<class T>struct Owned {T value=nullptr;explicit Owned(T v):value(v){if(!value)throw std::runtime_error("Cannot create native image resource");}~Owned(){if(value)CFRelease(value);}Owned(const Owned&)=delete;operator T()const{return value;}};
uint32_t dimension(CFDictionaryRef properties,CFStringRef key){const auto value=CFDictionaryGetValue(properties,key);int64_t n=0;if(!value||CFGetTypeID(value)!=CFNumberGetTypeID()||!CFNumberGetValue(static_cast<CFNumberRef>(value),kCFNumberSInt64Type,&n)||n<=0||n>32768)throw std::runtime_error("Invalid image dimensions");return uint32_t(n);}
}
DecodedImage decodeImage(const std::filesystem::path& file,uint32_t size,std::stop_token stop){
    image_detail::validateFile(file,size,stop);image_detail::gifFirstFrame(file,stop);const auto path=file.string();Owned<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(path.data()),CFIndex(path.size()),false));
    const void* readKeys[]={kCGImageSourceShouldCache};const void* readValues[]={kCFBooleanFalse};Owned<CFDictionaryRef> options(CFDictionaryCreate(nullptr,readKeys,readValues,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));Owned<CGImageSourceRef> source(CGImageSourceCreateWithURL(url,options));
    if(!CGImageSourceGetCount(source))throw std::runtime_error("Image contains no frames");Owned<CFDictionaryRef> properties(CGImageSourceCopyPropertiesAtIndex(source,0,nullptr));DecodedImage result;result.sourceWidth=dimension(properties,kCGImagePropertyPixelWidth);result.sourceHeight=dimension(properties,kCGImagePropertyPixelHeight);image_detail::dimensions(result.sourceWidth,result.sourceHeight,size);result.backend="ImageIO/CoreGraphics";result.sourceProfileReported=CFDictionaryContainsKey(properties,kCGImagePropertyProfileName);
    const int maximum=int(std::min(size,std::max(result.sourceWidth,result.sourceHeight)));Owned<CFNumberRef> limit(CFNumberCreate(nullptr,kCFNumberIntType,&maximum));const void* keys[]={kCGImageSourceCreateThumbnailFromImageAlways,kCGImageSourceCreateThumbnailWithTransform,kCGImageSourceThumbnailMaxPixelSize,kCGImageSourceShouldCacheImmediately};const void* values[]={kCFBooleanTrue,kCFBooleanTrue,limit.value,kCFBooleanTrue};Owned<CFDictionaryRef> thumbnailOptions(CFDictionaryCreate(nullptr,keys,values,4,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));Owned<CGImageRef> thumbnail(CGImageSourceCreateThumbnailAtIndex(source,0,thumbnailOptions));image_detail::cancelled(stop);
    const auto w=CGImageGetWidth(thumbnail),h=CGImageGetHeight(thumbnail);if(!w||!h||w>size||h>size)throw std::runtime_error("Native thumbnail dimensions exceed the sample limit");result.image.width=uint32_t(w);result.image.height=uint32_t(h);result.image.rgba.resize(w*h*4,0);Owned<CGColorSpaceRef> srgb(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    const CGBitmapInfo layout=CGBitmapInfo(kCGImageAlphaPremultipliedLast)|CGBitmapInfo(kCGBitmapByteOrder32Big);Owned<CGContextRef> context(CGBitmapContextCreate(result.image.rgba.data(),w,h,8,w*4,srgb,layout));CGContextSetBlendMode(context,kCGBlendModeCopy);CGContextSetInterpolationQuality(context,kCGInterpolationHigh);CGContextDrawImage(context,CGRectMake(0,0,CGFloat(w),CGFloat(h)),thumbnail);
    image_detail::unpremultiply(result.image);image_detail::cancelled(stop);return result;
}
}
