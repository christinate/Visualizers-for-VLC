// SPDX-License-Identifier: GPL-2.0-or-later
#include "node_api.h"
#include "shared_audio.hpp"
#include <unordered_map>
#include <memory>
#include <chrono>
#include <cstring>
static std::unordered_map<unsigned,std::unique_ptr<suite::SharedAudio>> readers,writers;
static uint64_t nowMs(){
#ifdef _WIN32
    return GetTickCount64();
#else
    return suite::milliseconds();
#endif
}
static napi_value jsBool(napi_env env,bool value){napi_value result;napi_get_boolean(env,value,&result);return result;}
static bool number(napi_env env,napi_value value,unsigned& result){uint32_t n=0;if(napi_get_value_uint32(env,value,&n)!=napi_ok||!n)return false;result=n;return true;}
static bool floats(napi_env env,napi_value value,float*& data,size_t& length){napi_typedarray_type type;void* pointer=nullptr;napi_value buffer;size_t offset=0;if(napi_get_typedarray_info(env,value,&type,&length,&pointer,&buffer,&offset)!=napi_ok||type!=napi_float32_array||!pointer)return false;data=static_cast<float*>(pointer);return true;}
static napi_value readAudio(napi_env env,napi_callback_info info){
    size_t count=2;napi_value args[2];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);unsigned pid=0;float* output=nullptr;size_t length=0;
    if(count!=2||!number(env,args[0],pid)||!floats(env,args[1],output,length)||length<4||length%2||length>suite::ringFrames*2){napi_throw_type_error(env,nullptr,"readAudio(pid, stereoFloat32Array)");return nullptr;}
    std::fill(output,output+length,0.f);
    auto& reader=readers[pid];if(!reader||!reader->ring)reader=std::make_unique<suite::SharedAudio>(pid,false);
    if(!reader->ring||!reader->lock())return jsBool(env,false);
    struct Unlock {suite::SharedAudio* value;~Unlock(){value->unlock();}} unlock{reader.get()};
    const auto& ring=*reader->ring;const auto now=nowMs();
    if(ring.magic!=suite::audioMagic||ring.version!=1||!ring.rate||now<ring.updatedMs||now-ring.updatedMs>250)return jsBool(env,false);
    const size_t frames=length/2;
    for(size_t n=0;n<frames;++n){double age=(frames-1-n)*ring.rate/44100.;auto whole=uint64_t(age);float fraction=float(age-whole);
        if(ring.written>whole+1&&whole+1<suite::ringFrames){auto position=ring.written-1-whole;for(unsigned ch=0;ch<2;++ch)output[n*2+ch]=suite::clean(ring.samples[position%suite::ringFrames][ch]*(1-fraction)+ring.samples[(position-1)%suite::ringFrames][ch]*fraction);}}
    return jsBool(env,true);
}
static napi_value writeAudio(napi_env env,napi_callback_info info){
    size_t count=4;napi_value args[4];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);unsigned pid=0,rate=0,channels=0;float* input=nullptr;size_t length=0;
    if(count!=4||!number(env,args[0],pid)||!floats(env,args[1],input,length)||!number(env,args[2],rate)||!number(env,args[3],channels)||channels>32||rate>768000||length%channels||length/channels>suite::ringFrames){napi_throw_type_error(env,nullptr,"writeAudio(pid, float32Array, sampleRate, channels)");return nullptr;}
    auto& writer=writers[pid];if(!writer)writer=std::make_unique<suite::SharedAudio>(pid,true);if(!writer->ring||!writer->lock())return jsBool(env,false);
    suite::write(*writer->ring,input,length/channels,channels,rate,nowMs());writer->unlock();return jsBool(env,true);
}
static napi_value release(napi_env env,napi_callback_info info){
    size_t count=1;napi_value arg;napi_get_cb_info(env,info,&count,&arg,nullptr,nullptr);unsigned pid=0;
    if(count==1&&number(env,arg,pid)){readers.erase(pid);
        if(writers.erase(pid)){
#ifndef _WIN32
            auto file=suite::audioDirectory()/(std::to_string(getuid())+"-"+std::to_string(pid)+".pcm");
            ::unlink(file.c_str());suite::ipcLifetime().files.erase(file);
#endif
        }}
    return jsBool(env,true);
}
static void cleanup(void*){readers.clear();writers.clear();}
template<napi_value(*Function)(napi_env,napi_callback_info)> static napi_value guarded(napi_env env,napi_callback_info info){try{return Function(env,info);}catch(const std::exception& error){napi_throw_error(env,nullptr,error.what());return nullptr;}catch(...){napi_throw_error(env,nullptr,"Native audio operation failed");return nullptr;}}
static napi_value initialize(napi_env env,napi_value exports){
    napi_property_descriptor methods[]={{"readAudio",nullptr,guarded<readAudio>,nullptr,nullptr,nullptr,napi_default,nullptr},{"writeAudio",nullptr,guarded<writeAudio>,nullptr,nullptr,nullptr,napi_default,nullptr},{"release",nullptr,guarded<release>,nullptr,nullptr,nullptr,napi_default,nullptr}};
    napi_define_properties(env,exports,3,methods);napi_add_env_cleanup_hook(env,cleanup,nullptr);return exports;
}
NAPI_MODULE(visualizer_audio,initialize)


