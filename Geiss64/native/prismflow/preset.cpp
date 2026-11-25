// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "preset.hpp"
#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace prismflow {
namespace {
double clamp(double value){return std::max(0.,std::min(1.,value));}
double lerp(double a,double b,double t){return a+(b-a)*t;}
double draw(const RandomFunction& random){
    if(!random)throw std::invalid_argument("A preset random source is required");
    const double value=random();if(!std::isfinite(value)||value<0||value>=1)throw std::invalid_argument("Preset random source must return [0,1)");return value;
}
double between(double lo,double hi,const RandomFunction& random){return lo+(hi-lo)*draw(random);}
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Prism Flow preset generation cancelled");}
double biasHorizontal(double theta,double iterations){
    iterations=std::clamp(iterations,0.,10.);const double t=theta/std::numbers::pi,base=std::floor(t);double dt=t-base;
    while(iterations>0){const double dt2=dt*dt*(3-2*dt),fraction=std::min(iterations,1.);dt=dt2*fraction+(1-fraction)*dt;iterations-=1;}
    return (base+dt)*std::numbers::pi;
}
double strengthTotal(const std::array<Warp,kMaxActiveWarps>& warps){
    double total=0;for(const auto& warp:warps){if(!std::isfinite(warp.strength)||warp.strength<0||warp.strength>65504)throw std::invalid_argument("Invalid preset motion strength");total+=warp.strength;}
    if(total==0)throw std::invalid_argument("Preset motion strengths total zero");return total;
}
}
Preset Preset::generate(uint64_t number,double t1,double t2,const PaletteCatalog& catalog,const RandomFunction& random,int32_t overrideMode,std::stop_token stop){
    if(!std::isfinite(t1)||!std::isfinite(t2)||t2<t1)throw std::invalid_argument("Invalid preset fade times");cancelled(stop);
    Preset preset;preset.number=number;preset.t1=t1;preset.t2=t2;
    preset.randomizeMotion(random,overrideMode,stop);preset.randomizeWave(random);preset.randomizeDots(random);preset.randomizeShader(random);
    preset.randomizePalette(catalog,random,stop);return preset;
}
void Preset::randomizeMotion(const RandomFunction& random,int32_t overrideMode,std::stop_token stop){
    cancelled(stop);if(overrideMode<-1)throw std::invalid_argument("Invalid preset override mode");
    std::array<Warp,kMaxActiveWarps> next;
    if(overrideMode>=0){next[0].mode=overrideMode;for(size_t i=1;i<next.size();++i)next[i].strength=0;}
    else{std::array<int32_t,kMaxActiveWarps> companions{};
        for(size_t i=0;i<next.size();++i){next[i].mode=pickRandomWarpMap(std::span(companions).first(i),draw(random));companions[i]=next[i].mode;}}
    for(auto& warp:next){cancelled(stop);const auto seed=jsInt32(draw(random)*4294967296.);warp.map=buildWarpMap(warp.mode,seed,stop);}
    // Build/measure off to the side, so cancellation or a failed map leaves
    // the existing preset usable. Moving the completed values does not copy maps.
    Preset staged;staged.warps=std::move(next);staged.mapsReady=true;
    staged.updateStarsFollowMotion(random,true);staged.extractMotionMetadata(random,stop);
    warps=std::move(staged.warps);motion=staged.motion;mapsReady=true;rebuildId=-1;
}
void Preset::extractMotionMetadata(const RandomFunction& random,std::stop_token stop){
    cancelled(stop);const auto total=strengthTotal(warps);const uint32_t size=kWarpMapSize;const size_t components=size_t(size)*size*2;
    for(const auto& warp:warps)if(warp.map.displacement.empty()?warp.map.dynamic.size()!=kDynamicMotionParamCount:warp.map.displacement.size()!=components)
        throw std::invalid_argument("Cannot measure an incomplete motion map");
    const double scale=100./std::sqrt(double(size*size+size*size)),cx=size/2,cy=size/2;MotionParams measured;
    for(unsigned sample=0;sample<256;++sample){
        cancelled(stop);
        // Explicit order is required: C++ does not sequence '+' operands.
        const double fx1=draw(random),fx2=draw(random),fy1=draw(random),fy2=draw(random);
        const double fx=(fx1+fx2)*.5,fy=(fy1+fy2)*.5;
        const auto dx=int32_t(fx*(size-1)),dy=int32_t(fy*(size-1));const size_t offset=(size_t(dy)*size+dx)*2;
        double blendedX=0,blendedY=0;
        for(const auto& warp:warps)if(!warp.map.displacement.empty()){
            blendedX+=double(warp.map.displacement[offset])*warp.strength;blendedY+=double(warp.map.displacement[offset+1])*warp.strength;}
        // Same float32 write as the reference's full temporary blended map,
        // evaluated only at the 256 sampled positions, without a 2 MiB copy.
        const double sx=dx+double(float(blendedX))*(.5*size),sy=dy+double(float(blendedY))*(.5*size);
        double rx=dx-cx,ry=dy-cy;const double inverse=1./std::sqrt(rx*rx+ry*ry+.00001);rx*=inverse;ry*=inverse;
        const double vx=(dx-sx)*scale,vy=(dy-sy)*scale,outward=rx*vx+ry*vy,clockwise=-ry*vx+rx*vy;
        measured.blended_net_motion+=std::sqrt(vx*vx+vy*vy);measured.blended_net_zoom_motion+=outward;measured.blended_in_or_out_motion+=std::abs(outward);
        measured.blended_net_clockwise_motion+=clockwise;measured.blended_cw_or_ccw_motion+=std::abs(clockwise);
    }
    measured.blended_net_motion/=256;measured.blended_net_zoom_motion/=256;measured.blended_in_or_out_motion/=256;
    measured.blended_net_clockwise_motion/=256;measured.blended_cw_or_ccw_motion/=256;
    for(const auto& warp:warps){measured.blended_motion_cx+=warp.map.preferences.motion_cx*warp.strength;measured.blended_motion_cy+=warp.map.preferences.motion_cy*warp.strength;}
    measured.blended_motion_cx/=total;measured.blended_motion_cy/=total;motion=measured;
}
WarpPreferences Preset::blendedPreferences(bool applyWavePreferenceWeights)const{
    strengthTotal(warps);std::array<double,kMaxActiveWarps> weights{};double total=0;
    for(size_t i=0;i<warps.size();++i){weights[i]=warps[i].strength;if(applyWavePreferenceWeights)weights[i]*=warps[i].map.preferences.wave_prefs_weight;total+=weights[i];}
    if(!std::isfinite(total)||total<=0)throw std::invalid_argument("Invalid weighted preset preferences");WarpPreferences blended{};
    for(const auto& field:preferenceFields){double value=0;for(size_t i=0;i<warps.size();++i)value+=weights[i]*(warps[i].map.preferences.*field.member);blended.*field.member=value/total;}
    double votes=0;for(const auto& warp:warps)votes+=warp.map.preferences.stars_voting_weight;
    if(!std::isfinite(votes)||votes<=0)throw std::invalid_argument("Invalid preset star votes");
    // The reference's star voting deliberately ignores motion strengths.
    for(auto member:{&WarpPreferences::stars_prob,&WarpPreferences::stars_population_min,&WarpPreferences::stars_population_max,&WarpPreferences::stars_opacity_min,&WarpPreferences::stars_opacity_max}){
        blended.*member=0;for(const auto& warp:warps)blended.*member+=(warp.map.preferences.*member)*(warp.map.preferences.stars_voting_weight/votes);}
    return blended;
}
void Preset::randomizeWave(const RandomFunction& random){
    WaveParams next;auto p=blendedPreferences(true);double flat=1,circle=.25,both=.01;
    constexpr double zoom=1.8;
    const double radial=clamp((motion.blended_in_or_out_motion-zoom*.2)/(zoom*.35-zoom*.2));circle*=radial;both*=radial;
    double factor=clamp((motion.blended_net_zoom_motion-zoom*.25)/(zoom*.35-zoom*.25));factor=1-.5*factor*draw(random);p.flat_stereo_sep_lo*=factor;p.flat_stereo_sep_hi*=factor;
    factor=clamp((motion.blended_net_zoom_motion-zoom*.3)/(zoom*.5-zoom*.3));factor=1-.5*factor*draw(random);p.circ_rad_lo*=factor;p.circ_rad_hi*=factor;
    if(p.use_motion_metadata_to_choose_wave_type<1){const double t=p.use_motion_metadata_to_choose_wave_type;
        flat=flat*t+(1-t)*p.wave_flat_prob;circle=circle*t+(1-t)*p.wave_circ_prob;both=both*t+(1-t)*p.wave_flat_plus_circ_prob;}
    flat=std::max(flat,.0000001);circle=std::max(circle,.0000001);both=std::max(both,.0000001);
    const double t=draw(random)*(flat+circle+both);
    if(t<flat)next.wave_flat_str=1;else if(t<flat+circle)next.wave_circ_str=1;else next.wave_flat_str=next.wave_circ_str=1;
    if(next.wave_flat_str>.0001){
        next.wave_flat_angle=biasHorizontal(between(p.flat_angle_lo,p.flat_angle_hi,random),p.flat_angle_bias_toward_horizontal_angles);
        next.wave_flat_scale=p.flat_scale;next.wave_flat_is_stereo=draw(random)<p.flat_is_stereo_chance?1:0;
        next.wave_flat_stereo_sep=between(p.flat_stereo_sep_lo,p.flat_stereo_sep_hi,random);next.wave_flat_stereo_amplitude_scale=p.flat_stereo_amplitude_scale;
        const double x1=between(p.flat_cx_lo,p.flat_cx_hi,random),x2=between(p.flat_cx_lo,p.flat_cx_hi,random),y1=between(p.flat_cy_lo,p.flat_cy_hi,random),y2=between(p.flat_cy_lo,p.flat_cy_hi,random);
        next.wave_flat_cx=(x1+x2)*.5;next.wave_flat_cy=(y1+y2)*.5;
    }
    if(next.wave_circ_str>.0001){
        next.wave_circ_rad=between(p.circ_rad_lo,p.circ_rad_hi,random);next.wave_circ_scale=p.circ_scale;next.wave_circ_angle=draw(random)*6.28;
        const double x1=between(p.circ_cx_lo,p.circ_cx_hi,random),x2=between(p.circ_cx_lo,p.circ_cx_hi,random),y1=between(p.circ_cy_lo,p.circ_cy_hi,random),y2=between(p.circ_cy_lo,p.circ_cy_hi,random);
        next.wave_circ_cx=(x1+x2)*.5;next.wave_circ_cy=(y1+y2)*.5;
    }
    wave=next;
}
void Preset::randomizeDots(const RandomFunction& random){
    DotParams next;auto p=blendedPreferences(true);p.radial_beat_dots_prob*=clamp((motion.blended_in_or_out_motion-1.8*.15)/(1.8*.50-1.8*.15));
    next.radial_beat_dots=draw(random)<p.radial_beat_dots_prob?1:0;next.random_beat_dots=draw(random)<p.random_beat_dots_prob?1:0;
    next.fading_dots=draw(random)<p.fading_dots_prob?1:0;next.grid_dots=draw(random)<p.grid_dots_prob?1:0;
    const bool chasers=draw(random)<p.chaser_prob;next.chaser_1=chasers?1:0;next.chaser_2=chasers&&draw(random)<.25?1:0;next.dotty_chaser=draw(random)<p.dotty_chaser_prob?1:0;
    const bool stars=draw(random)<p.stars_prob;next.stars_opacity=stars?lerp(p.stars_opacity_min,p.stars_opacity_max,draw(random)):0;
    if(p.stars_population_min<=0||p.stars_population_max<=0)throw std::invalid_argument("Invalid star population range");
    next.stars_population=p.stars_population_min*std::pow(p.stars_population_max/p.stars_population_min,draw(random));
    updateStarsFollowMotion(random,true);dots=next;
}
void Preset::randomizeShader(const RandomFunction& random){
    ShaderParams next;
    if(draw(random)<.065){const double a=draw(random),b=draw(random),c=draw(random);next.rx_diff_grey_scott_str=.2+.4*(a+b+c)*.3333333;}
    if(draw(random)<.12){const double a=draw(random),b=draw(random);next.frequency_injector_str=(a+b)*.5;}
    next.frequency_injector_freq_scale=.9+.2*draw(random);
    const double noise1=std::pow(draw(random),4.)*.5,noise2=std::pow(draw(random),4.)*(1.-.5);next.noise_str=noise1+noise2;
    if(draw(random)<.32){
        const double p1=draw(random),p2=draw(random);next.painterly_str=.5+.5*std::pow(p1*.5+p2*.5,.5);
        const double m1=draw(random),m2=draw(random);next.layer2_motion_scale=.78+.09*m1+.09*m2;
        next.layer2_brightness=std::pow(draw(random),.2);next.layer1_density=1.05+.3*std::pow(draw(random),4.);
    }else{
        next.painterly_str=draw(random)>=.10?0:draw(random);
        next.layer2_motion_scale=draw(random)<.8?.8+.2*draw(random):.5+.5*draw(random);
        next.layer2_brightness=draw(random)<.30?std::pow(draw(random),.4):0;
        next.layer1_density=1.2+1.3*std::pow(draw(random),3.);
    }
    shader=next;
}
void Preset::randomizePalette(const PaletteCatalog& catalog,const RandomFunction& random,std::stop_token stop){
    auto next=catalog.generate(random,stop);auto expanded=expandPalette(next.rgba);originalPalette=std::move(next);expandedPalette=expanded;
}
void Preset::updateStarsFollowMotion(const RandomFunction& random,bool forceAll){
    for(auto& warp:warps)if(forceAll||!warp.starsFollowMotion){const auto& p=warp.map.preferences;const double t=std::pow(clamp(draw(random)*1.2-.1),p.stars_follow_motion_pow);
        warp.starsFollowMotion=lerp(p.stars_follow_motion_min,p.stars_follow_motion_max,t);}
}
void Preset::setWarpSlot(size_t slot,int32_t mode,const RandomFunction& random,std::stop_token stop){
    if(slot>=warps.size())throw std::out_of_range("Invalid preset warp slot");const auto seed=jsInt32(draw(random)*4294967296.);
    auto next=buildWarpMap(mode,seed,stop);warps[slot].map=std::move(next);warps[slot].mode=mode;warps[slot].starsFollowMotion.reset();
    rebuildId=-1;updateStarsFollowMotion(random,false);extractMotionMetadata(random,stop);mapsReady=true;
}
void Preset::releaseMaps(){
    for(auto& warp:warps)std::vector<float>().swap(warp.map.displacement);
    mapsReady=std::all_of(warps.begin(),warps.end(),[](const auto& warp){return !warp.map.displacement.empty()||!warp.map.dynamic.empty();});
}
void Preset::rebuildMaps(bool fromWorker,std::stop_token stop){
    cancelled(stop);if(mapsReady)return;
    std::array<std::optional<MotionMap>,kMaxActiveWarps> next;
    for(size_t i=0;i<warps.size();++i)if(warps[i].map.displacement.empty()&&warps[i].map.dynamic.empty())next[i]=buildWarpMap(warps[i].mode,warps[i].map.seed,stop);
    for(size_t i=0;i<warps.size();++i)if(next[i])warps[i].map=std::move(*next[i]);mapsReady=true;if(!fromWorker)rebuildId=-1;
}
size_t Preset::mapBytes()const{size_t bytes=0;for(const auto& warp:warps)bytes+=(warp.map.displacement.capacity()+warp.map.dynamic.capacity())*sizeof(float);return bytes;}
}
