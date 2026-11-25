// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <array>
#include <stdexcept>
namespace visualizers {namespace {
template<class T>struct Owned {T value=nullptr;explicit Owned(T v):value(v){if(!value)throw std::runtime_error("Cannot create native font resource");}~Owned(){if(value)CFRelease(value);}Owned(const Owned&)=delete;operator T()const{return value;}};
}
RasterizedText rasterizeText(std::string_view utf8,const TextRasterOptions& options){
    raster_detail::validate(options);const auto text=raster_detail::normalizeText(utf8);if(text.empty())return {};const auto family=raster_detail::fontFamily(options.family);
    Owned<CFStringRef> string(CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8*>(text.data()),CFIndex(text.size()),kCFStringEncodingUTF8,false));Owned<CFStringRef> name(CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8*>(family.data()),CFIndex(family.size()),kCFStringEncodingUTF8,false));
    const double css[]={100,200,300,400,500,600,700,800,900},weights[]={-.8,-.6,-.4,0,.23,.3,.4,.56,.62};const unsigned index=std::min(7u,unsigned((options.weight-100)/100));const double fraction=(double(options.weight)-css[index])/100.,weight=weights[index]+(weights[index+1]-weights[index])*fraction;Owned<CFNumberRef> value(CFNumberCreate(nullptr,kCFNumberDoubleType,&weight));
    const void* traitKeys[]={kCTFontWeightTrait};const void* traitValues[]={value.value};Owned<CFDictionaryRef> traits(CFDictionaryCreate(nullptr,traitKeys,traitValues,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));const void* fontKeys[]={kCTFontFamilyNameAttribute,kCTFontTraitsAttribute};const void* fontValues[]={name.value,traits.value};Owned<CFDictionaryRef> attributes(CFDictionaryCreate(nullptr,fontKeys,fontValues,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));Owned<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(attributes));Owned<CTFontRef> font(CTFontCreateWithFontDescriptor(descriptor,options.fontPixels*options.supersample,nullptr));
    const void* keys[]={kCTFontAttributeName,kCTForegroundColorFromContextAttributeName};const void* values[]={font.value,kCFBooleanTrue};Owned<CFDictionaryRef> style(CFDictionaryCreate(nullptr,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));Owned<CFAttributedStringRef> attributed(CFAttributedStringCreate(nullptr,string,style));Owned<CTLineRef> line(CTLineCreateWithAttributedString(attributed));
    uint8_t scratch=0;Owned<CGContextRef> measure(CGBitmapContextCreate(&scratch,1,1,8,1,nullptr,kCGImageAlphaOnly));CGContextSetTextMatrix(measure,CGAffineTransformIdentity);const auto b=CTLineGetImageBounds(line,measure);TextRasterMetrics metrics{-b.origin.x,b.origin.x+b.size.width,b.origin.y+b.size.height,-b.origin.y,CTLineGetTypographicBounds(line,nullptr,nullptr,nullptr),0,options.supersample,family};if(CGRectIsNull(b)||std::all_of(text.begin(),text.end(),[](char c){return c==' ';}))metrics.left=metrics.right=metrics.ascent=metrics.descent=0;
    auto result=raster_detail::allocate(options,metrics);std::vector<uint8_t> alpha(size_t(result.image.width)*result.image.height,0);Owned<CGContextRef> context(CGBitmapContextCreate(alpha.data(),result.image.width,result.image.height,8,result.image.width,nullptr,kCGImageAlphaOnly));CGContextSetShouldAntialias(context,true);CGContextSetShouldSmoothFonts(context,false);CGContextSetGrayFillColor(context,1,1);CGContextSetTextMatrix(context,CGAffineTransformIdentity);CGContextSetTextPosition(context,result.metrics.padding+result.metrics.left,result.image.height-(result.metrics.padding+result.metrics.ascent));CTLineDraw(line,context);raster_detail::whiteAlpha(result.image,alpha.data(),result.image.width);return result;
}
}
