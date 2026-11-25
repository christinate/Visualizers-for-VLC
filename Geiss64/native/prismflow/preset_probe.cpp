// SPDX-License-Identifier: Apache-2.0
#include "preset.hpp"
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
static json read(const fs::path& file){require(fs::file_size(file)<4*1024*1024,"Preset reference exceeds bound");std::ifstream input(file);return json::parse(input);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid preset request accepted");}
static json encode(const MotionParams& p){json j;
#define F(field) j[#field]=p.field;
    F(blended_net_motion) F(blended_net_zoom_motion) F(blended_in_or_out_motion) F(blended_net_clockwise_motion) F(blended_cw_or_ccw_motion) F(blended_motion_cx) F(blended_motion_cy)
#undef F
    return j;
}
static json encode(const WaveParams& p){json j;
#define F(field) j[#field]=p.field;
    F(wave_flat_str) F(wave_flat_angle) F(wave_flat_scale) F(wave_flat_is_stereo) F(wave_flat_stereo_sep) F(wave_flat_stereo_amplitude_scale) F(wave_flat_cx) F(wave_flat_cy)
    F(wave_circ_str) F(wave_circ_rad) F(wave_circ_scale) F(wave_circ_angle) F(wave_circ_cx) F(wave_circ_cy)
#undef F
    return j;
}
static json encode(const DotParams& p){json j;
#define F(field) j[#field]=p.field;
    F(radial_beat_dots) F(random_beat_dots) F(fading_dots) F(grid_dots) F(chaser_1) F(chaser_2) F(dotty_chaser) F(stars_opacity) F(stars_population)
#undef F
    return j;
}
static json encode(const ShaderParams& p){json j;
#define F(field) j[#field]=p.field;
    F(rx_diff_grey_scott_str) F(rx_diff_grey_scott_str2) F(rx_diff_barkley_str) F(frequency_injector_str) F(frequency_injector_freq_scale) F(painterly_str) F(sharpen_str) F(noise_str)
    F(layer2_warp_scale) F(layer1_density) F(layer2_brightness) F(layer2_motion_scale)
#undef F
    return j;
}
struct Comparison {
    uint64_t scalars=0,motionSamples=0,paletteComponents=0,snapshots=0,rebuilds=0;double parameterError=0,mapError=0,paletteError=0;
    static double numeric(double actual,double expected,double tolerance,const std::string& label){
        require(std::isfinite(actual),"Nonfinite native preset output");const double error=std::abs(actual-expected);
        if(error>tolerance+std::abs(expected)*tolerance)throw std::runtime_error(label+" differs: native="+std::to_string(actual)+" expected="+std::to_string(expected));return error;
    }
    void parameters(const json& actual,const json& expected){require(actual.size()==expected.size(),"Preset parameter field count differs");for(const auto& [key,value]:expected.items()){
        parameterError=std::max(parameterError,numeric(actual.at(key),value,1e-9,key));++scalars;}}
    void floats(std::span<const float> actual,const json& expected){require(actual.size()==expected.size(),"Preset palette length differs");for(size_t n=0;n<actual.size();++n){paletteError=std::max(paletteError,numeric(actual[n],expected.at(n),2e-7,"palette"));++paletteComponents;}}
    void check(const Preset& preset,size_t calls,const json& expected,const std::vector<size_t>& pixels){
        require(preset.number==expected.at("global_preset_number")&&preset.t1==expected.at("t1")&&preset.t2==expected.at("t2"),"Preset identity or fade times differ");
        require(preset.mapsReady==expected.at("warp_maps_ready")&&preset.rebuildId==expected.at("rebuild_id"),"Preset readiness differs");
        require(calls==expected.at("calls"),"Preset random consumption differs");
        parameters(encode(preset.motion),expected.at("motion_params"));parameters(encode(preset.wave),expected.at("wave_params"));parameters(encode(preset.dots),expected.at("dot_params"));parameters(encode(preset.shader),expected.at("shader_params"));
        for(size_t slot=0;slot<preset.warps.size();++slot){const auto& warp=preset.warps[slot];const auto& ref=expected.at("warps").at(slot);const auto& map=ref.at("warp_map");
            require(warp.mode==ref.at("mode")&&warp.rand_seed==ref.at("rand_seed")&&warp.strength==ref.at("str"),"Preset warp identity differs");
            require(warp.map.mode==map.at("mode")&&warp.map.seed==map.at("seed")&&warp.map.width==map.at("W")&&warp.map.height==map.at("H"),"Preset map identity differs");
            for(const auto& field:preferenceFields){parameterError=std::max(parameterError,numeric(warp.map.preferences.*field.member,map.at("warp_prefs").at(field.name),1e-9,field.name));++scalars;}
            require(warp.starsFollowMotion.has_value(),"Preset stars lost their motion choice");parameterError=std::max(parameterError,numeric(*warp.starsFollowMotion,map.at("warp_prefs").at("stars_follow_motion"),1e-9,"stars_follow_motion"));
            parameterError=std::max(parameterError,numeric(warp.map.breathingStrength,map.at("breathing_str"),1e-9,"breathing_str"));parameterError=std::max(parameterError,numeric(warp.map.breathingPhase,map.at("breathing_phase"),1e-9,"breathing_phase"));parameterError=std::max(parameterError,numeric(warp.map.breathingFrequency,map.at("breathing_freq"),1e-9,"breathing_freq"));
            if(map.at("src_dxy").is_null()){require(warp.map.displacement.empty(),"Dynamic preset has static pixels");require(warp.map.dynamic.size()==map.at("dynamic_motion_params").size(),"Dynamic descriptor length differs");
                for(size_t n=0;n<warp.map.dynamic.size();++n){mapError=std::max(mapError,numeric(warp.map.dynamic[n],map.at("dynamic_motion_params").at(n),2e-7,"dynamic descriptor"));++motionSamples;}}
            else{require(warp.map.displacement.size()==size_t(kWarpMapSize)*kWarpMapSize*2,"Static preset pixels missing");require(map.at("src_dxy").size()==pixels.size()*2,"Bad sampled map reference");
                for(size_t n=0;n<pixels.size();++n)for(unsigned c=0;c<2;++c){mapError=std::max(mapError,numeric(warp.map.displacement.at(pixels[n]*2+c),map.at("src_dxy").at(n*2+c),2e-7,"motion pixel"));++motionSamples;}}
        }
        const auto& palette=expected.at("palette_orig");require(preset.originalPalette.name==palette.at("name").get<std::string>()&&preset.originalPalette.index==palette.at("index"),"Preset palette identity differs");
        floats(preset.originalPalette.rgba,palette.at("palette"));floats(preset.expandedPalette,expected.at("palette_expanded"));
        for(unsigned c=0;c<3;++c)parameterError=std::max(parameterError,numeric(preset.originalPalette.background[c],palette.at("bkg").at(c),1e-9,"palette companion"));++snapshots;
    }
    void roundtrip(Preset& preset){
        auto original=preset.warps;const auto before=encode(preset.wave);preset.rebuildId=19;preset.releaseMaps();
        size_t descriptors=0;for(const auto& warp:original)descriptors+=warp.map.dynamic.size()*sizeof(float);require(preset.mapBytes()==descriptors,"Released static map allocation retained");
        preset.rebuildMaps(true);require(preset.mapsReady&&preset.rebuildId==19,"Worker rebuild token changed");
        for(size_t i=0;i<original.size();++i){require(preset.warps[i].map.displacement==original[i].map.displacement&&preset.warps[i].map.dynamic==original[i].map.dynamic,"Rebuilt map differs");
            require(preset.warps[i].starsFollowMotion==original[i].starsFollowMotion,"Rebuild lost star motion choice");}
        require(encode(preset.wave)==before,"Map rebuild overwrote waveform settings");preset.releaseMaps();preset.rebuildMaps(false);require(preset.mapsReady&&preset.rebuildId==-1,"Foreground rebuild did not invalidate token");++rebuilds;
    }
};
int main(int argc,char** argv){json report;fs::path output;std::string current;
    try{
        require(argc==3,"Expected preset fixture manifest and result path");const fs::path file=argv[1];output=argv[2];const auto manifest=read(file);const auto pixels=manifest.at("sample_pixels").get<std::vector<size_t>>();
        require(pixels.size()<=4096&&manifest.at("cases").size()<=256,"Preset reference list exceeds bound");PaletteCatalog catalog;Comparison compared;
        for(const auto& item:manifest.at("cases")){current=item.at("reference");const auto states=read(file.parent_path()/current);Mulberry32 generator(item.at("seed"));size_t calls=0;auto random=[&]{++calls;return generator();};
            auto preset=Preset::generate(item.at("number"),item.at("t1"),item.at("t2"),catalog,random,item.at("mode"));
            for(const auto& state:states){const auto operation=state.at("operation").get<std::string>();
                if(operation=="wave")preset.randomizeWave(random);else if(operation=="dots")preset.randomizeDots(random);else if(operation=="shader")preset.randomizeShader(random);
                else if(operation=="palette")preset.randomizePalette(catalog,random);else if(operation=="slot")preset.setWarpSlot(state.at("slot"),state.at("mode"),random);
                else if(operation=="motion")preset.randomizeMotion(random);else require(operation=="generate","Unknown fixture operation");
                compared.check(preset,calls,state.at("state"),pixels);
            }
            compared.roundtrip(preset);
        }
        current="invalid and cancelled requests";
        rejects([&]{Preset::generate(0,2,1,catalog,[]{return .5;});});rejects([&]{Preset::generate(0,0,1,catalog,[]{return 1.;});});rejects([&]{Preset::generate(0,0,1,catalog,[]{return .5;},42);});
        std::stop_source stop;stop.request_stop();rejects([&]{Preset::generate(0,0,1,catalog,[]{return .5;},-1,stop.get_token());});
        auto preset=Preset::generate(0,0,1,catalog,[]{return .5;},35);auto original=preset.warps;const auto originalMotion=encode(preset.motion);
        rejects([&]{preset.randomizeMotion([]{return .5;},-1,stop.get_token());});require(preset.mapsReady&&preset.warps[0].map.dynamic==original[0].map.dynamic&&encode(preset.motion)==originalMotion,"Cancelled motion changed preset");
        rejects([&]{preset.setWarpSlot(4,0,[]{return .5;});});for(auto& warp:preset.warps)warp.strength=0;rejects([&]{preset.blendedPreferences(true);});
        report={{"result","pass"},{"preset_cases",manifest.at("cases").size()},{"snapshots_compared",compared.snapshots},{"parameter_components",compared.scalars},{"motion_samples_compared",compared.motionSamples},
            {"palette_components_compared",compared.paletteComponents},{"release_rebuild_roundtrips",compared.rebuilds},{"maximum_parameter_error",compared.parameterError},{"maximum_sampled_map_error",compared.mapError},
            {"maximum_palette_error",compared.paletteError},{"invalid_and_cancelled_requests","rejected"}};
        std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
