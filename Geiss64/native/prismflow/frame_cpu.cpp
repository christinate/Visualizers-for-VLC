// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR engine.js, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "frame_cpu.hpp"
#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace prismflow {namespace {
constexpr double motionScale=109.*.9*.94,pi=std::numbers::pi;
double draw(const RandomFunction& random){if(!random)throw std::invalid_argument("Frame needs a random source");const double v=random();if(!std::isfinite(v)||v<0||v>=1)throw std::invalid_argument("Invalid frame random value");return v;}
template<class T,size_t N>T blend(const T& a,const T& b,double t,const std::array<ParameterField<T>,N>& fields){T result;for(auto f:fields)result.*f.member=a.*f.member*(1-t)+b.*f.member*t;return result;}
struct Transform {double width,height,invX,invY;Transform(uint32_t w,uint32_t h):width(w),height(h){const double aspect=double(w)/h;invX=1./(aspect>1?2:2*aspect);invY=1./(aspect>1?2/aspect:2);}double x(double v)const{return (v*invX+.5)*width;}double y(double v)const{return (v*invY+.5)*height;}};
void point(FramePlan& p,double x,double y,double size,double alpha){if(p.points.size()>=32768*4)throw std::runtime_error("Frame exceeds its 32768-point budget");const std::array values={x,y,size,alpha};for(double v:values)if(!std::isfinite(v)||std::abs(v)>1e30)throw std::invalid_argument("Nonfinite/excessive frame point");for(double v:values)p.points.push_back(float(v));}
void validate(const FrameInput& in,const FrameAudio& audio){
    const std::array scalars={in.shiftTime,in.time,in.fps,in.motionScale,in.brightness,in.headroom,in.darkening,in.alignFraction,in.beat,in.averageStarLifetime,audio.rms,audio.visualEnergy};
    if(!in.width||!in.height||in.width>16384||in.height>16384||uint64_t(in.width)*in.height>16777216||!std::all_of(scalars.begin(),scalars.end(),[](double v){return std::isfinite(v);})||in.fps<1||in.fps>10000||in.motionScale<.01||in.motionScale>32||in.brightness<0||in.brightness>32||in.headroom<1||in.headroom>65504||in.darkening<0||in.darkening>16||in.alignFraction<=0||in.alignFraction>1||in.beat<0||in.beat>16||audio.rms<0||audio.rms>16||audio.visualEnergy<0||audio.visualEnergy>256||in.averageStarLifetime<0||in.averageStarLifetime>65504||in.waveSmoothing>4096)throw std::invalid_argument("Invalid Prism Flow frame controls");
    if(audio.wave.empty()||audio.wave.size()>4096)throw std::invalid_argument("Invalid Prism Flow audio window");for(float v:audio.wave)if(!std::isfinite(v)||std::abs(v)>16)throw std::invalid_argument("Invalid Prism Flow waveform");
}
}
FramePlan FramePlanner::build(const PresetHistory& history,const ActivePresets& active,const FrameInput& in,const FrameAudio& audio,const RandomFunction& random,FramePlan reuse){
    validate(in,audio);if(active.count<1||active.count>2)throw std::invalid_argument("A frame needs one or two active presets");FramePlan p=std::move(reuse);p.points.clear();p.stars.clear();p.title.reset();p.warps.fill(nullptr);p.strengths.fill(0);p.follow.fill(0);p.noiseX=p.noiseY=0;const auto& a=history.at(active.indices[0]);
    p.wave=a.wave;p.dots=a.dots;p.shader=a.shader;p.motion=a.motion;p.palette=a.expandedPalette;p.background=a.originalPalette.background;
    for(size_t n=0;n<active.count;++n){const auto& preset=history.at(active.indices[n]);for(size_t slot=0;slot<4;++slot){const auto j=n*4+slot;const auto& w=preset.warps[slot];p.warps[j]=&w;p.strengths[j]=float(active.count==1?1.:n?active.blend:1-active.blend);p.follow[j]=float(w.starsFollowMotion.value_or(w.map.preferences.stars_follow_motion_min));}}p.warpCount=active.count*4;
    if(active.count==2){const auto& b=history.at(active.indices[1]);const double t=active.blend;if(!std::isfinite(t)||t<0||t>1)throw std::invalid_argument("Invalid preset blend");
        p.wave=blend(a.wave,b.wave,t,waveFields);p.dots=blend(a.dots,b.dots,t,dotFields);p.shader=blend(a.shader,b.shader,t,shaderFields);p.motion=blend(a.motion,b.motion,t,motionFields);
        const double angle=a.wave.wave_flat_angle+pi*std::floor((b.wave.wave_flat_angle-a.wave.wave_flat_angle)/pi+.5);p.wave.wave_flat_angle=angle*(1-t)+b.wave.wave_flat_angle*t;
        for(size_t n=0;n<p.palette.size();++n)p.palette[n]=float(double(a.expandedPalette[n])*(1-t)+double(b.expandedPalette[n])*t);for(size_t n=0;n<3;++n)p.background[n]=float(a.originalPalette.background[n]*(1-t)+b.originalPalette.background[n]*t);
    }
    if(!in.paletteLocked)frozenPalette_.reset();else if(frozenPalette_)p.palette=*frozenPalette_;else frozenPalette_=p.palette;
    double maximum=0;for(size_t n=0;n<256;++n){for(unsigned c=0;c<3;++c){const double v=std::min(double(p.palette[n*4+c])*in.brightness,3.);p.palette[n*4+c]=float(v);maximum=std::max(maximum,v);}p.palette[n*4+3]=1;}
    for(size_t n=0;n<256;++n){const Color c={p.palette[n*4],p.palette[n*4+1],p.palette[n*4+2]};const auto mapped=reduceHdrHeadroom(in.headroom,n*(1./255),c,maximum,in.experiment);for(unsigned j=0;j<3;++j)p.palette[n*4+j]=mapped[j];}
    Color bg=p.background;for(auto& v:bg)v*=in.brightness;const auto adapted=reduceHdrHeadroom(in.headroom,.7,bg,maximum,in.experiment);for(unsigned c=0;c<3;++c)p.background[c]=adapted[c];
    for(size_t n=0;n<p.warpCount;++n){const auto& w=*p.warps[n];const double strength=w.strength*p.strengths[n],breathing=strength*(1+w.map.breathingStrength*std::cos(in.shiftTime*w.map.breathingFrequency+w.map.breathingPhase));p.strengths[n]=float(breathing*motionScale*in.motionScale/in.fps);}
    const auto diagonal=jsInt32(std::sqrt(double(in.width)*in.width+double(in.height)*in.height));const double shift=motionScale*in.motionScale*diagonal/in.fps*.000121*(.95*.65),t=in.shiftTime*60;
    p.shiftX=shift*(std::cos(t*.01171*.7)*2+std::cos(t*.00111*.7+3)*2);p.shiftY=shift*(std::cos(t*.00874*.7+1)*1.3+std::cos(t*.00351*.7+2)*1.3);
    p.pointSize=std::max(1.,double(diagonal)/jsInt32(std::sqrt(2060.*2060+1430.*1430)));const double rate=audio.visualEnergy>energy_?.7:.1,factor=1-std::pow(1-rate,1./std::clamp(in.fps,5.,120.));energy_=energy_*(1-factor)+factor*audio.visualEnergy;p.amplitude=energy_>.009?.009/energy_:1;
    p.points.reserve(32768*4);wavePoints(p,in,audio);decorations(p,in,random);
    if(in.embedUnits){if(in.embedUnits>8192)throw std::invalid_argument("Embedded title exceeds its size budget");if(titleX_<=0){const double x0=draw(random),x1=draw(random),x2=draw(random),y0=draw(random),y1=draw(random),y2=draw(random);titleX_=in.width*(.4+.2*((x0+x1+x2)*(1./3)));titleY_=in.height*(.4+.2*((y0+y1+y2)*(1./3)));}p.title=FramePlan::Title{titleX_,titleY_,uint32_t(std::max(8.,diagonal*1.1/std::max(36u,in.embedUnits)))};}
    p.autoDarkening=std::clamp((p.motion.blended_net_zoom_motion-.8)/(2-.8),0.,1.)*.25*(.5+.5*p.wave.wave_flat_str*(1-p.wave.wave_flat_is_stereo));p.darkening=in.darkening+p.autoDarkening;
    // GPUWarp.set_params consumes these only when a simulation step is encoded.
    if(!in.frozen){p.noiseX=uint32_t(draw(random)*255);p.noiseY=uint32_t(draw(random)*255);}return p;
}
void FramePlanner::wavePoints(FramePlan& out,const FrameInput& in,const FrameAudio& audio){
    const auto& p=out.wave;const auto diagonal=jsInt32(std::sqrt(double(in.width)*in.width+double(in.height)*in.height));const Transform transform(in.width,in.height);std::vector<float> waveform;
    if(p.wave_flat_str>0||p.wave_circ_str>0)waveform=blurWave(aligner_.align(audio.wave,in.alignFraction),in.waveSmoothing);
    if(p.wave_flat_str>0){const double cosine=std::cos(p.wave_flat_angle),sine=std::sin(p.wave_flat_angle);const auto count=jsInt32(diagonal*1.5*(in.alignFraction/.55)/out.pointSize);if(count<0||count>32768)throw std::invalid_argument("Flat waveform exceeds sample budget");const auto samples=resampleWave(waveform,uint32_t(count));
        const double separation=p.wave_flat_is_stereo*p.wave_flat_stereo_sep;if(separation<0)throw std::invalid_argument("Negative waveform separation");const double amplitude=2.*.7*p.wave_flat_scale*out.amplitude*(1-.7*std::sqrt(separation)),base=std::min(255.,std::min(255.,audio.rms*20000+16)*p.wave_flat_str)*.75*1.1;
        double cx=p.wave_flat_cx,cy=p.wave_flat_cy;const double projection=cx*cosine+cy*sine;cx-=cosine*projection;cy-=sine*projection;const double mx=transform.x(cx),my=transform.y(cy),second=std::max(0.,p.wave_flat_is_stereo*2-1);
        for(unsigned pass=0;pass<2;++pass){if(pass==1&&(p.wave_flat_is_stereo<.0001||p.wave_flat_stereo_sep<.0001))break;const double mag=std::min((pass?double(jsInt32(base*second)):base)*(1./255),1.),offset=(-.5+pass)*separation,amplitude2=amplitude*(second*p.wave_flat_stereo_amplitude_scale+(1-second));
            for(size_t k=0;k<samples.size();++k){const double x0=(double(k)-samples.size()*.5)*out.pointSize,y0=(double(samples[k])*amplitude2+offset)*(diagonal*.3);point(out,mx+x0*cosine-y0*sine,my+x0*sine+y0*cosine,out.pointSize,mag);}}
    }
    if(p.wave_circ_str>0){const auto count=jsInt32(diagonal*.45);const auto samples=resampleWave(waveform,uint32_t(count));const auto overlap=samples.size()/8,drawCount=samples.size()-overlap;const double amplitude=2.2*p.wave_circ_scale*p.wave_circ_rad*out.amplitude,mag=std::min(double(jsInt32(std::min(255.,std::min(255.,audio.rms*20000*.16+12)*.6*p.wave_circ_str)))*(1./255),1.)*.75;
        for(size_t k=0;k<drawCount;++k){double value=double(samples[k])*amplitude;if(k<overlap){const double t=k*(1./overlap);value=double(samples[k+drawCount])*amplitude*(1-t)+value*t;}const double t=k*(1./drawCount),r=diagonal*(.1*(p.wave_circ_rad*1.15))+value*(diagonal*.3)*(.2*(p.wave_circ_scale*.85)),angle=t*(3.1415927*2)+p.wave_circ_angle;point(out,transform.x(p.wave_circ_cx)+r*std::cos(angle),transform.y(p.wave_circ_cy)+r*std::sin(angle),out.pointSize,mag);}
    }
}
void FramePlanner::decorations(FramePlan& p,const FrameInput& in,const RandomFunction& random){
    const Transform transform(in.width,in.height);const auto diagonal=jsInt32(std::sqrt(double(in.width)*in.width+double(in.height)*in.height));
    if(in.beat>.0001){double radius=.0025+draw(random)*.0025*10,count=std::pow(2.,4+3*draw(random)),intensity=32+draw(random)*128;const double energy=radius*radius*count*intensity*(in.beat+1),target=(.08+.08*draw(random))*1.5,adjust=std::pow(target/energy,.25);radius*=adjust*adjust*.44;count*=adjust*.5;intensity*=adjust;const auto r=jsInt32(radius*diagonal);draw(random); // Retained flat-shading random choice, unused by source drawing.
        if(p.dots.radial_beat_dots>.001){const auto v=jsInt32(intensity*p.dots.radial_beat_dots);const double circle=.1+.2*draw(random);const auto n=jsInt32(count/2);for(int i=0;i<n;++i){const double theta=i*pi*2/n;point(p,jsInt32(transform.x(std::cos(theta)*circle+p.motion.blended_motion_cx)),jsInt32(transform.y(std::sin(theta)*circle+p.motion.blended_motion_cy)),r*2,v*(1./255));}}
        if(p.dots.random_beat_dots>.001){const auto v=jsInt32(intensity*p.dots.random_beat_dots);for(int i=0;i<count;++i){const double x=jsInt32(draw(random)*(in.width-1)),y=jsInt32(draw(random)*(in.height-1));point(p,x,y,r*1.3,v*(1./255));}}
    }
    if(!in.frozen&&p.dots.stars_opacity>0){const double adj=1./in.motionScale,target=std::min(4095.,p.dots.stars_population*512),t=in.averageStarLifetime/4,spawn=target/(in.fps*4)*(1./std::max(t,.25)),radius=std::min(1.,std::max(.25,std::pow(t,.5)));const auto count=std::max(0.,std::floor(spawn+draw(random)));if(count>4096)throw std::invalid_argument("Frame exceeds bokeh spawn budget");p.stars.reserve(size_t(count));
        for(size_t n=0;n<count;++n){double x=0,y=0;unsigned attempts=0;do{x=draw(random)*2-1;y=draw(random)*2-1;if(++attempts>1024)throw std::runtime_error("Random source cannot sample a bokeh position");}while(x*x+y*y>1);x*=radius;y*=radius;const double z=.45+.1*draw(random),z2=.45+.1*draw(random);const bool background=draw(random)<.25;const double position=.3+draw(random)*.7;p.stars.push_back({x,y,z,z2,position,.5*adj,3*adj,.5*adj,background});}
    }
    for(unsigned kind=0;kind<3;++kind){const double strength=kind==0?p.dots.chaser_1:kind==1?p.dots.chaser_2:p.dots.dotty_chaser;if(strength<=.001)continue;const double step=kind==2?.06:.003,phase=kind==0?0:kind==1?371:150,speed=2.5*(kind==2?.75:1)*.93,scale=kind==2?1.5:1;
        for(unsigned i=0;i<10;++i){const double t=in.time+i*step+phase,fx=(std::cos(t*1.732*speed+1)*.15+std::cos(t*1.592*speed+2)*.2)*scale,fy=(std::cos(t*1.332*speed+3)*.15+std::cos(t*2.188*speed)*.2)*scale;point(p,transform.x(fx),transform.y(fy),p.pointSize*2,strength);}}
    if(p.dots.grid_dots>.001){const auto step=std::max(1,diagonal/24),v=jsInt32(96*p.dots.grid_dots);const double t=in.time*.55,fx=(std::cos(t*.537+1)+std::cos(t*.359+2))*.5,fy=(std::cos(t*.611+3)+std::cos(t*.429+5))*.4;const auto startX=jsInt32((fx+1)*step*3)%step,startY=jsInt32((fy+1)*step*3)%step;
        for(int y=startY;y<int(in.height);y+=step)for(int x=startX;x<int(in.width);x+=step)point(p,x,y,p.pointSize,v*(1./255));}
}
void applyFrameRequests(PresetController& controller,const ActivePresets& active,const FrameRequests& r,const RandomFunction& random){
    if(active.count<1||active.count>2)throw std::invalid_argument("Invalid editable active presets");auto& source=controller.history.at(active.mostActive);
    if(r.motion)source.randomizeMotion(random);if(r.wave)source.randomizeWave(random);if(r.dots)source.randomizeDots(random);if(r.shader)source.randomizeShader(random);
    bool palette=r.palette||r.overwrite||r.paletteScale!=1;if(r.palette)source.randomizePalette(controller.catalog,random);if(r.overwrite){source.originalPalette=*r.overwrite;source.expandedPalette=expandPalette(source.originalPalette.rgba);}if(r.paletteScale!=1){if(!std::isfinite(r.paletteScale)||r.paletteScale<=0||r.paletteScale>32)throw std::invalid_argument("Invalid palette brightness adjustment");for(auto& v:source.originalPalette.rgba)v=float(double(v)*r.paletteScale);source.expandedPalette=expandPalette(source.originalPalette.rgba);}
    for(size_t n=0;n<active.count;++n){auto& dst=controller.history.at(active.indices[n]);if(active.indices[n]!=active.mostActive){if(r.motion){dst.warps=source.warps;dst.motion=source.motion;dst.mapsReady=source.mapsReady;dst.rebuildId=-1;}if(r.wave)dst.wave=source.wave;if(r.dots)dst.dots=source.dots;if(r.shader)dst.shader=source.shader;if(palette){dst.originalPalette=source.originalPalette;dst.expandedPalette=source.expandedPalette;}}}
    const auto toggle=[&](bool enabled,double DotParams::*field){if(enabled){const double value=source.dots.*field>.1?0.:1.;for(size_t n=0;n<active.count;++n)controller.history.at(active.indices[n]).dots.*field=value;}};
    toggle(r.grid,&DotParams::grid_dots);toggle(r.fading,&DotParams::fading_dots);toggle(r.dotty,&DotParams::dotty_chaser);toggle(r.randomBeat,&DotParams::random_beat_dots);toggle(r.radialBeat,&DotParams::radial_beat_dots);
    if(r.chasers){const unsigned count=((source.dots.chaser_1>.001?1:0)+(source.dots.chaser_2>.001?1:0)+1)%3;for(size_t n=0;n<active.count;++n){auto& p=controller.history.at(active.indices[n]).dots;p.chaser_1=count>0?1:0;p.chaser_2=count>1?1:0;}}
}
}
