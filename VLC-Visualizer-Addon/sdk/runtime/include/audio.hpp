#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
namespace suite {
constexpr uint32_t audioMagic = 0x31565A56;
constexpr unsigned ringFrames = 16384;
// Fixed-width, pointer-free layout shared between the native x64 module and host.
struct AudioRing {
    uint32_t magic, version, rate, channels;
    uint64_t written, updatedMs;
    float samples[ringFrames][2];
};
static_assert(sizeof(AudioRing)==32+ringFrames*2*sizeof(float),"Audio IPC layout changed");
inline float clean(float v) { return std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
inline std::array<float,2> stereo(const float* v, unsigned channels) {
    if (!channels) return {0,0};
    if (channels == 1) return {clean(v[0]),clean(v[0])};
    // VLC native order: FL FR ...; fold additional channels equally with headroom.
    float l=clean(v[0]), r=clean(v[1]), weight=1.0f;
    for (unsigned c=2;c<channels;++c) { float x=clean(v[c])*0.5f; l+=x;r+=x;weight+=0.5f; }
    return {l/weight,r/weight};
}
inline void write(AudioRing& ring, const float* pcm, size_t frames, unsigned channels, unsigned rate, uint64_t now) {
    if (!channels || !rate) return;
    ring.magic=audioMagic;ring.version=1;ring.rate=rate;ring.channels=2;
    // Keep only the newest ring if VLC hands us a very large block.
    size_t start = frames>ringFrames ? frames-ringFrames : 0;
    ring.written += start;
    for (size_t n=start;n<frames;++n) {
        const auto s=stereo(pcm+n*channels,channels);
        auto& out=ring.samples[ring.written++ % ringFrames];out[0]=s[0];out[1]=s[1];
    }
    ring.updatedMs=now;
}
inline uint8_t waveByte(float v) { return static_cast<uint8_t>(static_cast<int8_t>(std::lround(clean(v)*127.0f))); }
// Winamp supplies 576 signed 8-bit waveform samples at 44.1 kHz.
// Interpolate to that timeline, including for 48/96/192 kHz VLC input.
inline void analyze(const AudioRing& ring, uint64_t now, uint8_t wave[2][576], uint8_t spectrum[2][576]) {
    for(unsigned ch=0;ch<2;++ch){std::fill(wave[ch],wave[ch]+576,0);std::fill(spectrum[ch],spectrum[ch]+576,0);}
    if (ring.magic != audioMagic || ring.version != 1 || !ring.rate || now<ring.updatedMs || now-ring.updatedMs>250) return;
    constexpr size_t N=1024;
    for (unsigned ch=0;ch<2;++ch) {
        std::array<std::complex<float>,N> fft{};
        for(size_t i=0;i<N;++i) {
            double age=(N-1-i)*ring.rate/44100.0;
            auto whole=static_cast<uint64_t>(age);float f=float(age-whole);
            float v=0;
            if (ring.written>whole+1 && whole+1<ringFrames) {
                uint64_t pos=ring.written-1-whole;
                v=ring.samples[pos%ringFrames][ch]*(1-f)+ring.samples[(pos-1)%ringFrames][ch]*f;
            }
            if(i>=N-576) wave[ch][i-(N-576)]=waveByte(v);
            fft[i]=v*(0.5f-0.5f*std::cos(float(6.283185307179586*i/(N-1))));
        }
        for(size_t i=1,j=0;i<N;++i) { size_t bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(fft[i],fft[j]); }
        for(size_t len=2;len<=N;len<<=1) {
            auto root=std::polar(1.0f,float(-6.283185307179586/len));
            for(size_t i=0;i<N;i+=len){std::complex<float>w(1,0);for(size_t j=0;j<len/2;++j){auto a=fft[i+j],b=fft[i+j+len/2]*w;fft[i+j]=a+b;fft[i+j+len/2]=a-b;w*=root;}}
        }
        for(size_t i=0;i<576;++i) spectrum[ch][i]=uint8_t(std::clamp(std::abs(fft[i*(N/2-1)/575])*2.0f,0.0f,255.0f));
    }
}
}
