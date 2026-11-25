// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_capture.hpp"
#include <miniaudio.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace visualizers {
#ifndef __APPLE__
CapturePermission capturePermission(){return CapturePermission::Granted;}
void requestCapturePermission(){}
#endif
const char* capturePermissionMessage(CapturePermission value)noexcept{
    switch(value){
        case CapturePermission::Granted:return "Recording permission granted";
        case CapturePermission::NotDetermined:return "Microphone permission has not been requested";
        case CapturePermission::Pending:return "Waiting for microphone permission; respond to the macOS permission dialog";
        case CapturePermission::Denied:return "Microphone access is denied; enable this app in System Settings > Privacy & Security > Microphone";
        case CapturePermission::Restricted:return "Microphone access is restricted by macOS policy";
        default:return "Microphone capture requires the native app bundle with its microphone usage description; device listing remains available";
    }
}
uint64_t audioMilliseconds()noexcept{
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
void AudioFifo::reset()noexcept{
    written_=0;consumed_=0;dropped_=0;publishedAt_=0;received_=0;
    historyHead_=0;historyFrames_=0;lastStamp_=0;history_.fill(0);
}
void AudioFifo::push(const float* stereo,uint32_t frames,uint64_t nowMs)noexcept{
    const auto written=written_.load(std::memory_order_relaxed),consumed=consumed_.load(std::memory_order_acquire);
    const auto count=std::min<uint64_t>(frames,capacity_-(written-consumed));
    for(uint64_t n=0;n<count;++n)for(unsigned channel=0;channel<2;++channel){
        float value=stereo?stereo[n*2+channel]:0;
        if(!std::isfinite(value))value=0;
        data_[((written+n)%capacity_)*2+channel]=std::clamp(value,-32.f,32.f);
    }
    if(count){publishedAt_.store(nowMs,std::memory_order_relaxed);written_.store(written+count,std::memory_order_release);}
    received_.fetch_add(frames,std::memory_order_relaxed);dropped_.fetch_add(frames-count,std::memory_order_relaxed);
}
unsigned AudioFifo::window(std::span<float,4096> stereo,uint64_t nowMs)noexcept{
    const auto consumed=consumed_.load(std::memory_order_relaxed),written=written_.load(std::memory_order_acquire);
    const auto count=written-consumed;
    for(uint64_t n=0;n<count;++n){
        history_[historyHead_*2]=data_[((consumed+n)%capacity_)*2];history_[historyHead_*2+1]=data_[((consumed+n)%capacity_)*2+1];
        historyHead_=(historyHead_+1)%2048;historyFrames_=std::min(2048u,historyFrames_+1);
    }
    if(count){lastStamp_=publishedAt_.load(std::memory_order_relaxed);consumed_.store(written,std::memory_order_release);}
    if(!lastStamp_||nowMs<lastStamp_||nowMs-lastStamp_>250){std::fill(stereo.begin(),stereo.end(),0.f);return unsigned(count);}
    const unsigned missing=2048-historyFrames_;std::fill_n(stereo.begin(),missing*2,0.f);
    for(unsigned n=0;n<historyFrames_;++n){
        const unsigned source=(historyHead_+2048-historyFrames_+n)%2048;
        stereo[(missing+n)*2]=history_[source*2];stereo[(missing+n)*2+1]=history_[source*2+1];
    }
    return unsigned(count);
}
unsigned AudioFifo::drain(std::span<float> stereo,uint64_t nowMs)noexcept{
    if(stereo.empty()||stereo.size()%2)return 0;
    const auto consumed=consumed_.load(std::memory_order_relaxed),written=written_.load(std::memory_order_acquire);
    if(consumed==written)return 0;
    const auto stamp=publishedAt_.load(std::memory_order_relaxed);
    if(!stamp||nowMs<stamp||nowMs-stamp>250){
        consumed_.store(written,std::memory_order_release);historyHead_=historyFrames_=0;lastStamp_=0;history_.fill(0);return 0;
    }
    const auto count=std::min<uint64_t>(written-consumed,stereo.size()/2);
    for(uint64_t n=0;n<count;++n){
        const auto index=((consumed+n)%capacity_)*2;
        stereo[n*2]=data_[index];stereo[n*2+1]=data_[index+1];
        history_[historyHead_*2]=stereo[n*2];history_[historyHead_*2+1]=stereo[n*2+1];
        historyHead_=(historyHead_+1)%2048;historyFrames_=std::min(2048u,historyFrames_+1);
    }
    lastStamp_=stamp;consumed_.store(consumed+count,std::memory_order_release);return unsigned(count);
}
namespace {
void check(ma_result result,const char* operation){
    if(result!=MA_SUCCESS)throw std::runtime_error(std::string(operation)+": "+ma_result_description(result));
}
std::string text(const char* value,size_t capacity){
    size_t n=0;while(n<capacity&&value[n])++n;
    if(n==capacity)throw std::runtime_error("Unterminated audio device identifier");
    return std::string(value,n);
}
std::string stableId(ma_backend backend,const ma_device_id& id){
    std::string value;
    switch(backend){
#ifdef _WIN32
        case ma_backend_wasapi:{
            const auto* wide=reinterpret_cast<const wchar_t*>(id.wasapi);size_t n=0;while(n<64&&wide[n])++n;
            if(n==64)throw std::runtime_error("Invalid WASAPI endpoint identifier");
            const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,int(n),nullptr,0,nullptr,nullptr);
            if(size<=0)throw std::runtime_error("Cannot encode WASAPI endpoint identifier");
            value.resize(size);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,int(n),value.data(),size,nullptr,nullptr);break;
        }
#endif
        case ma_backend_coreaudio:value=text(id.coreaudio,sizeof(id.coreaudio));break;
        case ma_backend_pulseaudio:value=text(id.pulse,sizeof(id.pulse));break;
        case ma_backend_alsa:value=text(id.alsa,sizeof(id.alsa));break;
        default:throw std::runtime_error("Audio backend has no supported stable identifier");
    }
    return std::string(ma_get_backend_name(backend))+":"+value;
}
bool equalLabel(const std::string& a,const std::string& b){
#ifdef _WIN32
    auto wide=[](const std::string& input){int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),int(input.size()),nullptr,0);
        std::wstring result(size>0?size:0,L'\0');if(size>0)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),int(input.size()),result.data(),size);return result;};
    const auto left=wide(a),right=wide(b);
    return !left.empty()&&!right.empty()&&CompareStringOrdinal(left.data(),int(left.size()),right.data(),int(right.size()),TRUE)==CSTR_EQUAL;
#else
    if(a.size()!=b.size())return false;
    for(size_t n=0;n<a.size();++n){unsigned char x=a[n],y=b[n];if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}
    return true;
#endif
}
void permissionPreflight(){
    const auto permission=capturePermission();
    if(permission!=CapturePermission::Granted)throw std::runtime_error(capturePermissionMessage(permission));
}
}
std::string resolveAudioInput(const std::vector<InputDevice>& devices,const std::string& token){
    if(devices.size()>1024)throw std::runtime_error("Recording device list exceeds its limit");
    if(token.empty()||token.size()>4096||token.find('\0')!=std::string::npos)throw std::runtime_error("Invalid recording input selection");
    if(equalLabel(token,"default"))return "default";
    const InputDevice* found=nullptr;
    for(const auto& device:devices)if(device.id==token){
        if(found)throw std::runtime_error("Recording input identifiers are duplicated");found=&device;}
    if(found)return found->id;
    for(const auto& device:devices)if(equalLabel(device.label,token)){
        if(found)throw std::runtime_error("Recording device label is ambiguous; choose its device ID");found=&device;}
    if(!found)throw std::runtime_error("Recording device is missing; choose an available input");
    return found->id;
}
struct AudioCapture::Impl {
    ma_context context{};ma_device device{};
    bool contextReady=false,deviceReady=false;
    AudioFifo fifo;
    std::string selected;
    struct Device {InputDevice description;ma_device_id id;};
    std::vector<Device> enumerated;
    Impl(){
#ifdef _WIN32
        const ma_backend backends[]={ma_backend_wasapi};
#elif defined(__APPLE__)
        const ma_backend backends[]={ma_backend_coreaudio};
#else
        const ma_backend backends[]={ma_backend_pulseaudio,ma_backend_alsa};
#endif
        check(ma_context_init(backends,sizeof(backends)/sizeof(backends[0]),nullptr,&context),"Cannot initialize native recording");contextReady=true;
    }
    ~Impl(){stop();if(contextReady)ma_context_uninit(&context);}
    void stop()noexcept{if(deviceReady){ma_device_uninit(&device);deviceReady=false;}selected.clear();fifo.reset();}
    std::vector<InputDevice> devices(){
        ma_device_info* inputs=nullptr;ma_uint32 count=0;
        check(ma_context_get_devices(&context,nullptr,nullptr,&inputs,&count),"Cannot list recording inputs");
        if(count>1024)throw std::runtime_error("Recording device list exceeds its limit");
        enumerated.clear();std::vector<InputDevice> result;
        for(ma_uint32 n=0;n<count;++n){InputDevice description{stableId(context.backend,inputs[n].id),text(inputs[n].name,sizeof(inputs[n].name)),inputs[n].isDefault!=0};
            enumerated.push_back({description,inputs[n].id});result.push_back(std::move(description));}
        return result;
    }
    static void data(ma_device* device,void*,const void* input,ma_uint32 frames){
        static_cast<Impl*>(device->pUserData)->fifo.push(static_cast<const float*>(input),frames,audioMilliseconds());
    }
    void start(const std::string& token){
        const auto canonical=resolveAudioInput(devices(),token);permissionPreflight();
        ma_device_id chosen{};bool useDefault=canonical=="default";std::string name="Default recording input";
        if(!useDefault){
            const Device* found=nullptr;
            for(const auto& value:enumerated)if(value.description.id==canonical){found=&value;break;}
            if(!found)throw std::runtime_error("Recording device is missing; choose an available input");
            chosen=found->id;name=found->description.label;
        }
        stop();auto config=ma_device_config_init(ma_device_type_capture);
        config.capture.pDeviceID=useDefault?nullptr:&chosen;config.capture.format=ma_format_f32;config.capture.channels=2;
        config.sampleRate=44100;config.periodSizeInFrames=512;config.dataCallback=data;config.pUserData=this;
        check(ma_device_init(&context,&config,&device),"Cannot open recording input");deviceReady=true;
        try{check(ma_device_start(&device),"Cannot start recording input (check microphone permission)");selected=text(device.capture.name,sizeof(device.capture.name));if(selected.empty())selected=name;}
        catch(...){stop();throw;}
    }
};
AudioCapture::AudioCapture():impl_(std::make_unique<Impl>()){}
AudioCapture::~AudioCapture()=default;
std::vector<InputDevice> AudioCapture::devices(){return impl_->devices();}
std::string AudioCapture::resolveInput(const std::string& token){return resolveAudioInput(impl_->devices(),token);}
void AudioCapture::start(const std::string& token){impl_->start(token);}
void AudioCapture::stop()noexcept{impl_->stop();}
unsigned AudioCapture::window(std::span<float,4096> stereo){return impl_->fifo.window(stereo,audioMilliseconds());}
unsigned AudioCapture::drain(std::span<float> stereo){return impl_->fifo.drain(stereo,audioMilliseconds());}
CaptureStatus AudioCapture::status()const{
    const auto& p=*impl_;CaptureStatus result;result.backend=ma_get_backend_name(p.context.backend);result.device=p.selected;
    result.running=p.deviceReady&&ma_device_is_started(&p.device);result.frames=p.fifo.received();result.dropped=p.fifo.dropped();
    if(p.deviceReady){result.nativeRate=p.device.capture.internalSampleRate;result.nativeChannels=p.device.capture.internalChannels;}
    return result;
}
}
