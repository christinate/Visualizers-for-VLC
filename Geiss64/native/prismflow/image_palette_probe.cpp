// SPDX-License-Identifier: Apache-2.0
#include "image_palette.hpp"
#include "random.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
namespace fs=std::filesystem;using namespace prismflow;using json=nlohmann::json;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>static void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid or cancelled image palette request accepted");}
struct Stream {Mulberry32 next;std::vector<double> prefix;size_t calls=0;double operator()(){const auto n=calls++;return n<prefix.size()?prefix[n]:next();}};
int main(int argc,char** argv){json report;fs::path output;size_t current=0;
    try{require(argc==3,"Expected image-palette manifest and output JSON");const fs::path file=argv[1];output=argv[2];require(fs::file_size(file)<=8*1024*1024,"Image-palette manifest exceeds bound");std::ifstream input(file);const auto fixtures=json::parse(input);require(fixtures.at("images").size()<=64&&fixtures.at("cases").size()<=2048,"Image-palette corpus exceeds bound");
        std::vector<std::vector<uint8_t>> images;for(const auto& image:fixtures.at("images")){const fs::path name=image.at("file").get<std::string>();require(name.filename()==name,"Invalid image fixture path");const auto path=file.parent_path()/name;const auto count=fs::file_size(path);require(count<=512*512*4,"Image fixture exceeds bound");std::vector<uint8_t> data(count);std::ifstream raw(path,std::ios::binary);raw.read(reinterpret_cast<char*>(data.data()),std::streamsize(count));require(bool(raw),"Cannot read image fixture");images.push_back(std::move(data));}
        uint64_t components=0,floatComponents=0,floatDifferences=0,expectedErrors=0;double maximum=0;report["per_case"]=json::array();
        for(const auto& fixture:fixtures.at("cases")){const auto index=fixture.at("image").get<size_t>();require(index<images.size(),"Invalid fixture image index");const auto& im=fixtures.at("images").at(index);ImagePaletteOptions p;const auto& options=fixture.at("options");p.sampleSize=options.value("sample_size",128u);p.clusterCount=options.value("cluster_count",12u);p.iterations=options.value("cluster_iterations",10u);p.highlightLuminance=options.value("highlight_luminance",.85);if(options.contains("smoothness")&&!options.at("smoothness").is_null())p.smoothness=options.at("smoothness");
            Stream stream{Mulberry32(fixture.at("seed")),fixture.at("prefix").get<std::vector<double>>()};const auto run=[&]{return paletteFromImageRgba(im.at("width"),im.at("height"),images[index],[&]{return stream();},p);};
            if(fixture.contains("error")){rejects(run);require(stream.calls==fixture.at("calls"),"Rejected image consumed a different random stream");++expectedErrors;++current;continue;}
            const auto colors=run();require(stream.calls==fixture.at("calls"),"Image palette random consumption differs");double previous=-1,caseMax=0;
            for(unsigned n=0;n<8;++n){for(unsigned c=0;c<3;++c){const double v=colors[n][c],reference=fixture.at("colors").at(n).at(c);require(std::isfinite(v)&&v>=0&&v<=1,"Image palette is outside finite linear sRGB gamut");const double e=std::abs(v-reference);require(e<=1e-7+std::abs(reference)*1e-7,"Image palette differs from unchanged reference");caseMax=std::max(caseMax,e);++components;}
                const double y=.2126*colors[n][0]+.7152*colors[n][1]+.0722*colors[n][2];require(y>previous,"Image palette luminance is not strictly increasing");previous=y;}
            require(colors[0]==Color{0,0,0}&&std::abs(previous-p.highlightLuminance)<=1e-12,"Image palette endpoint changed");
            const auto rgba=imagePaletteRgba(colors);for(size_t n=0;n<rgba.size();++n){const float reference=fixture.at("rgba").at(n);require(std::abs(double(rgba[n])-reference)<=2e-7,"Float image palette packing differs");floatDifferences+=std::bit_cast<uint32_t>(rgba[n])!=std::bit_cast<uint32_t>(reference);++floatComponents;}
            maximum=std::max(maximum,caseMax);report["per_case"].push_back({{"case",current},{"max_error",caseMax},{"random_calls",stream.calls}});++current;
        }
        const std::array<uint8_t,4> pixel={31,73,217,255};const auto valid=[&](const ImagePaletteOptions& p){return paletteFromImageRgba(1,1,pixel,[]{return .5;},p);};
        for(unsigned invalid=0;invalid<10;++invalid){ImagePaletteOptions p;switch(invalid){case 0:p.sampleSize=0;break;case 1:p.sampleSize=513;break;case 2:p.clusterCount=0;break;case 3:p.clusterCount=33;break;case 4:p.iterations=0;break;case 5:p.iterations=51;break;case 6:p.highlightLuminance=0;break;case 7:p.highlightLuminance=1;break;case 8:p.smoothness=NAN;break;case 9:p.smoothness=-.1;break;}rejects([&]{valid(p);});}
        rejects([&]{paletteFromImageRgba(0,1,pixel,[]{return .5;});});rejects([&]{paletteFromImageRgba(513,1,pixel,[]{return .5;});});rejects([&]{paletteFromImageRgba(1,1,std::span<const uint8_t>(pixel).first(3),[]{return .5;});});rejects([&]{paletteFromImageRgba(1,1,pixel,{});});for(double value:std::array<double,4>{-1.,1.,NAN,INFINITY})rejects([&]{paletteFromImageRgba(1,1,pixel,[&]{return value;});});
        std::stop_source stopped;stopped.request_stop();rejects([&]{paletteFromImageRgba(1,1,pixel,[]{return .5;},{},stopped.get_token());});
        std::vector<uint8_t> large(512*512*4);Mulberry32 pixels(123);for(auto& b:large)b=uint8_t(pixels()*256);std::atomic_bool entered=false,cancelled=false;ImagePaletteOptions slow;slow.clusterCount=32;slow.iterations=50;
        const auto start=std::chrono::steady_clock::now();std::jthread job([&](std::stop_token stop){try{paletteFromImageRgba(512,512,large,[&]{entered.store(true);return .5;},slow,stop);}catch(const std::exception&){cancelled.store(stop.stop_requested());}});
        for(unsigned n=0;n<1000&&!entered.load();++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(entered.load(),"Image palette cancellation control did not start clustering");job.request_stop();job.join();require(cancelled.load(),"In-flight image palette was not cancelled");const auto cancelMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        report["result"]="pass";report["cases"]=current;report["expected_transparent_errors"]=expectedErrors;report["components_compared"]=components;report["maximum_error"]=maximum;report["float_components_compared"]=floatComponents;report["float_components_differing_bits"]=floatDifferences;report["invalid_and_cancellation_checks"]="pass";report["in_flight_cancel_ms_including_setup"]=cancelMs;report["scope"]="Native palette extraction from color-managed sRGB RGBA8, unchanged-source RNG/numeric comparison; native decoding and window/controller integration are separate";
        std::ofstream(output)<<report.dump(2)<<'\n';auto summary=report;summary.erase("per_case");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& e){report["result"]="fail";report["case"]=current;report["error"]=e.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';auto summary=report;summary.erase("per_case");std::cerr<<summary.dump(2)<<'\n';return 1;}
}
