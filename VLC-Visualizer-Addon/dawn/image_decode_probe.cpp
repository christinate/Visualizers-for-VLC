// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_decode.hpp"
#include "image_palette.hpp"
#include "random.hpp"
#include "resource_metrics.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QCoreApplication>
#endif
namespace fs=std::filesystem;using namespace visualizers;using json=nlohmann::json;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>static void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid/cancelled image decode was accepted");}
static fs::path utf8Path(const std::string& text){return fs::path(std::u8string(text.begin(),text.end()));}
int main(int argc,char** argv){json report;fs::path output;size_t current=0;
    try{
#if !defined(_WIN32) && !defined(__APPLE__)
        QCoreApplication ui(argc,argv);
#endif
        require(argc==3,"Expected image fixture manifest and output folder");const fs::path manifestFile=argv[1];output=argv[2];require(fs::file_size(manifestFile)<=1024*1024,"Image fixture manifest exceeds bound");std::ifstream input(manifestFile);const auto fixtures=json::parse(input);require(fixtures.at("cases").size()<=64,"Image fixture count exceeds bound");fs::create_directories(output);report["cases"]=json::array();
        for(const auto& test:fixtures.at("cases")){const auto name=test.at("file").get<std::string>();const auto relative=utf8Path(name);require(relative.filename()==relative,"Invalid fixture path");const auto file=manifestFile.parent_path()/relative;const uint32_t side=test.value("side",128u);const auto decode=[&]{return decodeImage(file,side);};
            if(test.value("error",false)){rejects(decode);report["cases"].push_back({{"file",name},{"expected_error",true}});++current;continue;}
            const auto decoded=decode();const auto& image=decoded.image;require(image.width==test.at("width")&&image.height==test.at("height"),"Decoded/oriented/downsampled image dimensions differ");require(image.width<=side&&image.height<=side&&image.rgba.size()==uint64_t(image.width)*image.height*4,"Invalid native decoded image layout");if(test.value("profile_required",false))require(decoded.sourceProfileReported,"Image's embedded color profile was not reported by the decoder");
            json item={{"file",name},{"width",image.width},{"height",image.height},{"source_width",decoded.sourceWidth},{"source_height",decoded.sourceHeight},{"source_profile_reported",decoded.sourceProfileReported},{"backend",decoded.backend}};
            const auto rawName="image-"+std::to_string(current)+".u8";std::ofstream raw(output/rawName,std::ios::binary);raw.write(reinterpret_cast<const char*>(image.rgba.data()),std::streamsize(image.rgba.size()));item["raw_file"]=rawName;
            if(test.contains("pixels")){for(const auto& pixel:test.at("pixels")){const uint32_t x=pixel.at("x"),y=pixel.at("y");require(x<image.width&&y<image.height,"Invalid pixel fixture coordinate");for(unsigned c=0;c<4;++c){const int actual=image.rgba[(size_t(y)*image.width+x)*4+c],expected=pixel.at("rgba").at(c);require(std::abs(actual-expected)<=test.value("pixel_tolerance",0),"Native decoded reference pixel differs");}}}
            prismflow::Mulberry32 random(test.value("seed",17u));const auto palette=[&]{return prismflow::paletteFromImageRgba(image.width,image.height,image.rgba,[&]{return random();});};if(test.value("transparent",false))rejects(palette);else item["palette"]=palette();report["cases"].push_back(item);++current;
        }
        require(!fixtures.at("cases").empty(),"Empty image fixture list");const auto validFile=manifestFile.parent_path()/utf8Path(fixtures.at("cases").at(0).at("file").get<std::string>());rejects([&]{decodeImage(validFile,0);});rejects([&]{decodeImage(validFile,513);});std::stop_source stopped;stopped.request_stop();rejects([&]{decodeImage(validFile,128,stopped.get_token());});
        report["lifetimes"]=json::array();for(unsigned n=0;n<120;++n){{const auto image=decodeImage(validFile,1+n%128);require(!image.image.rgba.empty(),"Repeated image decode lost pixels");}if(n==23||n==71||n==119){auto sample=testing::resourceMetrics();sample["cycles"]=n+1;report["lifetimes"].push_back(sample);}}
        report["result"]="pass";report["scope"]="Native local-file decode, color/orientation/downsampling, palette extraction, bounds/cancellation and repeated-release samples; window/drop/controller integration remains separate";std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& e){report["result"]="fail";report["case"]=current;report["error"]=e.what();if(!output.empty()){std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';}auto summary=report;summary.erase("cases");std::cerr<<summary.dump(2)<<'\n';return 1;}
}
