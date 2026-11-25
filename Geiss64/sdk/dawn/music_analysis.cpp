// SPDX-License-Identifier: GPL-2.0-or-later
#include "music_analysis.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace visualizers {
namespace {
constexpr unsigned samples=2048,firstBassBin=2,lastBassBin=9;
constexpr double energyScale=1048576;
double hann(unsigned n){return .5-.5*std::cos(double(n)*std::numbers::pi*2/(samples-1));}
}

BeatState BeatDetector::update(double bass,double time){
    const double dt=std::clamp(time-lastTime_,.001,.1);lastTime_=time;
    const double baseline=average_;average_+=(bass-average_)*(1-std::exp(-dt/1.5));
    const bool detected=bass>std::max(.00003,baseline*1.55)&&time-lastBeat_>.28;
    if(detected){lastBeat_=time;++beats_;envelope_=1;}
    envelope_*=std::exp(-dt*4);
    return {beats_,envelope_,detected};
}
BeatState BeatDetector::state(double silenceSeconds)const{
    return {beats_,envelope_*std::exp(-std::max(0.,silenceSeconds)*4),false};
}
void BeatDetector::restart(double time){
    average_=.001;lastBeat_=time-10;lastTime_=time;envelope_=0;
}

double estimateBass(std::span<const float,4096> stereo){
    struct Basis {
        std::array<float,samples> real{},imaginary{};
    };
    // For intermediate player windows we only need the eight detector bins.
    // Precompute their Hann-weighted basis rather than running another FFT.
    static const auto bases=[]{
        std::array<Basis,lastBassBin-firstBassBin+1> out{};
        for(unsigned bin=firstBassBin;bin<=lastBassBin;++bin){
            auto& basis=out[bin-firstBassBin];
            for(unsigned n=0;n<samples;++n){
                const double angle=double(bin)*n*std::numbers::pi*2/samples;
                basis.real[n]=float(hann(n)*std::cos(angle));
                basis.imaginary[n]=float(hann(n)*std::sin(angle));
            }
        }
        return out;
    }();
    std::array<float,samples> mono{};
    for(unsigned n=0;n<samples;++n)mono[n]=(stereo[n*2]+stereo[n*2+1])*.5f;
    double bass=0;
    for(const auto& basis:bases){
        double real=0,imaginary=0;
        for(unsigned n=0;n<samples;++n){
            real+=double(mono[n])*basis.real[n];
            imaginary+=double(mono[n])*basis.imaginary[n];
        }
        bass+=(real*real+imaginary*imaginary)/energyScale;
    }
    return bass;
}

const MusicFrame& MusicAnalysis::update(std::span<const float> stereo,double time,const BeatState* playerBeat){
    if(stereo.size()!=4096||!std::isfinite(time)||time<0)throw std::runtime_error("Invalid music analysis window or timestamp");
    double squares=0,energy=0,previous=0;
    for(size_t n=0;n<2048;++n){
        const double left=std::isfinite(stereo[n*2])?stereo[n*2]:0;
        const double right=std::isfinite(stereo[n*2+1])?stereo[n*2+1]:0;
        const double sample=(left+right)*.5;
        result_.wave[n]=float(sample);squares+=sample*sample;energy+=std::abs(sample-previous);previous=sample;
        real_[n]=float(sample*hann(unsigned(n)));imag_[n]=0;
    }
    for(size_t n=1,j=0;n<2048;++n){
        size_t bit=1024;for(;j&bit;bit>>=1)j^=bit;j^=bit;
        if(n<j){std::swap(real_[n],real_[j]);std::swap(imag_[n],imag_[j]);}
    }
    for(size_t size=2;size<=2048;size*=2){
        const double angle=-std::numbers::pi*2/double(size);
        for(size_t start=0;start<2048;start+=size)for(size_t j=0;j<size/2;++j){
            const double c=std::cos(angle*double(j)),s=std::sin(angle*double(j));
            const auto a=start+j,b=a+size/2;
            const double real=double(real_[b])*c-double(imag_[b])*s;
            const double imag=double(real_[b])*s+double(imag_[b])*c;
            real_[b]=float(double(real_[a])-real);imag_[b]=float(double(imag_[a])-imag);
            real_[a]=float(double(real_[a])+real);imag_[a]=float(double(imag_[a])+imag);
        }
    }
    double bass=0;
    for(size_t n=0;n<1024;++n){
        const double value=(double(real_[n])*real_[n]+double(imag_[n])*imag_[n])/energyScale;
        result_.spectrum[n]=float(value*(double(n)*44100/2048));
        if(n>=firstBassBin&&n<=lastBassBin)bass+=value;
    }
    const auto beat=playerBeat?*playerBeat:beats_.update(bass,time);
    result_.rms=std::sqrt(squares/2048);result_.visualEnergy=energy/2047;
    result_.bass=bass;result_.beat=beat.detected;result_.beatCount=beat.count;result_.beatEnvelope=beat.envelope;
    return result_;
}
}
