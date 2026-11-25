// SPDX-License-Identifier: MIT
#pragma once
#include "context.hpp"
#include "music_analysis.hpp"
#include "scene_settings.hpp"
#include <array>
#include <random>

namespace terrain {
using Vec4 = std::array<float,4>;
inline constexpr size_t WaveSlots=24, RainSlots=32;
inline constexpr size_t ImpactSlots=4;
inline constexpr size_t KelpSlots=12;
inline constexpr size_t BeachTurtles=3;
inline constexpr unsigned BeachTurtleCamera=9;
inline constexpr std::array<const char*,10> BeachCameras={"shoreline","over_water","promenade","shore_facing","overhead_shore","ocean_facing","overhead_ocean","diagonal_shore","diagonal_ocean","turtle_follow"};

struct SceneUniforms {
    Vec4 viewport{},audio{},output{},camera{},geometry{},effects{},land{},features{},weather{},detail{},view{};
    Vec4 mountains{},spray{},buildings{};
    std::array<Vec4,8> waveform{};
    std::array<Vec4,WaveSlots> waves{};
    std::array<Vec4,RainSlots> rain{},rainStyle{};
    std::array<Vec4,ImpactSlots> impacts{};
    Vec4 ocean{},wildlife{},reef{},marine{},water{},detachedKelp{};
    std::array<Vec4,KelpSlots/4> kelp{};
    Vec4 beachWater{},beachDetail{},beachFollow{},beachVisitor{},beachDolphin{};
    std::array<Vec4,BeachTurtles> beachTurtles{};
    Vec4 reefCurrent{},reefTerrain{};
};
static_assert(sizeof(SceneUniforms)==(14+8+WaveSlots+2*RainSlots+ImpactSlots+13+KelpSlots/4+BeachTurtles)*16);

class EnvironmentTarget {
    visualizers::gpu::Context& context_;
    wgpu::RenderPipeline pipeline_;
    wgpu::Buffer uniform_;
    wgpu::BindGroup bindings_;
    wgpu::Texture target_;
    wgpu::TextureView view_;
public:
    EnvironmentTarget(visualizers::gpu::Context&,const std::string&,unsigned width,unsigned height);
    void resize(unsigned width,unsigned height);
    void draw(SceneUniforms&);
    const wgpu::Texture& texture() const {return target_;}
};

struct NoteSettings {
    bool enabled=true;
    float minimumHz,maximumHz,threshold,dropsPerSecond,splash;
};

class AudioPalette {
    float displayed_,hue_,fadeSeconds_;
    double interval_,lastCycle_=0;
    unsigned beats_,count_=0;
    float hueStep_;
    bool enabled_;
public:
    explicit AudioPalette(const SceneIni&);
    float update(double time,double dt,bool hit);
    void advance();
};

class BeachMotion {
    struct Settings {
        bool waves,breaker,notes,rainOnProps,dolphins,fish,turtles;
        std::array<bool,BeachCameras.size()> cameras{};
        unsigned props,seed;
        float minimumGap,maximumGap,travelSeconds,retreatSeconds,reach,runup,returnDistance,crest,advance,swell;
        float breakerSeconds,breakerForce;
        float cameraSeconds,cameraHeight,cameraSpeed,cameraFocal,cameraPitch,cameraTransition,rainClearance;
        float density,motion,spacing,horizon,shoreBend;
        float rainFall,rainFade,rainDepth,rainWidth,rainStreak,rainHeight,propSplashChance;
        bool mountainRange;
        float mountainSetback,mountainHeight,mountainWidth,mountainMotion;
        float spraySeconds,sprayHeight,nearRock,farRock,sequenceGap,rainbowChance;
        unsigned sprayDrops,approachBeats,rocksEvery;
        float buildingSeconds,buildingGrowth,buildingSwing,tideSpeed,tideMotion;
        float dolphinSeconds,fishSeconds,dolphinDuration,fishDuration,turtleSpeed;
        float turtleSurface,turtleDive,turtleDepth;
        float swellHeight,swellDistance,swellLength,swellSpeed;
        float clarity,caustics,chop,interactions,shoreDetail,duneHeight,skyColor,sandTexture;
        unsigned followTurtle;
        float followDistance;
        bool visitor;
        float visitInterval,visitDuration,headBob;
        NoteSettings low,high;
        explicit Settings(const SceneIni&);
    } settings_;
    struct Wave {double start=-1;float strength=0,offset=0,seed=0;};
    struct Drop {double start=-1;float x=0,z=0,height=0,size=0,intensity=0;bool ocean=false;};
    struct Impact {double start=-1;float z=0,power=0,seed=0;};
    std::array<Wave,WaveSlots> waves_{};
    std::array<Drop,RainSlots> rain_{};
    std::array<Impact,ImpactSlots> impacts_{};
    size_t waveIndex_=0,rainIndex_=0;
    unsigned impactIndex_=0,approachIndex_=0;
    unsigned rockBeat_=0;
    std::mt19937 random_;
    double nextWave_=0,lastTime_=-1,travel_=0,hitZ_=0,lastRockBeat_=-1;
    double nextDolphin_=0,nextFish_=0,dolphinStart_=-1,fishStart_=-1;
    float dolphinX_=0,dolphinZ_=0,dolphinDirection_=1;
    bool previewDolphinPending_=false,previewingDolphin_=false;
    double oceanStart_=0;
    std::array<double,BeachTurtles> turtleDiveStart_{};
    bool followingTurtle_=false;
    double visitNumber_=-1;
    float visitorZ_=0;
    float low_=0,high_=0,activity_=0,rainBudget_=0;
    BeatTrigger beat_;
    ModeCycle cameras_;
    AudioPalette palette_;
    float sample(float low,float high);
public:
    explicit BeachMotion(const SceneIni& ini):settings_(ini),random_(settings_.seed),palette_(ini){}
    void update(const visualizers::MusicFrame&,double time,double dt,SceneUniforms&);
    void nextMode(){cameras_.next();}
    void toggleModeLock(){cameras_.toggleLock();}
    void previewDolphin(){previewDolphinPending_=settings_.dolphins;}
};

class ObjectsMotion {
    struct Shape { bool enabled;float size,spin,twist,beatGain; };
    std::array<Shape,5> shapes_;
    std::array<unsigned,5> enabled_{};
    size_t count_=0;
    float seconds_,distance_,elevation_,orbit_,focal_,initialAngle_,light_,detail_,occlusion_,backdrop_;
    float baseSpin_,waveSpin_,lowMin_,lowMax_,highMin_,highMax_,lowTwist_,highTwist_,twistSpeed_,threshold_,beatPeak_,beatSeconds_;
    unsigned steps_;
    bool beatScale_,waveRotate_,notes_,orbitEnabled_;
    std::array<float,3> angle_{};
    float low_=0,high_=0,energy_=0;
    BeatTrigger beat_;
    ModeCycle shapesCycle_;
    AudioPalette palette_;
public:
    explicit ObjectsMotion(const SceneIni&,unsigned start=0);
    void update(const visualizers::MusicFrame&,double time,double dt,SceneUniforms&);
    void nextMode(){shapesCycle_.next();}
    void toggleModeLock(){shapesCycle_.toggleLock();}
};

class UnderSeaMotion {
    enum Event : unsigned { KelpBreak, Dolphin, Whale, Turtle, EventCount };
    struct TimedEvent { double interval,duration,due=0; bool enabled; };
    std::array<TimedEvent,EventCount> events_;
    std::array<double,KelpSlots> planting_{};
    unsigned seed_,kelpCount_,fishPerSchool_,coralCount_,seahorses_;
    float kelpGrowth_,kelpBend_,caustics_,causticSpeed_,bubbles_,fishSpeed_,fishRush_;
    float currentSeconds_,currentStrength_,currentAudio_,refraction_,coralDetail_,connections_,canyonWidth_,wallHeight_;
    double cameraSeconds_,cameraTransition_,lastTime_=-1,eventStart_=0;
    double detachedStart_=-1,sceneStart_=0;
    float driftSpeed_,focal_,swimClock_=0,energy_=0,rush_=0,swell_=0;
    bool aquarium_,glide_,canyon_;
    unsigned nextPreview_=0,detached_=0;
    bool previewPending_=false;
    Event current_=EventCount;
    BeatTrigger beat_;
    ModeCycle cameras_;
    AudioPalette palette_;
    void start(Event,double time);
public:
    explicit UnderSeaMotion(const SceneIni&,bool canyon=false);
    void update(const visualizers::MusicFrame&,double time,double dt,SceneUniforms&);
    void nextMode(){if(canyon_)nextCamera();else previewPending_=true;}
    void nextCamera(){cameras_.next();}
    void toggleModeLock(){cameras_.toggleLock();}
};
} // namespace terrain
