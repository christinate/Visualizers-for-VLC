// SPDX-License-Identifier: Apache-2.0
#include "palette.hpp"
#include "random.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;using json=nlohmann::json;using namespace prismflow;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static json read(const fs::path& file){require(fs::file_size(file)<1024*1024,"Palette reference exceeds its bound");std::ifstream input(file);return json::parse(input);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid palette request accepted");}
struct Stream {
    Mulberry32 next;std::vector<double> prefix;size_t calls=0;
    Stream(uint32_t seed,std::vector<double> values={}):next(seed),prefix(std::move(values)){}
    double operator()(){const auto n=calls++;return n<prefix.size()?prefix[n]:next();}
};
struct Comparison {
    uint64_t paletteComponents=0,expandedComponents=0,sampleComponents=0,toneComponents=0,blackFixComponents=0,whiteFadeComponents=0;
    double paletteError=0,backgroundError=0,sampleError=0,toneError=0;
    static double numeric(double actual,double expected,double tolerance,const char* message){
        require(std::isfinite(actual),"Nonfinite native palette result");const double error=std::abs(actual-expected);
        if(error>tolerance+std::abs(expected)*tolerance)throw std::runtime_error(std::string(message)+": native="+std::to_string(actual)+" reference="+std::to_string(expected));return error;
    }
    void tone(const FloatColor& actual,const json& expected,const std::string& correction,double headroom=1){for(unsigned c=0;c<3;++c){
        if(correction=="zero_luminance"){require(actual[c]==0&&expected.at(c).is_null(),"Reference zero-luminance NaN was not replaced with finite black");++blackFixComponents;}
        else if(correction=="bounded_white_fade"){require(actual[c]==float(headroom),"Extreme palette white fade exceeded its endpoint");++whiteFadeComponents;}
        else{require(correction.empty(),"Unknown tone correction");toneError=std::max(toneError,numeric(actual[c],expected.at(c),2e-7,"Tone mapping differs"));++toneComponents;}
    }}
    void palette(const PaletteDefinition& actual,size_t calls,const json& fixture){
        require(actual.rgba.size()==fixture.at("rgba").size(),"Palette length differs");
        require(actual.name==fixture.at("name").get<std::string>()&&actual.index==fixture.at("index"),"Palette identity differs");
        require(calls==fixture.at("calls"),"Palette random consumption differs");
        for(size_t n=0;n<actual.rgba.size();++n){paletteError=std::max(paletteError,numeric(actual.rgba[n],fixture.at("rgba").at(n),2e-7,"Palette color differs"));++paletteComponents;}
        for(unsigned c=0;c<3;++c)backgroundError=std::max(backgroundError,numeric(actual.background[c],fixture.at("background").at(c),1e-8,"Companion background differs"));
        // Four-decimal serialization must be exact, including halfway cases.
        require(serializePalette(actual.rgba)==fixture.at("serialized").get<std::string>(),"Palette serialization differs");
        const auto expanded=expandPalette(actual.rgba);for(size_t n=0;n<expanded.size();++n){paletteError=std::max(paletteError,numeric(expanded[n],fixture.at("expanded").at(n),2e-7,"Expanded palette differs"));++expandedComponents;}
        for(const auto& item:fixture.at("samples")){const auto rgb=samplePalette(actual.rgba,item.at("t"));for(unsigned c=0;c<3;++c){sampleError=std::max(sampleError,numeric(rgb[c],item.at("rgb").at(c),2e-7,"Sampled palette differs"));++sampleComponents;}}
        double maximum=0;for(size_t n=0;n<expanded.size();++n)if(n%4!=3)maximum=std::max(maximum,double(expanded[n]));
        for(const auto& item:fixture.at("tone"))for(unsigned n=0;n<256;++n){const Color rgb={expanded[n*4],expanded[n*4+1],expanded[n*4+2]};
            tone(reduceHdrHeadroom(item.at("headroom"),n*(1./255),rgb,maximum),item.at("values").at(n),item.at("corrections").at(n),item.at("headroom"));}
    }
};
int main(int argc,char** argv){json report;fs::path output;std::string current;
    try{
        require(argc==3,"Expected palette fixture manifest and result path");output=argv[2];const fs::path file=argv[1];const auto manifest=read(file);PaletteCatalog catalog;Comparison compared;
        require(catalog.size()==manifest.at("saved_count"),"Saved palette count differs");
        for(const auto& item:manifest.at("saved")){current=item.at("reference");const auto fixture=read(file.parent_path()/current);const size_t index=item.at("index");
            require(catalog.text(index)==fixture.at("text").get<std::string>(),"Saved palette text differs");Stream stream(item.at("seed"));const auto actual=catalog.saved(index,[&]{return stream();});compared.palette(actual,stream.calls,fixture);
            require(deserializePalette(fixture.at("text").get<std::string>())==actual.rgba,"Saved palette parsing differs");}
        for(const auto& item:manifest.at("generated")){current=item.at("reference");const auto fixture=read(file.parent_path()/current);Stream stream(item.at("seed"),item.at("prefix").get<std::vector<double>>());
            const auto actual=catalog.generate([&]{return stream();});compared.palette(actual,stream.calls,fixture);}
        for(const auto& item:manifest.at("custom")){current=item.at("reference");const auto fixture=read(file.parent_path()/current);auto rgba=fixture.at("rgba").get<Palette>();Stream stream(item.at("seed"));
            auto background=companionColor(rgba,[&]{return stream();});compared.palette({std::move(rgba),"custom",background,-1},stream.calls,fixture);
            const auto parsed=deserializePalette(fixture.at("text").get<std::string>());for(size_t n=0;n<parsed.size();++n)if(n%4!=3)require(parsed[n]==fixture.at("rgba").at(n).get<float>(),"Whitespace palette parsing differs");}
        current="catalog mutation";for(const auto& item:manifest.at("mutations")){const auto index=catalog.add(item.at("rgba").get<Palette>());require(index==item.at("index"),"Added palette index differs");
            require(catalog.text(index)==item.at("serialized").get<std::string>(),"Added palette text differs");require(catalog.saveBackground(index,item.at("background").get<Color>())==item.at("saved").get<std::string>(),"Saved background stanza differs");require(catalog.text(index)==item.at("text").get<std::string>(),"Saved background palette differs");}
        current="direct tone mapping";for(const auto& item:manifest.at("tone"))compared.tone(hdrToSdr(item.at("t"),item.at("rgb").get<Color>(),item.at("maximum")),item.at("values"),item.at("correction"));
        current="invalid requests";
        for(const auto* text:{"","0","-1","4097","1 0 0","1 nan 0 0","1 inf 0 0","1 -1 0 0","1 65505 0 0","1 0 0 0 extra","1.2 0 0 0"})rejects([&]{deserializePalette(text);});
        rejects([]{deserializePalette(std::string(1024*1024+1,' '));});rejects([]{samplePalette({},0);});rejects([]{expandPalette(Palette(4097*4));});
        rejects([]{serializePalette(Palette{0,0,std::numeric_limits<float>::infinity(),1});});rejects([]{samplePalette(Palette{0,0,0,1},std::numeric_limits<double>::quiet_NaN());});
        rejects([&]{catalog.saved(catalog.size(),[]{return .5;});});rejects([&]{catalog.generate({});});rejects([&]{catalog.generate([]{return 1.;});});
        rejects([]{hdrToSdr(0,{-1,0,0},1);});rejects([]{reduceHdrHeadroom(0,0,{0,0,0},1);});rejects([]{reduceHdrHeadroom(.5,0,{0,0,0},1);});rejects([]{reduceHdrHeadroom(65505,0,{0,0,0},1);});
        std::stop_source stop;stop.request_stop();rejects([&]{catalog.generate([]{return .5;},stop.get_token());});rejects([&]{companionColor(Palette{0,0,0,1},[]{return .5;},stop.get_token());});
        require(catalog.saveBackground(-1,{0,0,0}).empty()&&catalog.saveBackground(512,{0,0,0}).empty(),"Invalid save index behavior differs");
        while(catalog.size()<512)catalog.add(Palette{.03125f,.09375f,.15625f,1});rejects([&]{catalog.add(Palette{0,0,0,1});});require(catalog.size()==512,"Palette catalog bound changed");
        report={{"result","pass"},{"saved_palettes",manifest.at("saved").size()},{"generated_palettes",manifest.at("generated").size()},{"custom_palettes",manifest.at("custom").size()},
            {"palette_components",compared.paletteComponents},{"expanded_components",compared.expandedComponents},{"sample_components",compared.sampleComponents},{"tone_components",compared.toneComponents},
            {"reference_zero_luminance_nan_components_fixed",compared.blackFixComponents},{"bounded_white_fade_components",compared.whiteFadeComponents},{"maximum_palette_error",compared.paletteError},{"maximum_background_error",compared.backgroundError},
            {"maximum_sample_error",compared.sampleError},{"maximum_tone_error",compared.toneError},{"invalid_cancelled_and_catalog_limit_checks","pass"}};
        std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
