// SPDX-License-Identifier: GPL-2.0-or-later
#include "scene_dawn.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
using namespace prismflow;using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const fs::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Missing scene reference file");return {std::istreambuf_iterator<char>(input),{}};}
template<class T>void save(const fs::path& path,const std::vector<T>& data){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()*sizeof(T)));}
}
int main(int argc,char** argv){json report;fs::path output;
 try{
    if(argc<4||argc>5)throw std::runtime_error("Expected shader directory, fixtures.json, output directory and optional --exact");const bool exact=argc==5&&std::string(argv[4])=="--exact";const fs::path fixturePath=argv[2];output=argv[3];fs::create_directories(output);
    std::ifstream input(fixturePath);const auto fixture=json::parse(input);GpuContext context;report["adapter"]=context.info();report["reference_adapter"]=fixture.at("adapter");report["exact_required"]=exact;
    bool passed=true;uint64_t total=0,differing=0;double peak=0;
    for(const auto& item:fixture.at("cases")){
      SceneOptions options;options.seed=item.at("seed");options.supersampling=item.at("supersampling");options.linear=item.at("linear");options.headroom=item.at("headroom");DawnScene scene(context,argv[1],128,80,options);report["initial_random_calls"].push_back(scene.randomCalls());visualizers::MusicFrame audio;audio.rms=.12;audio.visualEnergy=.04;
      for(const auto& frame:fixture.at("frames")){if(frame.at("scene")!=item.at("scene"))continue;const unsigned index=frame.at("frame"),width=frame.at("width"),height=frame.at("height");scene.resize(width,height);if(frame.at("key").get<uint32_t>())scene.key(frame.at("key"));
        for(size_t n=0;n<audio.wave.size();++n)audio.wave[n]=float(.6*std::sin(n*.037+index*.3));for(size_t n=0;n<audio.spectrum.size();++n)audio.spectrum[n]=float(.2+.1*std::cos(n*.03+index));audio.beat=index%3==0;
        scene.render(audio,frame.at("time"),frame.at("dt"),frame.at("averageStarLifetime"));if(std::abs(scene.presetTime()-frame.at("presetTime").get<double>())>=1e-9){report["clock"]={{"actual",scene.presetTime()},{"expected",frame.at("presetTime")}};throw std::runtime_error("Integrated preset clock differs from original engine");}
        require(scene.randomCalls()==frame.at("randomCalls"),"Integrated random consumption differs from original engine");
        json result={{"scene",item.at("scene")},{"frame",index},{"warp_slots",scene.framePlan().warpCount},{"native_random_calls",scene.randomCalls()},{"reference_random_calls",frame.at("randomCalls")},{"reference_snapshot",frame.at("snapshot")}};for(size_t p=0;p<scene.controller().history.size();++p){const auto& preset=scene.controller().history.at(p);json snapshot={{"t1",preset.t1},{"t2",preset.t2}};for(const auto& w:preset.warps)snapshot["warps"].push_back({{"mode",w.mode},{"seed",w.rand_seed},{"str",w.strength}});result["native_snapshot"].push_back(snapshot);}
        const auto compare=[&](const std::string& name,const std::vector<uint8_t>& actual,bool half,double maxLimit,double meanLimit){
          const auto expected=bytes(fixturePath.parent_path()/frame.at(name+"_file").get<std::string>());require(actual.size()==expected.size(),"Scene texture byte count differs");double maximum=0,sum=0;uint64_t changed=0;const size_t count=actual.size()/(half?2:1);
          for(size_t n=0;n<count;++n){const auto a=half?visualizers::gpu::decodeHalf(uint16_t(actual[n*2])|uint16_t(actual[n*2+1])<<8):actual[n]/255.f,b=half?visualizers::gpu::decodeHalf(uint16_t(expected[n*2])|uint16_t(expected[n*2+1])<<8):expected[n]/255.f;
            require(std::isfinite(a)&&std::isfinite(b),"Nonfinite integrated scene component");const double error=std::abs(double(a)-b);maximum=std::max(maximum,error);sum+=error;if(error)++changed;
          }
          const double mean=sum/count;const bool okay=maximum<=maxLimit&&mean<=meanLimit&&(!exact||!changed);result[name]={{"components",count},{"maximum_absolute_error",maximum},{"mean_absolute_error",mean},{"differing_components",changed},{"pass",okay}};passed=passed&&okay;total+=count;differing+=changed;peak=std::max(peak,maximum);
          if(!okay)save(output/("actual-"+frame.at(name+"_file").get<std::string>()),actual);
        };
        const auto feedback=readHalfTexture(context,scene.feedback().texture());std::vector<uint8_t> feedbackBytes(feedback.size()*2);std::memcpy(feedbackBytes.data(),feedback.data(),feedbackBytes.size());compare("feedback",feedbackBytes,true,.002,.0002);
        if(options.linear){const auto color=readHalfTexture(context,scene.colorTexture());std::vector<uint8_t> colorBytes(color.size()*2);std::memcpy(colorBytes.data(),color.data(),colorBytes.size());compare("color",colorBytes,true,.004,.0002);}else compare("color",readByteTexture(context,scene.colorTexture()),false,1./255.,.0002);
        report["frames"].push_back(std::move(result));
      }
    }
    report["result"]=passed?"pass":"fail";report["components_compared"]=total;report["differing_components"]=differing;report["maximum_absolute_error"]=peak;std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<json{{"result",report["result"]},{"frames",report["frames"].size()},{"components",total},{"differing",differing},{"maximum_absolute_error",peak}}.dump(2)<<'\n';return passed?0:1;
 }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();if(!output.empty())std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
