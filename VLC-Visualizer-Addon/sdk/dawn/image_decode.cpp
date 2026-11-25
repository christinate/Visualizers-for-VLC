// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_decode.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstring>
#include <stdexcept>
namespace visualizers::image_detail {
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Image decoding cancelled");}
void validateFile(const std::filesystem::path& path,uint32_t size,std::stop_token stop){
    cancelled(stop);if(!size||size>512)throw std::invalid_argument("Image sample side must be 1..512");
    if(path.empty()||!std::filesystem::is_regular_file(path))throw std::invalid_argument("Expected a local image file");const auto bytes=std::filesystem::file_size(path);if(!bytes||bytes>64ull*1024*1024)throw std::invalid_argument("Image file must contain 1 byte..64 MiB");
}
std::array<uint32_t,2> dimensions(uint32_t width,uint32_t height,uint32_t size){
    if(!width||!height||width>32768||height>32768||uint64_t(width)*height>67108864)throw std::invalid_argument("Image dimensions exceed the 32768-axis/64-million-pixel limit");
    const double scale=std::min(1.,double(size)/std::max(width,height));return {uint32_t(std::max(1.,std::floor(width*scale+.5))),uint32_t(std::max(1.,std::floor(height*scale+.5)))};
}
void unpremultiply(RgbaImage& im){for(size_t n=0;n<im.rgba.size();n+=4){const unsigned a=im.rgba[n+3];for(unsigned c=0;c<3;++c)im.rgba[n+c]=a?uint8_t(std::min(255u,(unsigned(im.rgba[n+c])*255+a/2)/a)):0;}}
std::optional<GifFrame> gifFirstFrame(const std::filesystem::path& file,std::stop_token stop){
    std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot read image header");std::array<uint8_t,13> header{};input.read(reinterpret_cast<char*>(header.data()),6);if(input.gcount()!=6||(std::memcmp(header.data(),"GIF87a",6)&&std::memcmp(header.data(),"GIF89a",6)))return {};
    input.read(reinterpret_cast<char*>(header.data()+6),7);if(!input)throw std::runtime_error("Truncated GIF logical screen");uint64_t consumed=13;
    const auto word=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8);};
    const auto byte=[&](){if(++consumed>64ull*1024*1024)throw std::runtime_error("GIF metadata exceeds file limit");const auto value=input.get();if(value==std::char_traits<char>::eof())throw std::runtime_error("Truncated GIF metadata");return uint8_t(value);};
    const auto skip=[&](uint32_t count){if(count>64ull*1024*1024-consumed)throw std::runtime_error("GIF metadata exceeds file limit");input.ignore(count);if(input.gcount()!=count)throw std::runtime_error("Truncated GIF data block");consumed+=count;};
    const auto w=word(header.data()+6),h=word(header.data()+8);dimensions(w,h,128);if(header[10]&0x80)skip(3u<<(1+(header[10]&7)));
    for(;;){cancelled(stop);const auto kind=byte();if(kind==0x2c){std::array<uint8_t,9> frame{};for(auto& b:frame)b=byte();const auto fw=word(frame.data()+4),fh=word(frame.data()+6);dimensions(fw,fh,128);return GifFrame{w,h,word(frame.data()),word(frame.data()+2),fw,fh};}
        if(kind==0x21){byte();for(;;){cancelled(stop);const auto count=byte();if(!count)break;skip(count);}}else if(kind==0)continue;else throw std::runtime_error("GIF contains no valid first frame");}
}
}
