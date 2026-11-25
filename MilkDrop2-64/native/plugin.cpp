// SPDX-License-Identifier: BSD-3-Clause
// Native MilkDrop preset backend, approved for macOS/Linux; Windows retains
// the original Direct3D 9 renderer. projectM retains its LGPL notices.
#include "visualizer_api.h"
#include <projectM-4/projectM.h>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iostream>
struct Renderer {
    projectm_handle engine=nullptr;std::vector<std::string> presets;size_t index=0;bool locked=false,pending=false,hardCut=false;
    ~Renderer(){if(engine)projectm_destroy(engine);}
    void load(bool smooth){if(!presets.empty())projectm_load_preset_file(engine,presets[index].c_str(),smooth);}
};
static void* create(const VisualizerCreateInfo* info){
    if(!info||info->size<sizeof(*info)||info->api_version!=2)return nullptr;
    try{auto state=new Renderer;try{state->engine=projectm_create();if(!state->engine){delete state;return nullptr;}projectm_set_window_size(state->engine,info->width,info->height);projectm_set_mesh_size(state->engine,48,36);projectm_set_fps(state->engine,60);projectm_set_preset_duration(state->engine,30);
        auto folder=std::filesystem::path(info->assets_utf8)/"milkdrop2/presets";if(std::filesystem::exists(folder))for(const auto& entry:std::filesystem::directory_iterator(folder))if(entry.is_regular_file()){auto extension=entry.path().extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});if(extension==".milk")state->presets.push_back(entry.path().string());}
        auto textures=(std::filesystem::path(info->assets_utf8)/"milkdrop2/textures").string();auto presets=folder.string();const char* paths[]={textures.c_str(),presets.c_str()};projectm_set_texture_search_paths(state->engine,paths,2);
        projectm_set_preset_switch_requested_event_callback(state->engine,[](bool cut,void* data){auto r=static_cast<Renderer*>(data);r->pending=true;r->hardCut=cut;},state);
        projectm_set_preset_switch_failed_event_callback(state->engine,[](const char* file,const char* message,void*){std::cerr<<"Preset load failed: "<<(file?file:"")<<": "<<(message?message:"")<<'\n';},state);
        std::sort(state->presets.begin(),state->presets.end());state->load(false);return state;
    }catch(...){delete state;throw;}}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return nullptr;}
}
static void destroy(void* value){delete static_cast<Renderer*>(value);}
static void resize(void* value,int width,int height){projectm_set_window_size(static_cast<Renderer*>(value)->engine,width,height);}
static int render(void* value,const VisualizerAudio* audio){if(!audio||audio->size<sizeof(*audio)||audio->channels!=2)return 1;auto state=static_cast<Renderer*>(value);if(state->pending&&!state->locked&&!state->presets.empty()){state->pending=false;state->index=(state->index+1)%state->presets.size();state->load(!state->hardCut);}projectm_pcm_add_float(state->engine,audio->interleaved,audio->frames,PROJECTM_STEREO);projectm_opengl_render_frame(state->engine);return 0;}
static void key(void* value,int code,int down){if(!down)return;auto state=static_cast<Renderer*>(value);if(code=='l'||code=='L'){state->locked=!state->locked;projectm_set_preset_locked(state->engine,state->locked);}else if(!state->presets.empty()&&(code==' '||code=='n'||code=='N'||code==8)){state->index=(state->index+state->presets.size()+(code==8?-1:1))%state->presets.size();state->load(true);}}
static int keyState(void* value){return static_cast<Renderer*>(value)->locked?1:0;}
extern "C" __attribute__((visibility("default"))) const VisualizerApi* VisualizerGetApi(uint32_t version){static const VisualizerApi api{sizeof(VisualizerApi),2,"MilkDrop (projectM native backend)",create,destroy,resize,render,key,keyState};return version==2?&api:nullptr;}
