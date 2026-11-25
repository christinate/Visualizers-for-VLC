// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace visualizers::raster_detail {
std::string normalizeText(std::string_view input){
    if(input.size()>8192)throw std::invalid_argument("Text exceeds the 8192-byte UTF-8 limit");std::string text(input);
    for(size_t i=0;i<text.size();){const uint8_t first=uint8_t(text[i]);uint32_t code=0;size_t length=1;uint32_t minimum=0;
        if(first<0x80)code=first;else if(first>=0xc2&&first<=0xdf){length=2;code=first&0x1f;minimum=0x80;}else if(first>=0xe0&&first<=0xef){length=3;code=first&0xf;minimum=0x800;}else if(first>=0xf0&&first<=0xf4){length=4;code=first&7;minimum=0x10000;}else throw std::invalid_argument("Text is not valid UTF-8");
        if(i+length>text.size())throw std::invalid_argument("Truncated UTF-8 text");for(size_t j=1;j<length;++j){const uint8_t b=uint8_t(text[i+j]);if((b&0xc0)!=0x80)throw std::invalid_argument("Text is not valid UTF-8");code=(code<<6)|(b&0x3f);}
        if(!code||code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))throw std::invalid_argument("Text contains an invalid scalar or NUL");
        if(length==1&&(code==9||code==10||code==12||code==13))text[i]=' ';i+=length;
    }return text;
}
std::string fontFamily(std::string_view name){if(name.empty()||name.size()>256)throw std::invalid_argument("Font family must contain 1..256 UTF-8 bytes");auto family=normalizeText(name);if(family=="sans-serif")return "Arial";if(family=="serif")return "Times New Roman";if(family=="monospace")return "Courier New";return family;}
void validate(const TextRasterOptions& p){
    if(!std::isfinite(p.fontPixels)||p.fontPixels<0.5||p.fontPixels>1024||!std::isfinite(p.paddingPixels)||p.paddingPixels<0||p.paddingPixels>256||!p.supersample||p.supersample>8||p.weight<100||p.weight>900)throw std::invalid_argument("Invalid native font size/padding/supersample/weight");fontFamily(p.family);
}
RasterizedText allocate(const TextRasterOptions& p,const TextRasterMetrics& raw){
    const auto rounded=[](double v){if(!std::isfinite(v)||std::abs(v)>maximumDimension*2)throw std::runtime_error("Text bounds exceed the rendering size limit");return std::ceil(v);};
    RasterizedText result;result.metrics=raw;auto& m=result.metrics;m.left=rounded(m.left);m.right=rounded(m.right);m.ascent=rounded(m.ascent);m.descent=rounded(m.descent);m.padding=uint32_t(std::max(1.,std::ceil(p.paddingPixels*p.supersample)));m.supersample=p.supersample;
    const double w=std::max(1.,m.left+m.right)+m.padding*2+16,h=std::max(1.,m.ascent+m.descent)+m.padding*2;
    if(w>maximumDimension||h>maximumDimension||w*h>maximumPixels)throw std::runtime_error("Text mask exceeds the rendering size limit");result.image.width=uint32_t(w);result.image.height=uint32_t(h);result.image.rgba.resize(size_t(w)*size_t(h)*4,0);return result;
}
void whiteAlpha(RgbaImage& image,const uint8_t* alpha,uint32_t stride){if(!alpha||stride<image.width)throw std::invalid_argument("Invalid text alpha source");for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x){const uint8_t a=alpha[size_t(y)*stride+x];const size_t n=(size_t(y)*image.width+x)*4;image.rgba[n]=image.rgba[n+1]=image.rgba[n+2]=a?255:0;image.rgba[n+3]=a;}}
}
