// SPDX-License-Identifier: Apache-2.0
#include "frame_cpu.hpp"
#include "random.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace prismflow;using json=nlohmann::json;
namespace {
std::string current;uint64_t components=0,points=0,stars=0,waves=0;double maxDouble=0,maxPoint=0,maxWave=0;
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(current+": "+message);}
void compare(double value,double expected,const std::string& name,double limit=1e-10){if(!std::isfinite(value)||!std::isfinite(expected))require(false,name+" is nonfinite");const double error=std::abs(value-expected);maxDouble=std::max(maxDouble,error);++components;if(error>limit)require(false,name+" differs: "+std::to_string(value)+" vs "+std::to_string(expected));}
template<class T,size_t N>void load(T& out,const json& values,const std::array<ParameterField<T>,N>& fields){for(auto field:fields)out.*field.member=values.at(field.name).template get<double>();}
template<class T,size_t N>void compare(const T& value,const json& expected,const std::array<ParameterField<T>,N>& fields){for(auto f:fields)compare(value.*f.member,expected.at(f.name),f.name);}
void waveCompare(std::span<const float> value,const json& expected){require(value.size()==expected.size(),"wave size differs");for(size_t n=0;n<value.size();++n){const double error=std::abs(double(value[n])-expected[n].get<double>());maxWave=std::max(maxWave,error);++waves;if(error>2e-7)require(false,"wave sample differs at "+std::to_string(n));}}
template<class F>void rejects(F&& function){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid input was accepted");}
}
int main(int argc,char** argv){json report;std::filesystem::path output;
 try{
    if(argc!=3)throw std::runtime_error("Expected generated frame fixtures and result path");output=argv[2];std::ifstream input(argv[1]);const auto fixture=json::parse(input);
    for(const auto& test:fixture.at("waveform")){
      current="waveform "+test.at("operation").get<std::string>();const auto operation=test.at("operation").get<std::string>();
      if(operation=="align"){WaveAligner align;for(const auto& frame:test.at("frames")){const auto signal=frame.at("input").get<std::vector<float>>();waveCompare(align.align(signal,test.at("fraction")),frame.at("expected"));}}
      else {const auto signal=test.at("input").get<std::vector<float>>();waveCompare(operation=="blur"?blurWave(signal,test.at("radius")):resampleWave(signal,test.at("count")),test.at("expected"));}
    }
    for(const auto& group:fixture.at("groups")){
      PresetHistory history;for(const auto& row:group.at("rows")){
        Preset p;p.number=row.at("global_preset_number");p.t1=row.at("t1");p.t2=row.at("t2");p.mapsReady=true;
        load(p.wave,row.at("wave_params"),waveFields);load(p.dots,row.at("dot_params"),dotFields);load(p.shader,row.at("shader_params"),shaderFields);load(p.motion,row.at("motion_params"),motionFields);
        p.originalPalette.name=row.at("palette_orig").at("name");p.originalPalette.background=row.at("palette_orig").at("bkg").get<Color>();p.expandedPalette=row.at("palette_expanded").get<ExpandedPalette>();
        for(size_t i=0;i<4;++i){const auto& src=row.at("warps").at(i);auto& w=p.warps[i];w.strength=src.at("str");const auto& map=src.at("warp_map");w.map.mode=map.at("mode");w.map.breathingStrength=map.at("breathing_str");w.map.breathingFrequency=map.at("breathing_freq");w.map.breathingPhase=map.at("breathing_phase");w.starsFollowMotion=map.at("warp_prefs").at("stars_follow_motion");}
        history.append(std::move(p));
      }
      FramePlanner planner;FramePlan plan;size_t frameNumber=0;for(const auto& frame:group.at("frames")){
        current=group.at("name").get<std::string>()+" frame "+std::to_string(frameNumber++);const auto& c=frame.at("input");const auto& expected=frame.at("expected");FrameInput control;
        control.width=c.at("width");control.height=c.at("height");control.shiftTime=c.at("shiftTime");control.time=c.at("time");control.fps=c.at("fps");control.motionScale=c.at("motionScale");control.brightness=c.at("brightness");control.headroom=c.at("headroom");control.darkening=c.at("darkening");control.alignFraction=c.at("alignFraction");control.waveSmoothing=c.at("waveSmoothing");control.beat=c.at("beat");control.paletteLocked=c.at("paletteLocked");control.frozen=c.at("frozen");control.experiment=c.at("experiment");control.averageStarLifetime=c.at("averageStarLifetime");control.embedUnits=c.at("embedUnits");
        const auto signal=frame.at("audio").at("wave").get<std::vector<float>>();Mulberry32 random(frame.at("seed"));uint64_t calls=0;
        plan=planner.build(history,history.active(control.shiftTime),control,{signal,frame.at("audio").at("rms"),frame.at("audio").at("visualEnergy")},[&]{++calls;return random();},std::move(plan));
        require(calls==expected.at("calls"),"random draw count differs: "+std::to_string(calls)+" vs "+expected.at("calls").dump());
        compare(plan.wave,expected.at("wave"),waveFields);compare(plan.dots,expected.at("dots"),dotFields);compare(plan.shader,expected.at("shader"),shaderFields);compare(plan.motion,expected.at("motion"),motionFields);
        for(const auto& [name,value]:std::array<std::pair<const char*,double>,8>{{{"shiftX",plan.shiftX},{"shiftY",plan.shiftY},{"pointSize",plan.pointSize},{"amplitude",plan.amplitude},{"autoDarkening",plan.autoDarkening},{"darkening",plan.darkening},{"energy",planner.smoothedEnergy()},{"noiseX",plan.noiseX}}})compare(value,expected.at(name),name);
        compare(plan.noiseY,expected.at("noiseY"),"noiseY");for(size_t n=0;n<8;++n){compare(plan.strengths[n],expected.at("strengths").at(n),"strength");compare(plan.follow[n],expected.at("follow").at(n),"follow");}
        for(size_t n=0;n<1024;++n)compare(plan.palette[n],expected.at("palette").at(n),"palette",2e-7);for(size_t n=0;n<3;++n)compare(plan.background[n],expected.at("background").at(n),"background",2e-7);
        require(plan.points.size()==expected.at("points").size(),"point count differs: "+std::to_string(plan.points.size())+" vs "+std::to_string(expected.at("points").size()));
        for(size_t n=0;n<plan.points.size();++n){const double error=std::abs(double(plan.points[n])-expected.at("points").at(n).get<double>());maxPoint=std::max(maxPoint,error);++points;if(error>2e-4)require(false,"point component differs at "+std::to_string(n)+": "+std::to_string(plan.points[n])+" vs "+expected.at("points").at(n).dump());}
        require(plan.stars.size()==expected.at("stars").size(),"star spawn count differs");for(size_t n=0;n<plan.stars.size();++n){const auto& s=plan.stars[n];const auto& e=expected.at("stars").at(n);for(const auto& [name,value]:std::array<std::pair<const char*,double>,8>{{{"x",s.x},{"y",s.y},{"z",s.z},{"z2",s.z2},{"palette_position",s.palettePosition},{"fade_in",s.fadeIn},{"hold",s.hold},{"fade_out",s.fadeOut}}})compare(value,e.at(name),name);require(s.background==e.at("use_background"),"star background differs");++stars;}
        require(bool(plan.title)==!expected.at("title").is_null(),"title presence differs");if(plan.title){compare(plan.title->x,expected.at("title").at("x"),"title x");compare(plan.title->y,expected.at("title").at("y"),"title y");compare(plan.title->fontPixels,expected.at("title").at("fontPixels"),"title size");}
      }
    }
    current="length changes and invalid inputs";WaveAligner changes;std::vector<float> first(2048,1),second(97,2);changes.align(first,.55);const auto resized=changes.align(second,.85);require(resized.size()==82&&std::all_of(resized.begin(),resized.end(),[](float v){return v==2;}),"alignment length change retained stale samples");const auto expanded=changes.align(first,1);require(expanded.size()==2048&&expanded.front()==1,"fraction change retained a shorter template");
    rejects([&]{changes.align(first,0);});rejects([&]{changes.align(first,std::numeric_limits<double>::quiet_NaN());});rejects([&]{resampleWave(first,32769);});first[0]=std::numeric_limits<float>::infinity();rejects([&]{blurWave(first,1);});
    report={{"result","pass"},{"frame_sequences",fixture.at("groups").size()},{"frames",144},{"scalar_components",components},{"point_components",points},{"star_spawns",stars},{"waveform_components",waves},{"maximum_scalar_error",maxDouble},{"maximum_point_error_pixels",maxPoint},{"maximum_waveform_error",maxWave},{"source_fractional_startup_fault",fixture.at("startupFault")},{"native_length_change_controls","pass"}};
    std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
 }catch(const std::exception& error){report={{"result","fail"},{"case",current},{"error",error.what()}};if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
