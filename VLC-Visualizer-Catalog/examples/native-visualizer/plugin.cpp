// SPDX-License-Identifier: MIT
#include "../visualizer_api.h"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cmath>
#include <new>
struct State {bool alternate=false;};
static void* create(const VisualizerCreateInfo* info){return info&&info->size>=sizeof(*info)&&info->api_version==2?new(std::nothrow) State:nullptr;}
static void destroy(void* state){delete static_cast<State*>(state);}
static void resize(void*,int width,int height){glViewport(0,0,width,height);}
static int render(void* state,const VisualizerAudio* audio){
    if(!audio||audio->size<sizeof(*audio)||!audio->interleaved||audio->channels!=2)return 1;
    float level=0;for(uint32_t n=0;n<audio->frames*audio->channels;++n)level=std::max(level,std::abs(audio->interleaved[n]));
    glClearColor(static_cast<State*>(state)->alternate?.2f:level,.1f,.25f+level*.5f,1);glClear(GL_COLOR_BUFFER_BIT);return 0;
}
static void key(void* state,int code,int down){if(down&&(code=='l'||code=='L'))static_cast<State*>(state)->alternate=!static_cast<State*>(state)->alternate;}
static int keyState(void* state){return static_cast<State*>(state)->alternate;}
extern "C" __attribute__((visibility("default"))) const VisualizerApi* VisualizerGetApi(uint32_t version){static const VisualizerApi api{sizeof(VisualizerApi),2,"Example native visualizer",create,destroy,resize,render,key,keyState};return version==2?&api:nullptr;}
