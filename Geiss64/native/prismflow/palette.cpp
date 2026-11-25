// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "palette.hpp"
#include "../../sdk/runtime/include/decimal_parse.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace prismflow {
namespace {
#include "palettes.inc"
void validate(std::span<const float> rgba){
    if(rgba.empty()||rgba.size()%4||rgba.size()>4096*4)throw std::invalid_argument("Palette must contain 1 to 4096 RGBA colors");
    for(size_t n=0;n<rgba.size();++n)if(!std::isfinite(rgba[n])||(n%4!=3&&(rgba[n]<0||rgba[n]>65504)))throw std::invalid_argument("Palette RGB values must be finite and within float16 range");
}
Color sample(std::span<const float> rgba,double t){
    const auto count=rgba.size()/4;t=std::max(0.,std::min(1.,t));const double fx=t*(count-1);
    const auto first=size_t(std::floor(fx)),second=std::min(first+1,count-1);const double dx=fx-first;Color result{};
    for(unsigned n=0;n<3;++n)result[n]=double(rgba[first*4+n])*(1-dx)+dx*double(rgba[second*4+n]);return result;
}
double draw(const RandomFunction& random){if(!random)throw std::invalid_argument("A palette random source is required");const auto v=random();if(!std::isfinite(v)||v<0||v>=1)throw std::invalid_argument("Palette random source must return [0,1)");return v;}
double lerp(double a,double b,double t){return a*(1-t)+b*t;}
// JS toFixed(4) rounds the exact positive binary value, including halfway
// cases. Multiplying a double by 10000 and rounding can double-round instead.
std::string fixed4(double value){
    const auto bits=std::bit_cast<uint64_t>(value);const unsigned exponent=unsigned((bits>>52)&2047);
    const uint64_t mantissa=(bits&0xfffffffffffffull)|(exponent?0x10000000000000ull:0);
    const int shift=(exponent?int(exponent)-1023-52:-1074)+4;const uint64_t product=mantissa*625;
    uint64_t rounded=0;
    if(shift>=0)throw std::out_of_range("Palette serialization range exceeded");
    const unsigned right=unsigned(-shift);
    if(right<64){rounded=product>>right;const uint64_t remainder=product&((uint64_t(1)<<right)-1);
        if(remainder>=(uint64_t(1)<<(right-1)))++rounded;}
    const auto fraction=std::to_string(rounded%10000);return std::to_string(rounded/10000)+'.'+std::string(4-fraction.size(),'0')+fraction;
}
void validateTone(double t,const Color& rgb,double maximum){
    if(!std::isfinite(t)||!std::isfinite(maximum)||maximum<0)throw std::invalid_argument("Invalid palette tone-map parameter");
    for(double v:rgb)if(!std::isfinite(v)||v<0||v>65504)throw std::invalid_argument("Tone-map RGB values must be finite and within float16 range");
}
}
Palette deserializePalette(std::string_view text){
    if(text.size()>1024*1024)throw std::invalid_argument("Palette text exceeds 1 MiB");
    size_t offset=0;auto token=[&](){while(offset<text.size()&&(text[offset]==' '||text[offset]=='\t'||text[offset]=='\r'||text[offset]=='\n'))++offset;
        const size_t start=offset;while(offset<text.size()&&text[offset]!=' '&&text[offset]!='\t'&&text[offset]!='\r'&&text[offset]!='\n')++offset;return text.substr(start,offset-start);};
    const auto countText=token();uint32_t count=0;const auto parsed=std::from_chars(countText.data(),countText.data()+countText.size(),count);
    if(parsed.ec!=std::errc{}||parsed.ptr!=countText.data()+countText.size()||!count||count>4096)throw std::invalid_argument("Invalid palette color count");
    Palette result(count*4,1.f);for(uint32_t n=0;n<count;++n)for(unsigned channel=0;channel<3;++channel){const auto value=token();double number=0;
        if(!suite::finiteDecimal(value,number)||number<0||number>65504)throw std::invalid_argument("Invalid palette RGB component");
        result[n*4+channel]=float(number);
    }
    if(!token().empty())throw std::invalid_argument("Unexpected trailing palette data");return result;
}
std::string serializePalette(std::span<const float> rgba){validate(rgba);std::string result="8  ";
    for(unsigned n=0;n<8;++n){const auto color=sample(rgba,double(n)/7);result+=fixed4(color[0])+' '+fixed4(color[1])+' '+fixed4(color[2])+"  ";}return result;
}
Color samplePalette(std::span<const float> rgba,double t){validate(rgba);if(!std::isfinite(t))throw std::invalid_argument("Palette sample coordinate must be finite");return sample(rgba,t);}
ExpandedPalette expandPalette(std::span<const float> rgba){validate(rgba);Palette zeroed(rgba.begin(),rgba.end());zeroed[0]=zeroed[1]=zeroed[2]=0;
    ExpandedPalette result{};for(unsigned n=0;n<256;++n){const auto color=sample(zeroed,n*(1./255));for(unsigned c=0;c<3;++c)result[n*4+c]=float(color[c]);result[n*4+3]=1;}return result;
}
Color companionColor(std::span<const float> rgba,const RandomFunction& random,std::stop_token stop){
    validate(rgba);if(stop.stop_requested())throw std::runtime_error("Prism Flow palette generation cancelled");
    if(double(rgba[0])+double(rgba[1])+double(rgba[2])!=0)return {rgba[0],rgba[1],rgba[2]};
    const size_t count=rgba.size()/4,index=std::min(count-1,size_t(std::floor((count-1)*.7+.5)));
    const double maximum=std::max({.01,double(rgba[index*4]),double(rgba[index*4+1]),double(rgba[index*4+2])});
    FloatColor foreground{};for(unsigned c=0;c<3;++c)foreground[c]=float(double(rgba[index*4+c])*(1./maximum));
    auto color=pickBackgroundColor({foreground[0],foreground[1],foreground[2]},{},random,stop);
    const double luma=color[0]*.3+color[1]*.59+color[2]*.11;if(luma>.62)for(auto& c:color)c*=.62/luma;return color;
}
FloatColor hdrToSdr(double t,const Color& rgb,double maximum,bool experiment){
    (void)experiment;validateTone(t,rgb,maximum);maximum=std::max(maximum,1.);FloatColor color={float(rgb[0]),float(rgb[1]),float(rgb[2])};
    const double scale=std::pow(1./maximum,.57);for(auto& c:color)c=float(double(c)*scale);maximum*=scale;
    // Extreme valid half-float palettes can extrapolate past white in the
    // reference, creating negative luminance and then NaNs in pow(luma,.87).
    const double desaturate=std::min(1.,std::max(0.,t+maximum-2.)*.25);for(auto& c:color)c=float(lerp(c,1.,desaturate));
    const double luma=(double(color[0])+double(color[1])+double(color[2])+5*std::max(double(color[0]),std::max(double(color[1]),double(color[2]))))/8.;
    // Confirmed reference fault: black produces 0/0 here and three NaNs.
    // Keeping black finite is required before a native float texture upload.
    if(luma==0)return color;
    const double scale2=std::pow(luma,.87)/luma;for(auto& c:color)c=float(double(c)*scale2);
    const double maximumRgb=std::max(double(color[0]),std::max(double(color[1]),double(color[2])));
    const double shadow=std::max(0.,std::min(1.,(maximumRgb-.48)/(0.-.48)));
    for(auto& c:color){c=float(double(c)*(1.-shadow*.4));c=float(std::min(1.,double(c)));}return color;
}
FloatColor reduceHdrHeadroom(double headroom,double t,const Color& rgb,double maximum,bool experiment){
    if(!std::isfinite(headroom)||headroom<1||headroom>65504)throw std::invalid_argument("HDR headroom must be within [1,65504]");
    validateTone(t,rgb,maximum);Color reduced{};for(unsigned c=0;c<3;++c)reduced[c]=rgb[c]*(1./headroom);
    auto color=hdrToSdr(t,reduced,maximum*(1./headroom),experiment);for(auto& c:color)c=float(double(c)*headroom);return color;
}
PaletteCatalog::PaletteCatalog(){text_.reserve(savedPaletteText.size());for(auto value:savedPaletteText)text_.emplace_back(value);}
PaletteDefinition PaletteCatalog::saved(size_t index,const RandomFunction& random,std::stop_token stop)const{
    auto rgba=deserializePalette(text_.at(index));const auto background=companionColor(rgba,random,stop);
    return {std::move(rgba),"Palette "+std::to_string(index),background,int32_t(index)};
}
PaletteDefinition PaletteCatalog::generate(const RandomFunction& random,std::stop_token stop)const{
    if(stop.stop_requested())throw std::runtime_error("Prism Flow palette generation cancelled");
    if(draw(random)>.005){const auto index=std::min(size_t(std::floor(draw(random)*text_.size())),text_.size()-1);return saved(index,random,stop);}
    double r=.01+draw(random)*6.5*(2./3),g=.01+draw(random)*6.5*(2./3),b=.01+draw(random)*6.5*(2./3);const double sum=r+g+b;
    if(sum<5.5){r=r*5.5/sum;g=g*5.5/sum;b=b*5.5/sum;}if(sum>6.5){r=r*6.5/sum;g=g*6.5/sum;b=b*6.5/sum;}
    const double rp=std::pow(2.,-.7+2.*draw(random)),gp=std::pow(2.,-.7+2.*draw(random)),bp=std::pow(2.,-.7+2.*draw(random));
    Palette rgba(8*4,1);for(unsigned n=0;n<8;++n){const double t=n*(1./7);
        rgba[n*4]=float(r*std::pow(t,rp));rgba[n*4+1]=float(g*std::pow(t,gp));rgba[n*4+2]=float(b*std::pow(t,bp));}
    const double power=std::max(0.,std::min(1.,draw(random)*1.8-.2))*.6;double sr=1,sg=1,sb=1;
    for(unsigned n=1;n<8;++n){sr*=std::pow(.78+.53*draw(random),power);sg*=std::pow(.78+.53*draw(random),power);sb*=std::pow(.78+.53*draw(random),power);
        rgba[n*4]=float(double(rgba[n*4])*sr);rgba[n*4+1]=float(double(rgba[n*4+1])*sg);rgba[n*4+2]=float(double(rgba[n*4+2])*sb);}
    const auto background=companionColor(rgba,random,stop);return {std::move(rgba),"Random palette",background,-1};
}
int32_t PaletteCatalog::add(std::span<const float> rgba){if(text_.size()>=512)throw std::out_of_range("Native saved palette limit reached");const int32_t index=int32_t(text_.size());text_.push_back(serializePalette(rgba));return index;}
std::string PaletteCatalog::saveBackground(int32_t index,const Color& background){
    if(index<0||size_t(index)>=text_.size())return {};for(auto value:background)if(!std::isfinite(value)||value<0||value>65504)throw std::invalid_argument("Invalid saved palette background");
    auto rgba=deserializePalette(text_[index]);for(unsigned c=0;c<3;++c)rgba[c]=float(background[c]);text_[index]=serializePalette(rgba);
    return "  \""+text_[index]+"\",  // "+std::to_string(index)+'\n';
}
}
