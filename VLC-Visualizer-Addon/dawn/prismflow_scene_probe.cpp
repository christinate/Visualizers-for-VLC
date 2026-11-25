// SPDX-License-Identifier: GPL-2.0-or-later
#include "scene_dawn.hpp"
#include "plugin.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QGuiApplication>
#endif
using namespace visualizers::gpu;using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F&& function){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid Prism Flow request was accepted");}
double milliseconds(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
}
int main(int argc,char** argv){
#if !defined(_WIN32) && !defined(__APPLE__)
 QGuiApplication ui(argc,argv);
#endif
 json report;fs::path output;
 try{
    if(argc!=4)throw std::runtime_error("Expected native module, shader directory and output directory");output=argv[3];fs::create_directories(output);
    Context context;report["adapter"]=context.info();const auto assets=fs::absolute(argv[2]);PluginLibrary library(fs::absolute(argv[1]),"geiss");
    require(library.api().api_version==VIS_NATIVE_API_VERSION,"Unexpected Prism Flow API version");
    rejects([&]{library.create(context,128,80,assets,"{\"supersampling\":3}");});rejects([&]{library.create(context,0,80,assets);});
    rejects([&]{library.create(context,128,80,assets,"{\"nested\":"+std::string(34,'[')+"0"+std::string(34,']')+"}");});
    visualizers::MusicFrame audio;audio.rms=.12;audio.visualEnergy=.04;audio.bass=.2;
    uint64_t compared=0,finite=0;for(unsigned test=0;test<3;++test){
      prismflow::SceneOptions options;options.supersampling=test==1?2:1;options.linear=test==2;options.headroom=options.linear?1000./203:1;
      auto start=std::chrono::steady_clock::now();prismflow::DawnScene direct(context,assets,128,80,options);const double directInit=milliseconds(start);
      auto plugin=library.create(context,128,80,assets,json{{"supersampling",options.supersampling},{"seed",options.seed}}.dump(),options.linear?VIS_NATIVE_LINEAR_OUTPUT:0);
      json item={{"supersampling",options.supersampling},{"linear_experimental",options.linear},{"direct_initialization_ms",directInit}};
      unsigned width=128,height=80;std::vector<uint16_t> previous;double maximum=0;bool changed=false;
      for(unsigned frame=0;frame<7;++frame){
        for(size_t n=0;n<audio.wave.size();++n)audio.wave[n]=float(.6*std::sin(double(n)*.037+frame*.3));
        for(size_t n=0;n<audio.spectrum.size();++n)audio.spectrum[n]=float(.2+.1*std::cos(double(n)*.03+frame));audio.beat=frame%3==0;
        if(frame==3){width=145;height=91;direct.resize(width,height);plugin->resize(width,height);}
        if(frame==5){direct.key('w');plugin->key('w');}
        start=std::chrono::steady_clock::now();direct.render(audio,frame/60.,1./60);if(frame==0)item["first_frame_ms"]=milliseconds(start);plugin->render(audio,frame/60.,1./60);
        uint32_t encoding=0;auto image=plugin->image(&encoding);require(encoding==(options.linear?VIS_NATIVE_OUTPUT_LINEAR:VIS_NATIVE_OUTPUT_SRGB),"Output color encoding differs");
        const auto pixels=context.readRgba16(image,width,height),reference=context.readRgba16(direct.texture(),width,height);require(pixels==reference,"Native C ABI frame differs from direct Prism Flow scene");compared+=pixels.size();
        for(auto pixel:pixels){const auto value=decodeHalf(pixel);require(std::isfinite(value),"Integrated scene contains nonfinite output");maximum=std::max(maximum,double(value));++finite;}
        if(!previous.empty()&&pixels.size()==previous.size()&&pixels!=previous)changed=true;previous=pixels;
        if(frame==6){std::ofstream raw(output/("scene-"+std::to_string(test)+".rgba16"),std::ios::binary);raw.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*2));}
      }
      require(changed&&maximum>.01,"Integrated scene did not animate");item["maximum_component"]=maximum;item["animated"]=changed;item["frames"]=7;
      rejects([&]{plugin->resize(0,0);});audio.wave[0]=std::numeric_limits<float>::infinity();rejects([&]{plugin->render(audio,1,1./60);});audio.wave[0]=0;
      // Exercise original navigation and each independent effect rerandomizer.
      for(const uint32_t key:std::array<uint32_t,8>{32,13,8,'m','p','d','s','l'}){plugin->key(key);plugin->render(audio,1+double(key)*.001,1./60);context.checkErrors();}
      direct.embed("Prism Flow native");direct.render(audio,2,1./60);direct.render(audio,3,1./60);require(!direct.framePlan().points.empty()||direct.framePlan().dots.stars_opacity>0,"No frame effects remain after title burn");
      report["cases"].push_back(std::move(item));
    }
    report["result"]="pass";report["direct_module_compared_components"]=compared;report["finite_components"]=finite;report["scope"]="Integrated native CPU/GPU and C ABI consistency; original-engine GPU scene parity remains a separate gate";
    std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
 }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();if(!output.empty())std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
