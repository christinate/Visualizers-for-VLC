#pragma once
#include "catalog.hpp"
namespace native {
struct Settings {int width=640,height=480,fps=60;bool fullscreen=false,vsync=false;};
inline Settings readSettings(const fs::path& profile,const std::string& id){Settings s;auto file=profile/(id+".json");if(!fs::exists(file))return s;auto j=read(file);s.width=std::clamp(j.value("width",640),320,3840);s.height=std::clamp(j.value("height",480),240,2160);s.fps=std::clamp(j.value("fps",60),15,144);s.fullscreen=j.value("fullscreen",false);s.vsync=j.value("vsync",false);return s;}
inline void saveSettings(const fs::path& profile,const std::string& id,const Settings& s){write(profile/(id+".json"),{{"width",s.width},{"height",s.height},{"fps",s.fps},{"fullscreen",s.fullscreen}});}
bool editSettings(Settings& value,const std::string& title,int argc,char** argv);
}
