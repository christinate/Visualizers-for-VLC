// SPDX-License-Identifier: GPL-2.0-or-later
#include <miniaudio.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Converter {ma_data_converter value{};bool ready=false;~Converter(){if(ready)ma_data_converter_uninit(&value,nullptr);}};
static double magnitude(const std::vector<float>& samples,unsigned channel,double frequency){
    double real=0,imaginary=0;
    for(size_t n=512;n<samples.size()/2;++n){const double angle=2*std::numbers::pi*frequency*double(n)/44100.;
        real+=samples[n*2+channel]*std::cos(angle);imaginary+=samples[n*2+channel]*std::sin(angle);}
    return std::hypot(real,imaginary)/double(samples.size()/2-512);
}
static void check(ma_format format,unsigned channels,unsigned rate,bool silence){
    const size_t frameCount=rate/4;std::vector<float> input(frameCount*channels);
    for(size_t n=0;n<frameCount&&!silence;++n){input[n*channels]=float(.3*std::sin(2*std::numbers::pi*220*double(n)/rate));
        if(channels>1)input[n*channels+1]=float(.3*std::sin(2*std::numbers::pi*440*double(n)/rate));}
    const unsigned bytesPerFrame=ma_get_bytes_per_frame(format,channels);std::vector<ma_uint8> encoded(frameCount*bytesPerFrame);
    ma_convert_pcm_frames_format(encoded.data(),format,input.data(),ma_format_f32,frameCount,channels,ma_dither_mode_none);
    if(silence)ma_silence_pcm_frames(encoded.data(),frameCount,format,channels);
    auto config=ma_data_converter_config_init(format,ma_format_f32,channels,2,rate,44100);config.resampling.linear.lpfOrder=4;
    Converter converter;require(ma_data_converter_init(&config,nullptr,&converter.value)==MA_SUCCESS,"Capture converter initialization failed");converter.ready=true;
    std::vector<float> output;size_t cursor=0;unsigned calls=0;
    while(cursor<frameCount){
        ma_uint64 inFrames=std::min<size_t>(127,frameCount-cursor),outFrames=512;float block[1024]{};
        require(ma_data_converter_process_pcm_frames(&converter.value,encoded.data()+cursor*bytesPerFrame,&inFrames,block,&outFrames)==MA_SUCCESS,"Capture conversion failed");
        require((inFrames||outFrames)&&inFrames<=127&&outFrames<=512,"Capture conversion made invalid progress");
        cursor+=size_t(inFrames);output.insert(output.end(),block,block+outFrames*2);
        require(++calls<frameCount*2,"Capture conversion did not terminate");
    }
    require(std::abs(double(output.size()/2)-11025)<64,"Capture resampling changed duration unexpectedly");
    for(float value:output)require(std::isfinite(value),"Capture conversion produced a nonfinite value");
    if(silence){
        // miniaudio maps unsigned PCM across [0,255] to [-1,1], whose midpoint
        // is 127.5. Permit one u8 quantization step per contributing channel;
        // signed PCM and float silence must stay exactly zero.
        const float tolerance=format==ma_format_u8?float(channels)*2.f/255:0.f;
        require(std::all_of(output.begin(),output.end(),[&](float value){return std::abs(value)<=tolerance;}),"Silence exceeded its format's quantization bound");return;
    }
    const double left220=magnitude(output,0,220),left440=magnitude(output,0,440),right220=magnitude(output,1,220),right440=magnitude(output,1,440);
    require(left220>.02&&left220>left440*15,"Left-channel pitch or channel mapping changed");
    if(channels==1){require(right220>.02&&right220>right440*15,"Mono expansion lost the source tone");
        for(size_t n=0;n<output.size();n+=2)require(output[n]==output[n+1],"Mono channels differ after stereo expansion");}
    else require(right440>.02&&right440>right220*15,"Right-channel pitch or channel mapping changed");
}
int main(){try{
    unsigned cases=0;for(auto format:{ma_format_u8,ma_format_s16,ma_format_s24,ma_format_s32,ma_format_f32})
        for(unsigned channels:{1u,2u,6u,8u})for(unsigned rate:{22050u,44100u,48000u,96000u,192000u})
            for(bool silence:{false,true}){try{check(format,channels,rate,silence);++cases;}
                catch(const std::exception& error){throw std::runtime_error(std::string(error.what())+" (format "+std::to_string(format)+", channels "+std::to_string(channels)+", rate "+std::to_string(rate)+", silence "+std::to_string(silence)+")");}}
    std::cout<<"Capture conversion: "<<cases<<" streaming tone/silence cases pass; 5 PCM formats, 4 channel layouts, 5 sample rates\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
