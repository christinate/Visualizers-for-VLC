// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR wave_align.js/resample.js, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#include "waveform.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace prismflow {namespace {
void validate(std::span<const float> input){if(input.size()>32768)throw std::invalid_argument("Waveform exceeds 32768 samples");for(float v:input)if(!std::isfinite(v)||std::abs(v)>16)throw std::invalid_argument("Invalid waveform sample");}
template<class T>using Pyramid=std::vector<std::vector<T>>;
Pyramid<float> pyramid(std::span<const float> input,unsigned levels){Pyramid<float> p(1);p[0].assign(input.begin(),input.end());for(unsigned n=0;n<levels;++n){const auto& last=p.back();std::vector<float> next(last.size()/2);for(size_t i=0;i<next.size();++i)next[i]=float((double(last[i*2])+last[i*2+1])*.5);p.push_back(std::move(next));}return p;}
Pyramid<uint8_t> weights(size_t count,unsigned levels){Pyramid<uint8_t> p(1);p[0].resize(count);for(size_t i=0;i<count;++i){const double t=(i+.5)/count,value=((1-std::abs(t-.5)*2)*5-2)*15;p[0][i]=uint8_t(std::clamp(value,0.,15.))&15;}
    for(unsigned n=0;n<levels;++n){const auto& last=p.back();std::vector<uint8_t> next(last.size()/2);for(size_t i=0;i<next.size();++i)next[i]=uint8_t((last[i*2]+last[i*2+1]+1)>>1)&15;p.push_back(std::move(next));}return p;}
double correlation(const std::vector<float>& s,const std::vector<float>& t,const std::vector<uint8_t>& w,size_t offset){double sum=0;for(size_t i=0;i<t.size();++i)sum+=double(s[offset+i])*t[i]*w[i];return sum;}
std::pair<size_t,double> bestOffset(const Pyramid<float>& s,const Pyramid<float>& t,const Pyramid<uint8_t>& w,size_t maximum){
    const auto level=t.size()-1;size_t best=0;double score=-std::numeric_limits<double>::infinity();for(size_t off=0;off<=s[level].size()-t[level].size();++off){const double value=correlation(s[level],t[level],w[level],off);if(value>score){score=value;best=off;}}
    for(size_t index=level;index>0;--index){const auto n=index-1,max=s[n].size()-t[n].size(),prediction=best*2;const auto a=prediction?prediction-1:0,b=std::min(prediction,max),c=std::min(prediction+1,max);size_t selected=b;double value=correlation(s[n],t[n],w[n],b);
        if(a!=b){const double v=correlation(s[n],t[n],w[n],a);if(v>value){value=v;selected=a;}}if(c!=b&&c!=a){const double v=correlation(s[n],t[n],w[n],c);if(v>value){value=v;selected=c;}}best=selected;score=value;
    }return {std::min(best,maximum),score};
}
}
std::vector<float> blurWave(std::span<const float> input,uint32_t radius){validate(input);const auto count=input.size();if(!radius||uint64_t(radius)*2+1>=count)return {input.begin(),input.end()};std::vector<float> result(count);double sum=0;for(size_t i=0;i<=radius;++i)sum+=input[i];result[0]=float(sum/(radius+1));
    for(size_t i=1;i<=radius;++i){sum+=input[i+radius];result[i]=float(sum/(i+radius+1));}const double inv=1./(radius*2+1);for(size_t i=radius+1;i<count-radius;++i){sum+=double(input[i+radius])-input[i-radius-1];result[i]=float(sum*inv);}for(size_t i=count-radius;i<count;++i){sum-=input[i-radius-1];result[i]=float(sum/(count-i+radius));}return result;
}
std::vector<float> resampleWave(std::span<const float> input,uint32_t count){validate(input);if(count>32768)throw std::invalid_argument("Resampled waveform exceeds 32768 samples");const auto n=input.size();if(count==n)return {input.begin(),input.end()};if(!count||!n)return {};if(n==1)return std::vector<float>(count,input[0]);const auto length=n-1;
    if(count==1){double area=0;for(size_t i=0;i<length;++i)area+=.5*(double(input[i])+input[i+1]);return {float(area/length)};}std::vector<float> out(count);const double scale=double(length)/(count-1);
    if(count>n){for(size_t i=0;i<count;++i){const double p=i*scale;const auto index=size_t(p);out[i]=index>=length?input[length]:float(double(input[index])+(double(input[index+1])-input[index])*(p-index));}return out;}
    std::vector<float> areas(n,0);for(size_t i=0;i<length;++i)areas[i+1]=float(double(areas[i])+.5*(double(input[i])+input[i+1]));
    const auto integral=[&](double x){if(x<=0)return 0.;if(x>=length)return double(areas[length]);const auto i=size_t(x);const double t=x-i;return double(areas[i])+double(input[i])*t+.5*(double(input[i+1])-input[i])*t*t;};double left=0;
    for(size_t i=0;i<count;++i){const double p=i*scale,right=i+1==count?double(length):.5*(p+(i+1)*scale);out[i]=float((integral(right)-integral(left))/(right-left));left=right;}return out;
}
std::vector<float> WaveAligner::align(std::span<const float> input,double fraction){
    validate(input);if(input.empty()||input.size()>4096||!std::isfinite(fraction)||fraction<=0||fraction>1)throw std::invalid_argument("Invalid waveform alignment window");const auto count=uint32_t(std::floor(input.size()*fraction));if(!count)throw std::invalid_argument("Waveform alignment window is empty");
    // Initialize lengths before computing offsets. The reference computes them
    // from its old input size, causing a wrong first/size-change window.
    if(original_!=input.size()||aligned_!=count){previous_.clear();original_=uint32_t(input.size());aligned_=count;}const auto maximum=original_-aligned_;
    if(previous_.empty()){const auto off=maximum/2;previous_.assign(input.begin()+off,input.begin()+off+count);return previous_;}
    unsigned levels=0;auto a=count;while(levels<4&&(a>>1)>=48&&(original_>>1)>(aligned_>>1)){++levels;a>>=1;}
    const auto target=pyramid(previous_,levels);const auto weight=weights(count,levels);std::vector<float> selected;double best=-std::numeric_limits<double>::infinity();
    for(unsigned flip=0;flip<4;++flip){std::vector<float> wave(input.size());for(size_t i=0;i<input.size();++i)wave[i]=float((flip>=2?-1.:1.)*input[flip&1?input.size()-1-i:i]);const auto source=pyramid(wave,levels);const auto [off,score]=bestOffset(source,target,weight,maximum);if(score>best){best=score;selected.assign(wave.begin()+off,wave.begin()+off+count);}}
    previous_=std::move(selected);return previous_;
}
}
