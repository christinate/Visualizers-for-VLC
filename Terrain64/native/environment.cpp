// SPDX-License-Identifier: MIT
#include "environment.hpp"
#include "marine_wgsl.hpp"
#include <numbers>

namespace terrain {
namespace {
constexpr float Tau=2*std::numbers::pi_v<float>;
constexpr char UniformSource[]=R"(
struct Scene {
    viewport:vec4f, audio:vec4f, output:vec4f, camera:vec4f, geometry:vec4f,
    effects:vec4f, land:vec4f, features:vec4f, weather:vec4f, detail:vec4f, view:vec4f,
    mountains:vec4f, spray:vec4f, buildings:vec4f,
    waveform:array<vec4f,8>, waves:array<vec4f,24>,
    rain:array<vec4f,32>, rainStyle:array<vec4f,32>, impacts:array<vec4f,4>,
    ocean:vec4f, wildlife:vec4f, reef:vec4f, marine:vec4f, water:vec4f, detachedKelp:vec4f,
    kelp:array<vec4f,3>,
    beachWater:vec4f, beachDetail:vec4f, beachFollow:vec4f, beachVisitor:vec4f, beachDolphin:vec4f,
    beachTurtles:array<vec4f,3>,
    reefCurrent:vec4f, reefTerrain:vec4f,
};
@group(0) @binding(0) var<uniform> g:Scene;
@vertex fn vertex(@builtin(vertex_index) index:u32)->@builtin(position) vec4f{
    let points=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
    return vec4f(points[index],0,1);
}
fn tint(h:f32)->vec3f{
    return .53+.47*cos(6.2831853*(h+vec3f(0,.33333,.66667)));
}
fn displayColor(color:vec3f,output:vec4f)->vec4f{
    let value=max(vec3f(0),color);
    if(output.y>.5){return vec4f(value*output.z,1);}
    return vec4f(pow(value/(vec3f(1)+value),vec3f(1.0/2.2)),1);
}
fn modeBadge(color:vec3f,pixel:vec2f,size:vec2f,state:f32)->vec3f{
    if(state==0.0){return color;}
    let span=clamp(min(size.x,size.y)*.075,18.0,36.0);
    let p=(pixel-(size-vec2f(span*.76)))/span;
    let panel=1.0-smoothstep(.40,.47,length(max(abs(p)-vec2f(.04),vec2f(0))));
    let body=1.0-smoothstep(.20,.247,max(abs(p.x)*.91,abs(p.y-.149)*1.29));
    let open=state<0.0;
    let q=p-vec2f(select(0.0,.105,open),-.092);
    let ring=abs(length(vec2f(q.x,min(q.y,0.0)))-.159);
    let legs=q.y<.105&&(q.y<.009||!open||q.x>0.0);
    let shackle=select(0.0,1.0-smoothstep(.023,.057,ring),legs);
    let keyhole=1.0-smoothstep(.024,.055,length((p-vec2f(0,.137))*vec2f(1,.76)));
    let glyph=max(body*(1.0-keyhole),shackle);
    let contrast=mix(color,vec3f(.022,.033,.052),panel*.85);
    let mark=mix(vec3f(.85,.98,1.0),vec3f(1.0,.85,.53),select(0.0,1.0,open));
    return mix(color,mix(contrast,mark,glyph),abs(state));
}
fn hash(n:f32)->f32{return fract(sin(n*127.1+17.9)*43758.5453);}
fn rotate2(p:vec2f,a:f32)->vec2f{
    let c=cos(a);let s=sin(a);
    return vec2f(c*p.x-s*p.y,s*p.x+c*p.y);
}
)";
float smooth(float value,float sample,double dt,float rate){
    return value+(sample-value)*static_cast<float>(1-std::exp(-dt*rate));
}
float spectral(const visualizers::MusicFrame& frame,float minimum,float maximum){
    constexpr float binHz=44100.f/2048;
    const auto first=std::max(1u,static_cast<unsigned>(std::ceil(minimum/binHz)));
    const auto last=std::min(1023u,static_cast<unsigned>(maximum/binHz));
    float peak=0;
    for(unsigned bin=first;bin<=last;++bin)peak=std::max(peak,frame.spectrum[bin]);
    return peak;
}
float bandLevel(float energy,float threshold){return std::clamp((energy-threshold)/(threshold*5+.015f),0.f,1.f);}
void copyWave(const visualizers::MusicFrame& frame,SceneUniforms& uniforms){
    for(size_t slot=0;slot<32;++slot){
        float sample=0;
        for(size_t index=0;index<8;++index)sample+=frame.wave[slot*64+index*8];
        uniforms.waveform[slot/4][slot%4]=std::clamp(sample/8,-1.f,1.f);
    }
}
} // namespace

EnvironmentTarget::EnvironmentTarget(visualizers::gpu::Context& context,const std::string& source,unsigned width,unsigned height)
    :context_(context){
    if(source.empty())throw std::runtime_error("Scene shader is empty");
    pipeline_=visualizers::gpu::pipeline(context,context.shader(std::string(UniformSource)+MarineSource+source),"vertex","fragment");
    wgpu::BufferDescriptor description{};
    description.size=sizeof(SceneUniforms);
    description.usage=wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst;
    uniform_=context.device.CreateBuffer(&description);
    wgpu::BindGroupEntry entry{};
    entry.buffer=uniform_;
    entry.size=sizeof(SceneUniforms);
    wgpu::BindGroupDescriptor group{};
    group.layout=pipeline_.GetBindGroupLayout(0);
    group.entryCount=1;
    group.entries=&entry;
    bindings_=context.device.CreateBindGroup(&group);
    resize(width,height);
}
void EnvironmentTarget::resize(unsigned width,unsigned height){
    if(target_&&width==target_.GetWidth()&&height==target_.GetHeight())return;
    auto target=context_.target(width,height);
    auto view=target.CreateView();
    context_.checkErrors();
    target_=std::move(target);
    view_=std::move(view);
}
void EnvironmentTarget::draw(SceneUniforms& uniforms){
    uniforms.viewport[0]=static_cast<float>(target_.GetWidth());
    uniforms.viewport[1]=static_cast<float>(target_.GetHeight());
    uniforms.viewport[3]=uniforms.viewport[0]/uniforms.viewport[1];
    context_.queue.WriteBuffer(uniform_,0,&uniforms,sizeof(uniforms));
    visualizers::gpu::render(context_,pipeline_,view_,bindings_);
}

AudioPalette::AudioPalette(const SceneIni& ini)
    :displayed_(ini.number("palette.hue",.52f,0.f,1.f)),hue_(displayed_),
     fadeSeconds_(ini.number("palette.fade_seconds",.34f,.01f,10.f)),
     interval_(ini.number("palette.seconds",60.,1.,3600.)),
     beats_(ini.number("palette.beats_per_change",4u,0u,128u)),
     hueStep_(ini.number("palette.hue_step",.137f,0.f,1.f)),
     enabled_(ini.enabled("palette.enabled",true)){}
float AudioPalette::update(double time,double dt,bool hit){
    if(!enabled_)return displayed_;
    if(beats_&&hit&&++count_>=beats_){count_=0;advance();}
    if(time<lastCycle_)lastCycle_=time;
    if(time-lastCycle_>=interval_){lastCycle_=time;advance();}
    // Follow the shortest arc between hues, including when it crosses 0/1.
    displayed_+=std::remainder(hue_-displayed_,1.f)*static_cast<float>(1-std::exp(-dt/fadeSeconds_));
    displayed_-=std::floor(displayed_);
    return displayed_;
}
void AudioPalette::advance(){if(enabled_){hue_+=hueStep_;hue_-=std::floor(hue_);}}

BeachMotion::Settings::Settings(const SceneIni& i)
    :waves(i.enabled("waves.enabled",true)),breaker(i.enabled("breaker.enabled",true)),
     notes(i.enabled("rain.enabled",true)),rainOnProps(i.enabled("rain.splash_on_props",true)),
     dolphins(i.enabled("ocean_life.dolphins",true)),fish(i.enabled("ocean_life.fish",true)),
     turtles(i.enabled("ocean_life.turtles",true)),
     props(0),seed(i.number("environment.seed",719u,0u,65535u)),
     minimumGap(i.number("waves.minimum_interval",.3f,.12f,8.f)),
     maximumGap(i.number("waves.maximum_interval",.7f,minimumGap,8.f)),
     travelSeconds(i.number("waves.travel_seconds",1.5f,.15f,12.f)),
     retreatSeconds(i.number("waves.retreat_seconds",1.4f,.1f,12.f)),
     reach(i.number("waves.ocean_distance",24.f,3.f,65.f)),runup(i.number("waves.shore_runup",1.8f,0.f,8.f)),
     returnDistance(i.number("waves.return_distance",9.f,.5f,40.f)),
     crest(i.number("waves.crest_width",.8f,.1f,3.f)),advance(i.number("waves.audio_advance",2.f,0.f,12.f)),
     swell(i.number("waves.waveform_depth",2.4f,0.f,7.f)),
     breakerSeconds(i.number("breaker.seconds",1.45f,.2f,8.f)),breakerForce(i.number("breaker.strength",1.f,0.f,3.f)),
     cameraSeconds(i.number("camera.seconds",24.f,2.f,600.f)),cameraHeight(i.number("camera.height",5.4f,2.f,15.f)),
     cameraSpeed(i.number("camera.speed",2.f,-9.f,9.f)),cameraFocal(i.number("camera.focal",1.28f,.7f,3.f)),
     cameraPitch(i.number("camera.look_down",.28f,.02f,.9f)),
     cameraTransition(i.number("camera.transition_fraction",.18f,0.f,.8f)),
     rainClearance(i.number("rain.camera_clearance",4.f,.5f,24.f)),
     density(i.number("environment.density",.86f,0.f,1.f)),motion(i.number("environment.audio_motion",1.f,0.f,4.f)),
     spacing(i.number("environment.spacing",6.8f,4.f,16.f)),horizon(i.number("environment.horizon",105.f,35.f,180.f)),
     shoreBend(i.number("environment.shore_bend",1.45f,0.f,5.f)),
     rainFall(i.number("rain.fall_seconds",.68f,.1f,3.f)),rainFade(i.number("rain.splash_seconds",.65f,.08f,3.f)),
     rainDepth(i.number("rain.horizon_distance",92.f,12.f,165.f)),rainWidth(i.number("rain.area_width",39.f,8.f,90.f)),
     rainStreak(i.number("rain.streak_length",.64f,.04f,2.5f)),
     rainHeight(i.number("rain.drop_height",18.f,1.f,72.f)),
     propSplashChance(i.number("rain.prop_splash_chance",.22f,0.f,1.f)),
     mountainRange(i.enabled("mountains.enabled",true)),
     mountainSetback(i.number("mountains.setback",21.f,18.f,42.f)),
     mountainHeight(i.number("mountains.height",21.f,3.f,45.f)),
     mountainWidth(i.number("mountains.width",43.f,10.f,85.f)),
     mountainMotion(i.number("mountains.audio_motion",.75f,0.f,3.f)),
     spraySeconds(i.number("spray.seconds",1.28f,.35f,4.f)),
     sprayHeight(i.number("spray.height",5.2f,.4f,12.f)),
     nearRock(i.number("spray.near_distance",5.f,2.f,22.f)),
     farRock(i.number("spray.far_distance",34.f,nearRock+4.f,70.f)),
     sequenceGap(i.number("spray.reset_after_seconds",2.4f,.25f,10.f)),
     rainbowChance(i.number("spray.rainbow_chance",.19f,0.f,1.f)),
     sprayDrops(i.number("spray.droplets",112u,0u,128u)),
     approachBeats(i.number("spray.approach_beats",5u,2u,9u)),
     rocksEvery(i.number("spray.beats_per_impact",4u,1u,32u)),
     buildingSeconds(i.number("buildings.return_seconds",.46f,.08f,4.f)),
     buildingGrowth(i.number("buildings.growth",.29f,0.f,.7f)),
     buildingSwing(i.number("buildings.shimmy",.33f,0.f,.8f)),
     tideSpeed(i.number("shore_motion.speed",.22f,0.f,1.25f)),
     tideMotion(i.number("shore_motion.amount",.28f,0.f,1.f)),
     dolphinSeconds(i.number("ocean_life.dolphin_interval",300.f,1.f,3600.f)),
     fishSeconds(i.number("ocean_life.fish_interval",60.f,1.f,3600.f)),
     dolphinDuration(i.number("ocean_life.dolphin_jump_seconds",2.4f,.4f,8.f)),
     fishDuration(i.number("ocean_life.fish_jump_seconds",1.15f,.3f,5.f)),
     turtleSpeed(i.number("ocean_life.turtle_speed",.45f,.02f,3.f)),
     turtleSurface(i.number("ocean_life.turtle_surface_seconds",65.f,5.f,900.f)),
     turtleDive(i.number("ocean_life.turtle_dive_seconds",30.f,0.f,180.f)),
     turtleDepth(i.number("ocean_life.turtle_dive_depth",2.8f,.8f,6.f)),
     swellHeight(i.number("offshore_swells.height",2.15f,0.f,6.f)),
     swellDistance(i.number("offshore_swells.distance",21.f,10.f,65.f)),
     swellLength(i.number("offshore_swells.wavelength",20.f,9.f,50.f)),
     swellSpeed(i.number("offshore_swells.speed",1.2f,0.f,6.f)),
     clarity(i.number("water.clarity",.80f,0.f,1.f)),
     caustics(i.number("water.caustic_strength",.92f,0.f,2.f)),
     chop(i.number("water.surface_chop",.82f,0.f,2.f)),
     interactions(i.number("water.impact_strength",1.f,0.f,2.f)),
     shoreDetail(i.number("environment.detail",1.f,0.f,1.f)),
     duneHeight(i.number("landscape.dune_height",.83f,0.f,1.5f)),
     skyColor(i.number("landscape.sky_color",1.05f,0.f,2.f)),
     sandTexture(i.number("landscape.sand_texture",.24f,0.f,1.f)),
     followTurtle(i.number("turtle_follow.turtle",1u,1u,unsigned(BeachTurtles))-1u),
     followDistance(i.number("turtle_follow.distance",3.05f,2.4f,7.f)),
     visitor(i.enabled("beach_chair.enabled",true)),
     visitInterval(i.number("beach_chair.interval",60.f,6.f,900.f)),
     visitDuration(i.number("beach_chair.visible_seconds",16.f,1.f,visitInterval)),
     headBob(i.number("beach_chair.head_bob",.17f,0.f,.45f)),
     low{i.enabled("low_notes.enabled",true),i.number("low_notes.minimum_hz",45.f,20.f,9000.f),
         i.number("low_notes.maximum_hz",250.f,21.f,18000.f),i.number("low_notes.threshold",.012f,.0001f,1.f),
         i.number("low_notes.drops_per_second",14.f,0.f,32.f),i.number("low_notes.splash_radius",.55f,.05f,2.5f)},
     high{i.enabled("high_notes.enabled",true),i.number("high_notes.minimum_hz",1700.f,20.f,17000.f),
          i.number("high_notes.maximum_hz",9500.f,21.f,20500.f),i.number("high_notes.threshold",.006f,.0001f,1.f),
          i.number("high_notes.drops_per_second",20.f,0.f,32.f),i.number("high_notes.splash_radius",.22f,.03f,2.5f)}{
    if(low.minimumHz>=low.maximumHz||high.minimumHz>=high.maximumHz)
        throw std::runtime_error("Beach note sensor minimum_hz must be below maximum_hz");
    for(unsigned n=0;n<cameras.size();++n)cameras[n]=i.enabled(std::string(BeachCameras[n])+".enabled",true);
    cameras[BeachTurtleCamera]=cameras[BeachTurtleCamera]&&turtles;
    if(std::none_of(cameras.begin(),cameras.end(),[](bool enabled){return enabled;}))
        throw std::runtime_error("Enable at least one Beach camera in beach.ini");
    constexpr std::array kinds={"animals","houses","trees","rocks","grass"};
    for(unsigned index=0;index<kinds.size();++index)
        if(i.enabled(std::string("environment.")+kinds[index],true))props|=1u<<index;
}

float BeachMotion::sample(float minimum,float maximum){
    return minimum+(maximum-minimum)*std::generate_canonical<float,24>(random_);
}
void BeachMotion::update(const visualizers::MusicFrame& frame,double time,double dt,SceneUniforms& u){
    const auto& s=settings_;
    if(time<lastTime_){
        waves_={};rain_={};nextWave_=time;travel_=0;rainBudget_=0;
        impacts_={};lastRockBeat_=-1;approachIndex_=0;impactIndex_=0;rockBeat_=0;
    }
    if(lastTime_<0||time<lastTime_){
        nextDolphin_=time+s.dolphinSeconds;nextFish_=time+s.fishSeconds;
        dolphinStart_=fishStart_=-1;
        previewingDolphin_=false;
        oceanStart_=time;
        visitNumber_=-1;
        followingTurtle_=false;
        for(size_t index=0;index<turtleDiveStart_.size();++index)
            turtleDiveStart_[index]=time+s.turtleSurface*(.34+.29*index);
    }
    if(lastTime_<0||time-lastTime_>s.travelSeconds+s.retreatSeconds)nextWave_=time;
    lastTime_=time;
    const bool hit=beat_.update(time,frame.beatCount,frame.beat);
    const float pulseStrength=beat_.remaining(time,s.breakerSeconds);
    const float input=std::clamp(static_cast<float>(frame.rms)*4.5f,0.f,1.f);
    activity_=smooth(activity_,input,dt,5);
    low_=smooth(low_,s.low.enabled?bandLevel(spectral(frame,s.low.minimumHz,s.low.maximumHz),s.low.threshold):0,dt,9);
    high_=smooth(high_,s.high.enabled?bandLevel(spectral(frame,s.high.minimumHz,s.high.maximumHz),s.high.threshold):0,dt,11);
    travel_+=dt*s.cameraSpeed;
    std::array<unsigned,BeachCameras.size()> cameras{};
    unsigned count=0,followSlot=0;
    for(unsigned index=0;index<s.cameras.size();++index){
        if(!s.cameras[index])continue;
        if(index==BeachTurtleCamera)followSlot=count;
        cameras[count++]=index;
    }
    constexpr double diveLead=2.0, surfaceHold=4.0;
    const double extra=s.cameras[BeachTurtleCamera]&&s.turtleDive>0
        ?std::max(0.,diveLead+s.turtleDive+surfaceHold+s.cameraSeconds*s.cameraTransition-s.cameraSeconds):0.;
    const auto camera=cameras_.frame(time,s.cameraSeconds,s.cameraTransition,count,true,followSlot,extra);
    const float blend=static_cast<float>(camera.blend);
    // X offset, relative altitude, yaw from the shoreline, and fixed downward pitch.
    constexpr std::array<Vec4,BeachCameras.size()> views={{
        {-1.8f,1.f,.065f,0}, {-8.6f,1.24f,.21f,0}, {5.4f,.81f,-.19f,0},
        {-13.f,1.52f,Tau/4,.43f}, {-3.f,3.2f,Tau/4,1.29f},
        {17.f,1.75f,-Tau/4,.46f}, {5.f,3.2f,-Tau/4,1.29f},
        {-9.f,1.86f,.82f,.52f}, {9.f,1.98f,-.96f,.59f}, {-8.5f,1.24f,.16f,.33f}
    }};
    const auto& from=views[cameras[camera.from]];
    const auto& to=views[cameras[camera.to]];
    const float cameraX=std::lerp(from[0],to[0],blend);
    const float cameraY=s.cameraHeight*std::lerp(from[1],to[1],blend);
    const float yaw=from[2]+std::remainder(to[2]-from[2],Tau)*blend;
    const float pitch=std::lerp(from[3]>0?from[3]:std::atan(s.cameraPitch),to[3]>0?to[3]:std::atan(s.cameraPitch),blend);
    const float followBlend=std::lerp(float(cameras[camera.from]==BeachTurtleCamera),
                                      float(cameras[camera.to]==BeachTurtleCamera),blend);
    const bool following=cameras[camera.current]==BeachTurtleCamera;
    const auto diveAge=[&](size_t index){
        const double start=turtleDiveStart_[index];
        const double cycle=s.turtleSurface*(.91+.17*index)+s.turtleDive;
        return time>=start?std::fmod(time-start,cycle):double(s.turtleDive);
    };
    if(following&&!followingTurtle_&&diveAge(s.followTurtle)>=s.turtleDive)
        turtleDiveStart_[s.followTurtle]=time+diveLead;
    followingTurtle_=following;
    if(hit&&s.breaker){
        if(lastRockBeat_<0||time-lastRockBeat_>s.sequenceGap)approachIndex_=0;
        lastRockBeat_=time;
        rockBeat_=(rockBeat_+1)%s.rocksEvery;
        const float progress=float(approachIndex_)/float(s.approachBeats-1);
        const int closest=std::max(1,int(std::ceil(s.nearRock/s.spacing)));
        const int farthest=std::max(closest+int(s.approachBeats)-1,int(std::lround(s.farRock/s.spacing)));
        const int rock=int(std::lround(std::lerp(float(farthest),float(closest),progress)));
        const double heading=std::cos(yaw)<-.12f?-1.:1.;
        hitZ_=(std::floor(travel_/s.spacing)+.45+heading*rock)*s.spacing;
        if(rockBeat_==0){
            if((s.props&(1u<<3))&&s.breakerForce>0){
                const float seed=sample(0,1)+(sample(0,1)<s.rainbowChance?1.f:0.f);
                impacts_[impactIndex_++%impacts_.size()]={time,float(hitZ_),s.breakerForce*(.77f+.23f*activity_),seed};
            }
            approachIndex_=(approachIndex_+1)%s.approachBeats;
        }
    }
    const auto shore=[&](float z){return s.shoreBend*(std::sin(z*.12f)+.65f*std::sin(z*.041f+.42f));};
    const bool dolphinActive=dolphinStart_>=0&&time-dolphinStart_<s.dolphinDuration;
    if(previewDolphinPending_||(hit&&s.dolphins&&!dolphinActive&&time>=nextDolphin_)){
        previewingDolphin_=previewDolphinPending_;
        previewDolphinPending_=false;
        dolphinStart_=time;
        if(!previewingDolphin_)nextDolphin_=time+s.dolphinSeconds;
        const float forward=std::cos(yaw);
        dolphinDirection_=std::copysign(1.f,forward);
        dolphinZ_=float(travel_)+(previewingDolphin_?9.f*dolphinDirection_:16.f*forward);
        dolphinX_=shore(dolphinZ_)-std::lerp(8.3f,10.5f,std::max(0.f,-std::sin(yaw)));
    }
    if(hit&&s.fish&&time>=nextFish_){
        fishStart_=time;nextFish_=time+s.fishSeconds;
    }
    const auto eventPhase=[time](double start,float duration){
        return start<0||time-start>=duration?-1.f:float((time-start)/duration);
    };
    float maxRise=0;
    for(unsigned n=0;n<count;++n)maxRise=std::max(maxRise,views[cameras[n]][1]);
    u.viewport[2]=static_cast<float>(time);
    u.audio={activity_,pulseStrength,low_,high_};
    u.output[0]=palette_.update(time,dt,hit);
    u.camera={cameraX,cameraY,static_cast<float>(travel_),s.cameraFocal};
    u.view={yaw,pitch,cameras_.badge(time),followBlend};
    u.geometry={s.reach,s.runup,s.crest,s.swell};
    u.effects={s.breaker?pulseStrength*s.breakerForce:0,0,0,static_cast<float>(hitZ_)};
    u.land={s.spacing,s.density,s.motion,s.horizon};
    u.features={float(s.props),float(s.seed),s.shoreBend,s.advance};
    u.weather={s.rainFall,s.rainFade,s.rainStreak,s.retreatSeconds};
    u.mountains={s.mountainSetback,s.mountainRange?s.mountainHeight:0,s.mountainWidth,s.mountainMotion};
    u.spray={s.spraySeconds,s.sprayHeight,float(s.sprayDrops),0};
    u.ocean={s.swellHeight,s.swellDistance,s.swellLength,s.swellSpeed};
    u.wildlife={eventPhase(dolphinStart_,s.dolphinDuration),eventPhase(fishStart_,s.fishDuration),
                 s.turtles?s.turtleSpeed:0,s.dolphinDuration};
    const auto dolphinPhase=u.wildlife[0];
    const auto ease=[](float x){
        const float t=std::clamp(x,0.f,1.f);
        return t*t*(3.f-2.f*t);
    };
    const float previewBlend=previewingDolphin_&&dolphinPhase>=0
        ?ease(dolphinPhase/.19f)*ease((1.f-dolphinPhase)/.24f):0.f;
    u.beachDolphin={dolphinX_,dolphinZ_,dolphinDirection_,previewBlend};
    u.beachWater={s.clarity,s.caustics,s.chop,s.interactions};
    u.beachDetail={s.shoreDetail,s.duneHeight,s.skyColor,s.sandTexture};
    u.beachFollow={float(s.followTurtle),s.followDistance,0,0};
    const double elapsed=time-oceanStart_;
    constexpr double firstVisit=4.;
    u.beachVisitor={};
    if(s.visitor&&elapsed>=firstVisit){
        const double seconds=elapsed-firstVisit;
        const double visit=std::floor(seconds/s.visitInterval);
        const double local=std::fmod(seconds,s.visitInterval);
        if(visit!=visitNumber_){
            const float forward=std::cos(yaw);
            const float distance=std::lerp(2.f,17.f,std::abs(forward));
            visitorZ_=float(travel_)+std::copysign(distance,forward)+1.22f;
            visitNumber_=visit;
        }
        if(local<s.visitDuration){
            const auto remaining=beat_.remaining(time,.51);
            const float bob=std::sin(remaining*std::numbers::pi_v<float>)*remaining*s.headBob;
            u.beachVisitor={visitorZ_,bob,float(std::fmod(visit*.237,.94)),1};
        }
    }
    for(size_t index=0;index<u.beachTurtles.size();++index){
        const double age=diveAge(index);
        const double ramp=std::min(2.7,s.turtleDive*.33);
        float depth=0,pitch=0;
        if(s.turtleDive>0&&age<s.turtleDive){
            const auto ease=[](double x){return x*x*(3-2*x);};
            const double down=std::min(1.,age/ramp);
            const double up=std::min(1.,(s.turtleDive-age)/ramp);
            depth=s.turtleDepth*float(ease(std::min(down,up)));
            pitch=.61f*float(4*up*(1-up)-4*down*(1-down));
        }
        const double swim=std::remainder(elapsed*s.turtleSpeed*1.2+index*2.39,500*double(Tau));
        u.beachTurtles[index]={depth,pitch,float(swim),0};
    }
    const float remaining=beat_.remaining(time,s.buildingSeconds);
    const float phase=1-remaining;
    const float ramp=std::min(phase/.2f,1.f);
    const float pulse=remaining*ramp*ramp*(3-2*ramp);
    u.buildings={1+s.buildingGrowth*pulse,s.buildingSwing*pulse*std::sin(phase*Tau),
                 float(std::remainder(time*s.tideSpeed*Tau,Tau)),s.tideMotion};
    for(size_t index=0;index<impacts_.size();++index){
        const auto& impact=impacts_[index];
        const float age=float(time-impact.start)-.095f;
        u.impacts[index]={impact.z,impact.start<0||age<0||age>=s.spraySeconds+2.f?-1.f:age/s.spraySeconds,
                          impact.power,impact.seed};
    }
    copyWave(frame,u);

    if(s.waves&&time>=nextWave_){
        waves_[waveIndex_++%waves_.size()]={time,.56f+activity_*.67f,activity_,sample(0,1)};
        nextWave_=time+sample(s.minimumGap,s.maximumGap);
    }
    for(size_t index=0;index<waves_.size();++index){
        const auto& wave=waves_[index];
        const float age=static_cast<float>(time-wave.start);
        u.waves[index]={wave.start<0||age>=s.travelSeconds+s.retreatSeconds?-1.f:age/s.travelSeconds,
                        wave.strength,wave.offset,wave.seed};
    }
    u.detail={s.travelSeconds,s.returnDistance,s.cameraPitch,std::max(s.rainHeight,s.cameraHeight*maxRise+s.rainClearance)};

    const float lowRate=s.notes?s.low.dropsPerSecond*low_:0;
    const float highRate=s.notes?s.high.dropsPerSecond*high_:0;
    rainBudget_=std::min(8.f,rainBudget_+float(dt)*(lowRate+highRate));
    while(rainBudget_>=1){
        rainBudget_-=1;
        const bool big=sample(0,lowRate+highRate)<lowRate;
        const float distance=sample(0,1);
        const float side=std::abs(std::sin(yaw));
        float z=static_cast<float>(travel_)+(4+distance*distance*s.rainDepth)*
            (sample(0,1)<.5f*side?-1.f:1.f);
        float x=sample(-s.rainWidth*.62f,s.rainWidth*.38f);
        float propTop=0;
        if(s.rainOnProps&&s.density>0&&sample(0,1)<s.propSplashChance){
            constexpr std::array anchors{3.1f,10.3f,5.85f,1.35f,2.6f};
            constexpr std::array tops{.25f,2.77f,2.9f,.97f,.4f};
            const auto kind=std::min(anchors.size()-1,static_cast<size_t>(sample(0,float(anchors.size()))));
            if(s.props&(1u<<kind)){
                z=(std::floor(z/s.spacing)+.45f)*s.spacing;
                x=shore(z)+anchors[kind];
                propTop=tops[kind];
            }
        }
        const float coast=shore(z);
        const float floor=std::clamp((x-coast)*.065f,0.f,.7f)+propTop;
        rain_[rainIndex_++%rain_.size()]={time,x,z,floor,big?s.low.splash:s.high.splash,sample(.58f,1.f),x<coast};
    }
    for(size_t index=0;index<rain_.size();++index){
        const auto& drop=rain_[index];
        const float age=static_cast<float>(time-drop.start);
        u.rain[index]={drop.x,drop.height,drop.z,drop.start<0||age>s.rainFall+s.rainFade?-1.f:age};
        u.rainStyle[index]={drop.size,drop.intensity,drop.ocean?1.f:0.f,0};
    }
}

UnderSeaMotion::UnderSeaMotion(const SceneIni& ini,bool canyon)
    :events_{{
        {ini.number("kelp.break_interval",125.,1.,7200.),ini.number("kelp.drift_seconds",12.,1.,90.),0,ini.enabled("kelp.enabled",true)},
        {ini.number("dolphins.interval",410.,1.,14400.),ini.number("dolphins.duration",13.,2.,90.),0,ini.enabled("dolphins.enabled",true)},
        {ini.number("whale.interval",2100.,1.,28800.),ini.number("whale.duration",24.,3.,150.),0,ini.enabled("whale.enabled",true)},
        {ini.number("turtle.interval",600.,1.,28800.),ini.number("turtle.duration",22.,3.,120.),0,ini.enabled("turtle.enabled",true)}
     }},
     seed_(ini.number("reef.seed",0u,0u,65535u)),
     kelpCount_(events_[KelpBreak].enabled?ini.number("kelp.count",12u,1u,12u):0u),
     fishPerSchool_(ini.number("fish.per_school",6u,0u,9u)),
     coralCount_(ini.number("reef.coral_count",14u,1u,20u)),
     seahorses_(ini.number("reef.seahorses",4u,0u,12u)),
     kelpGrowth_(ini.number("kelp.growth_seconds",120.f,2.f,900.f)),
     kelpBend_(ini.number("kelp.waveform_bend",.85f,0.f,2.5f)),
     caustics_(ini.number("water.caustic_strength",.87f,0.f,2.f)),
     causticSpeed_(ini.number("water.caustic_speed",.63f,0.f,3.f)),
     bubbles_(ini.number("water.bubble_rate",.42f,0.f,2.f)),
     fishSpeed_(ini.number("fish.speed",.8f,.05f,3.f)),
     fishRush_(ini.number("fish.beat_speedup",2.1f,0.f,6.f)),
     currentSeconds_(ini.number("current.reversal_seconds",15.f,10.f,20.f)),
     currentStrength_(ini.number("current.strength",1.f,0.f,2.f)),
     currentAudio_(ini.number("current.audio_influence",.26f,0.f,.7f)),
     refraction_(ini.number("water.surface_refraction",1.f,0.f,2.f)),
     coralDetail_(ini.number("reef.detail",1.f,0.f,1.f)),
     connections_(ini.number("reef.connectivity",.72f,0.f,1.f)),
     canyonWidth_(ini.number("canyon.half_width",4.9f,3.8f,8.f)),
     wallHeight_(ini.number("canyon.wall_height",8.f,4.f,10.f)),
     cameraSeconds_(ini.number("camera.seconds",35.,0.,600.)),
     cameraTransition_(ini.number("camera.transition_fraction",.18,0.,.65)),
     driftSpeed_(ini.number("camera.drift_speed",.19f,.01f,1.2f)),
     focal_(ini.number("camera.focal",1.25f,.75f,2.3f)),
     aquarium_(ini.enabled("camera.aquarium",true)),glide_(ini.enabled("camera.glide",true)),
     canyon_(canyon),
     palette_(ini){
    if(!aquarium_&&!glide_)throw std::runtime_error("Enable at least one UnderSea camera");
    if(!seed_)seed_=1+std::random_device{}()%65535;
    if(canyon_)for(auto& event:events_)event.enabled=false;
    planting_.fill(-kelpGrowth_);
    planting_.back()=-kelpGrowth_*.62f;
}

void UnderSeaMotion::start(Event event,double time){
    current_=event;
    eventStart_=time;
    events_[event].due=time+events_[event].interval;
    if(event==KelpBreak){
        const auto oldest=std::min_element(planting_.begin(),planting_.begin()+kelpCount_);
        detached_=static_cast<unsigned>(oldest-planting_.begin());
        detachedStart_=time;
        *oldest=time;
    }else{
        detachedStart_=-1;
    }
}

void UnderSeaMotion::update(const visualizers::MusicFrame& frame,double time,double dt,SceneUniforms& u){
    if(lastTime_<0||time<lastTime_){
        for(auto& event:events_)event.due=event.enabled?time+event.interval:0;
        current_=EventCount;detachedStart_=-1;swimClock_=0;swell_=0;sceneStart_=time;
        planting_.fill(time-kelpGrowth_);
        if(kelpCount_>2)planting_[kelpCount_-1]=time-kelpGrowth_*.62f;
    }
    lastTime_=time;
    if(current_!=EventCount&&time-eventStart_>=events_[current_].duration)current_=EventCount;
    if(previewPending_){
        previewPending_=false;
        for(unsigned offset=0;offset<EventCount;++offset){
            const auto event=static_cast<Event>((nextPreview_+offset)%EventCount);
            if(!events_[event].enabled)continue;
            start(event,time);
            nextPreview_=(event+1)%EventCount;
            break;
        }
    }
    if(current_==EventCount){
        double first=time;
        for(unsigned n=0;n<EventCount;++n){
            if(events_[n].enabled&&events_[n].due<=first){
                first=events_[n].due;
                current_=static_cast<Event>(n);
            }
        }
        if(current_!=EventCount)start(current_,time);
    }
    const bool hit=beat_.update(time,frame.beatCount,frame.beat);
    energy_=smooth(energy_,std::clamp(float(frame.rms)*4.f,0.f,1.f),dt,4.f);
    rush_=smooth(rush_,beat_.remaining(time,.72),dt,13.f);
    float waveform=0;
    for(size_t i=0;i<frame.wave.size();i+=16)waveform+=std::abs(frame.wave[i]);
    swell_=smooth(swell_,std::clamp(waveform*4.f/float(frame.wave.size()/16),0.f,1.f),dt,.29f);
    swimClock_+=float(dt)*fishSpeed_*(1.f+fishRush_*rush_);
    const auto camera=cameras_.frame(time,cameraSeconds_>0?cameraSeconds_:1e20,
                                     cameraTransition_,unsigned(aquarium_)+unsigned(glide_));
    const auto drift=[this](unsigned index){return (!aquarium_||index==1)?1.f:0.f;};
    const float moving=std::lerp(drift(camera.from),drift(camera.to),float(camera.blend));
    const float wave=float(time)*driftSpeed_;
    const float elapsed=float(time-sceneStart_);
    u.viewport[2]=float(time);
    u.audio={energy_,rush_,0,0};
    u.output[0]=palette_.update(time,dt,hit);
    u.camera=canyon_?Vec4{std::sin(elapsed*.16f)*.61f,3.15f+1.34f*moving,-7.8f+elapsed*driftSpeed_,focal_}
                    :Vec4{moving*std::sin(wave)*1.65f,3.25f+moving*std::sin(wave*.76f)*.64f,
                           -7.8f+moving*(1-std::cos(wave*.7f))*.92f,focal_};
    u.view={moving,cameras_.badge(time),canyon_?1.f:0.f,0};
    u.reef={float(seed_),float(kelpCount_),float(fishPerSchool_),float(coralCount_)};
    u.marine={current_==EventCount?0.f:float(current_+1),
              current_==EventCount?0.f:float((time-eventStart_)/events_[current_].duration),
              swimClock_,float(seahorses_)};
    u.water={kelpBend_,caustics_,causticSpeed_,bubbles_};
    const double period=2.*currentSeconds_;
    u.reefCurrent={float(std::fmod(time,period)*Tau/period),
                   currentStrength_,swell_*currentAudio_,refraction_};
    u.reefTerrain={coralDetail_,connections_,canyonWidth_,wallHeight_};
    const float detachedAge=detachedStart_<0?-1.f:float((time-detachedStart_)/events_[KelpBreak].duration);
    u.detachedKelp={float(detached_),detachedAge<1?detachedAge:-1.f,0,0};
    for(size_t n=0;n<planting_.size();++n)
        u.kelp[n/4][n%4]=std::clamp(float((time-planting_[n])/kelpGrowth_),0.f,1.f);
}

ObjectsMotion::ObjectsMotion(const SceneIni& i,unsigned start)
    :seconds_(i.number("objects.seconds_per_shape",18.f,1.f,600.f)),
     distance_(i.number("camera.distance",5.1f,2.7f,14.f)),elevation_(i.number("camera.elevation",.27f,-1.f,1.f)),
     orbit_(i.number("camera.orbit_speed",.13f,-2.f,2.f)),focal_(i.number("camera.focal",2.2f,.6f,3.5f)),
     initialAngle_(i.number("camera.angle",.4f,-Tau,Tau)),light_(i.number("render.light",1.28f,.1f,4.f)),
     detail_(i.number("render.detail",1.f,.5f,2.f)),occlusion_(i.number("render.occlusion",.45f,0.f,1.f)),
     backdrop_(i.number("render.backdrop",1.f,0.f,2.f)),
     baseSpin_(i.number("rotation.idle_speed",.31f,0.f,4.f)),waveSpin_(i.number("rotation.waveform_gain",2.1f,0.f,9.f)),
     lowMin_(i.number("notes.low_minimum_hz",42.f,20.f,999.f)),
     lowMax_(i.number("notes.low_maximum_hz",260.f,21.f,1000.f)),
     highMin_(i.number("notes.high_minimum_hz",1700.f,500.f,16000.f)),
     highMax_(i.number("notes.high_maximum_hz",12000.f,501.f,20500.f)),
     lowTwist_(i.number("notes.low_twist",.26f,0.f,1.5f)),highTwist_(i.number("notes.high_twist",.19f,0.f,1.5f)),
     twistSpeed_(i.number("notes.twist_speed",2.4f,.1f,12.f)),
     threshold_(i.number("notes.threshold",.008f,.0001f,.5f)),
     beatPeak_(i.number("beat.peak_percent",165.f,100.f,250.f)/100),
     beatSeconds_(i.number("beat.return_seconds",.5f,.02f,8.f)),
     steps_(i.number("render.steps",64u,24u,104u)),beatScale_(i.enabled("beat.enabled",true)),
     waveRotate_(i.enabled("rotation.waveform_enabled",true)),notes_(i.enabled("notes.enabled",true)),
     orbitEnabled_(i.enabled("camera.orbit_enabled",true)),palette_(i){
    if(lowMin_>=lowMax_||highMin_>=highMax_)throw std::runtime_error("Objects note sensor minimum_hz must be below maximum_hz");
    constexpr const char* names[]={"armillary","gyroid","knot","prism","satellites"};
    for(unsigned index=0;index<shapes_.size();++index){
        const auto prefix=std::string(names[index])+".";
        auto& shape=shapes_[index];
        shape={i.enabled(prefix+"enabled",true),i.number(prefix+"size",1.f,.45f,1.65f),
               i.number(prefix+"spin",1.f,0.f,3.f),i.number(prefix+"twist",1.f,0.f,3.f),
               i.number(prefix+"beat_gain",1.f,0.f,1.5f)};
        if(shape.enabled)enabled_[count_++]=index;
    }
    if(!count_)throw std::runtime_error("Enable at least one object in objects.ini");
    if(i.enabled("objects.vary_start",true))shapesCycle_.startAt(start%count_);
}
void ObjectsMotion::update(const visualizers::MusicFrame& frame,double time,double dt,SceneUniforms& u){
    const auto mode=shapesCycle_.frame(time,seconds_,0,static_cast<unsigned>(count_),false);
    const unsigned kind=enabled_[mode.from];
    const auto& shape=shapes_[kind];
    const bool hit=beat_.update(time,frame.beatCount,frame.beat);
    const float drop=beat_.remaining(time,beatSeconds_);
    const float energy=std::clamp(static_cast<float>(frame.rms)*5,0.f,1.f);
    energy_=smooth(energy_,energy,dt,6);
    const float pitchLow=notes_?bandLevel(spectral(frame,lowMin_,lowMax_),threshold_):0;
    const float pitchHigh=notes_?bandLevel(spectral(frame,highMin_,highMax_),threshold_):0;
    low_=smooth(low_,pitchLow,dt,10);
    high_=smooth(high_,pitchHigh,dt,10);
    std::array<float,3> wave{};
    for(size_t n=1;n<frame.wave.size();++n){
        const float movement=std::abs(frame.wave[n]-frame.wave[n-1]);
        wave[n%3]+=movement;
    }
    for(size_t n=0;n<angle_.size();++n){
        const float response=waveRotate_?waveSpin_*(std::min(1.f,wave[n]/250)+energy_*.55f):0;
        angle_[n]=std::remainder(angle_[n]+float(dt)*shape.spin*(baseSpin_*(.54f+float(n)*.39f)+response),Tau);
    }
    const float beatSize=beatScale_?(beatPeak_-1)*shape.beatGain*drop*(.35f+.65f*energy_):0;
    const float orbitAngle=initialAngle_+(orbitEnabled_?static_cast<float>(std::remainder(time*orbit_,Tau)):0);
    u.viewport[2]=static_cast<float>(time);
    u.audio={energy_,drop,low_,high_};
    u.output[0]=palette_.update(time,dt,hit);
    u.camera={distance_,elevation_,orbitAngle,focal_};
    u.view[2]=shapesCycle_.badge(time);
    u.geometry={float(kind),shape.size*(1+beatSize),angle_[0],angle_[1]};
    const float phase=static_cast<float>(std::remainder(time*twistSpeed_,Tau));
    u.effects={low_*lowTwist_*shape.twist*std::sin(phase),
               high_*highTwist_*shape.twist*std::cos(1.37f*phase),phase,angle_[2]};
    u.detail={float(steps_),detail_,occlusion_,backdrop_};
    u.features[0]=light_;
}
} // namespace terrain
