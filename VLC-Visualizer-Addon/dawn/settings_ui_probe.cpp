// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_picker.hpp"
#include "settings_ui.hpp"
#include <chrono>
#include <iostream>

using namespace visualizers;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    NativeUiSession ui;const auto values=normalizeSettings({{"cameraSeconds",45.123456789123},{"inputDevice","preserve:音声"},{"custom",{{"unknown",true}}}});auto keep=[](){return true;};
    SettingsAutomation untouched;const auto unedited=editNativeSettings(values,"terrain","Rhythm Terrain",keep,&untouched);
    require(unedited&&unedited->empty(),"Unedited settings lost precision or generated changes");
    const SettingsJson geissChanges={{"width",800},{"height",600},{"fps",75},{"fullscreen",true},{"hdr",true},{"vsync",true},{"peak",1200},{"engine","classic"},{"supersampling",2}};
    SettingsAutomation edited{false,geissChanges};const auto result=editNativeSettings(values,"geiss","Prism Flow",keep,&edited);
    require(result&&*result==geissChanges,"Native display/renderer controls did not round-trip");
    const SettingsJson terrainChanges={{"cameraSeconds",50.25},{"colorBeats",8}};SettingsAutomation terrain{false,terrainChanges};
    const auto camera=editNativeSettings(values,"terrain","Rhythm Terrain",keep,&terrain);require(camera&&*camera==terrainChanges,"Native Terrain controls did not round-trip");
    auto beach=values;beach["sceneIni"]="[rain]\ndrop_height=18\n";
    const SettingsJson changed={{"sceneIni","[rain]\ndrop_height=28\n[overhead_shore]\nenabled=false\n"}};
    SettingsAutomation sceneEdit{false,changed};
    const auto scene=editNativeSettings(beach,"beach","Beach",keep,&sceneEdit);
    require(scene&&*scene==changed,"Multiline INI editor did not round-trip");
    SettingsAutomation cancelled{true,{{"width",800}}};require(!editNativeSettings(values,"milkdrop","MilkDrop 2",keep,&cancelled),"Cancelled settings were accepted");
    unsigned ticks=0;const auto start=std::chrono::steady_clock::now();require(!editNativeSettings(values,"terrain","Rhythm Terrain",[&](){return ++ticks<3;}),"Owner-close request was ignored");
    require(std::chrono::steady_clock::now()-start<std::chrono::seconds(3),"Settings owner closure was unresponsive");
    std::cout<<"Native settings controls: unchanged precision, display/HDR/engine/SSAA, Terrain, cancel and owner-close pass\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
