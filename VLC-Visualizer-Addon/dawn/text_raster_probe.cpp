// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include "resource_metrics.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#ifdef _WIN32
#include <objbase.h>
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QGuiApplication>
#endif
namespace fs=std::filesystem;using namespace visualizers;using json=nlohmann::json;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>static void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid native font request accepted");}
static uint64_t hash(std::span<const uint8_t> b){uint64_t v=1469598103934665603ull;for(auto c:b){v^=c;v*=1099511628211ull;}return v;}
static json inspect(const RasterizedText& text){const auto& im=text.image;require(im.rgba.size()==uint64_t(im.width)*im.height*4&&im.width<=8192&&im.height<=8192&&uint64_t(im.width)*im.height<=16777216,"Invalid native font bitmap layout");
    uint32_t left=im.width,top=im.height,right=0,bottom=0;uint64_t nonzero=0;for(uint32_t y=0;y<im.height;++y)for(uint32_t x=0;x<im.width;++x){const size_t n=(size_t(y)*im.width+x)*4;const auto a=im.rgba[n+3];require(im.rgba[n]==(a?255:0)&&im.rgba[n+1]==im.rgba[n]&&im.rgba[n+2]==im.rgba[n],"Native mask is not white/transparent RGBA");if(a){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);++nonzero;}}
    const auto& m=text.metrics;return {{"width",im.width},{"height",im.height},{"nonzero_alpha",nonzero},{"alpha_bounds",nonzero?json::array({left,top,right,bottom}):json::array()},{"hash",hash(im.rgba)},{"metrics",{{"left",m.left},{"right",m.right},{"ascent",m.ascent},{"descent",m.descent},{"advance",m.advance},{"padding",m.padding},{"supersample",m.supersample},{"primary_family",m.primaryFamily}}}};
}
static void ppm(const RgbaImage& image,const fs::path& file){std::ofstream out(file,std::ios::binary);out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";for(size_t n=3;n<image.rgba.size();n+=4){const char shade=char(255-image.rgba[n]);for(unsigned k=0;k<3;++k)out.put(shade);}}
int main(int argc,char** argv){fs::path output="native-font";json report;unsigned cycles=80;
    try{
#if !defined(_WIN32) && !defined(__APPLE__)
        QGuiApplication ui(argc,argv);ui.setQuitOnLastWindowClosed(false);
#endif
        for(int n=1;n<argc;++n){const std::string option=argv[n];if(option=="--output"&&n+1<argc)output=argv[++n];else if(option=="--cycles"&&n+1<argc)cycles=unsigned(std::stoul(argv[++n]));else throw std::runtime_error("Unknown font probe argument");}require(cycles>=80&&cycles<=10000,"Font lifecycle count must be 80..10000");fs::create_directories(output);report["cases"]=json::array();
#ifdef _WIN32
        report["backend"]="DirectWrite/Direct2D/WIC";
#elif defined(__APPLE__)
        report["backend"]="CoreText/CoreGraphics";
#else
        report["backend"]="Qt 6 QFont/QPainter";
#endif
        const std::vector<std::string> strings={"Prism Flow 64-bit","Prism • 日本語 🎵","مرحبا بالعالم","e\xcc\x81 & café","Prism\tFlow\n64\rbit\f","Prism Flow 64 bit ","   ","","jgj AMW","                 AMW"};
        for(size_t n=0;n<strings.size();++n){TextRasterOptions options;if(n>=8){options.family="serif";options.fontPixels=18.25;options.supersample=3;options.paddingPixels=0;options.weight=400;}auto native=rasterizeText(strings[n],options);auto data=inspect(native);data["text"]=strings[n];if(n!=6&&n!=7)require(data.at("nonzero_alpha").get<uint64_t>()>0,"Native font did not render any glyphs");if(n==6||n==7)require(data.at("nonzero_alpha")==0,"Empty/space text produced ink");
            const auto again=rasterizeText(strings[n],options);require(again.image.rgba==native.image.rgba&&again.image.width==native.image.width&&again.image.height==native.image.height,"Native font rasterization is not repeatable");
            if(!native.image.rgba.empty()){
                ppm(native.image,output/("text-"+std::to_string(n)+".ppm"));
                const auto file="text-"+std::to_string(n)+".u8";std::ofstream raw(output/file,std::ios::binary);raw.write(reinterpret_cast<const char*>(native.image.rgba.data()),std::streamsize(native.image.rgba.size()));data["file"]=file;
                auto guardedOptions=options;guardedOptions.paddingPixels+=32;const auto guarded=rasterizeText(strings[n],guardedOptions);const int delta=int(guarded.metrics.padding-native.metrics.padding);uint64_t lost=0;
                for(uint32_t y=0;y<guarded.image.height;++y)for(uint32_t x=0;x<guarded.image.width;++x){const int xx=int(x)-delta,yy=int(y)-delta;if((xx<0||yy<0||xx>=int(native.image.width)||yy>=int(native.image.height))&&guarded.image.rgba[(size_t(y)*guarded.image.width+x)*4+3])++lost;}
                data["clipped_ink_pixels"]=lost;require(!lost,"Native font bitmap clips ink compared with a padded render");
            }report["cases"].push_back(data);}
        const auto controls=rasterizeText(strings[4]),spaces=rasterizeText(strings[5]);require(controls.image.width==spaces.image.width&&controls.image.height==spaces.image.height&&controls.image.rgba==spaces.image.rgba,"Single-line text whitespace normalization differs");
        rejects([&]{rasterizeText(std::string("x\0y",3));});rejects([&]{rasterizeText(std::string("\xc0\xaf",2));});rejects([&]{rasterizeText(std::string("\xed\xa0\x80",3));});rejects([&]{rasterizeText(std::string(8193,'x'));});TextRasterOptions bad;bad.fontPixels=NAN;rejects([&]{rasterizeText("X",bad);});bad.fontPixels=1e-300;rejects([&]{rasterizeText("X",bad);});bad={};bad.supersample=0;rejects([&]{rasterizeText("X",bad);});bad={};bad.fontPixels=1024;bad.supersample=8;rejects([&]{rasterizeText(std::string(64,'M'),bad);});
        json samples=json::array();for(unsigned n=0;n<cycles;++n){TextRasterOptions p;p.family=n%3==0?"monospace":n%3==1?"sans-serif":"serif";p.weight=n%2?400:700;p.supersample=1+n%3;auto image=rasterizeText(strings[n%4],p);require(inspect(image).at("nonzero_alpha").get<uint64_t>()>0,"Repeated native font request lost glyphs");if(n==15||n==47||n==79||(n+1)%500==0||n+1==cycles){auto sample=testing::resourceMetrics();sample["cycles"]=n+1;
#ifdef _WIN32
                sample["gdi_objects"]=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);sample["user_objects"]=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
#endif
                samples.push_back(sample);}}
#ifdef _WIN32
        const auto beforeSta=rasterizeText(strings[1]);require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"Cannot initialize existing-STA font control");
        try{const auto sta=rasterizeText(strings[1]);require(sta.image.rgba==beforeSta.image.rgba&&sta.image.width==beforeSta.image.width&&sta.image.height==beforeSta.image.height,"Font rasterization changed in an existing STA");}catch(...){CoUninitialize();throw;}CoUninitialize();report["existing_sta"]="pass";
#endif
        report["lifetimes"]=samples;report["result"]="pass";report["scope"]="Native font shaping/rasterization, UTF-8/bounds, whitespace, repeatability and bounded resource samples; OS font output may differ from Canvas";std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& e){report["result"]="fail";report["error"]=e.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
