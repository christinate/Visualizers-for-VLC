#include "engine.h"
#include "visualizer_api.h"
#include "gl_present.hpp"
#include "audio.hpp"
#include <memory>
#include <cmath>
#include <iostream>
struct Renderer {native::PixelPresenter presenter;suite::AudioRing audio{};double previous=0;};
static void* create(const VisualizerCreateInfo* info){if(!info||info->size<sizeof(*info)||info->api_version!=2)return nullptr;try{if(!GeissInitialize())return nullptr;return new Renderer;}catch(const std::exception& e){GeissShutdown();std::cerr<<e.what()<<'\n';return nullptr;}}
static void destroy(void* renderer){delete static_cast<Renderer*>(renderer);GeissShutdown();}
static void resize(void*,int,int){}
static int render(void* instance,const VisualizerAudio* audio){
    auto& state=*static_cast<Renderer*>(instance);if(!audio||audio->size<sizeof(*audio)||audio->channels!=2)return 1;
    const auto now=uint64_t(audio->time_seconds*1000);suite::write(state.audio,audio->interleaved,audio->frames,audio->channels,audio->sample_rate,now);uint8_t wave[2][576],spectrum[2][576];suite::analyze(state.audio,now,wave,spectrum);int w,h;float fps=state.previous>0?float(1./std::max(.001,audio->time_seconds-state.previous)):60;state.previous=audio->time_seconds;auto pixels=GeissRender(wave,spectrum,fps,&w,&h);state.presenter.draw(pixels,w,h);return 0;
}
static void key(void*,int code,int down){if(down)GeissKey(code);}
static int keyState(void*){return GeissKeyState();}
extern "C" __attribute__((visibility("default"))) const VisualizerApi* VisualizerGetApi(uint32_t version){static const VisualizerApi api{sizeof(VisualizerApi),2,"Geiss Classic",create,destroy,resize,render,key,keyState};return version==2?&api:nullptr;}
