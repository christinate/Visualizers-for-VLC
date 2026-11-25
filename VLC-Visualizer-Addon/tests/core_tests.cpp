#include "audio.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
static void check(bool condition,const char* message){if(!condition){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
    suite::AudioRing ring{};float mono[1024];for(size_t i=0;i<1024;++i)mono[i]=.7f*std::sin(float(i)*.1f);
    suite::write(ring,mono,1024,1,48000,1000);check(ring.written==1024&&ring.samples[10][0]==ring.samples[10][1],"Mono must duplicate into stereo");
    uint8_t wave[2][576],spectrum[2][576];suite::analyze(ring,1000,wave,spectrum);check(wave[0][500]==wave[1][500],"Stereo timeline mismatch");check(*std::max_element(&spectrum[0][0],&spectrum[0][0]+1152)>0,"FFT must detect the signal");
    suite::analyze(ring,1300,wave,spectrum);check(std::all_of(&wave[0][0],&wave[0][0]+1152,[](auto v){return v==0;}),"Paused/stopped input must become silence");
    check(suite::waveByte(0)==0&&suite::waveByte(1)==127&&suite::waveByte(-1)==129,"Waveform must use signed Winamp encoding");
    check(suite::clean(std::numeric_limits<float>::quiet_NaN())==0,"Nonfinite PCM must be sanitized");
    float pcm[12]={1,-1,1,1,1,1,1,-1,1,1,1,1};suite::write(ring,pcm,2,6,96000,1400);check(ring.samples[1024][0]<=1&&ring.samples[1024][1]>=-1,"Multichannel fold must retain headroom");
    for(int n=0;n<20;++n)suite::write(ring,mono,1024,1,192000,1500);check(ring.written>suite::ringFrames,"Ring must wrap");suite::analyze(ring,1500,wave,spectrum);
    std::cout<<"Audio conversion, FFT, resampling, silence, finite input, and ring wrap passed\n";
}
