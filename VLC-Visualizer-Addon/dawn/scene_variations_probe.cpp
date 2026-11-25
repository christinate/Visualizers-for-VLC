// SPDX-License-Identifier: GPL-2.0-or-later
#include "environment.hpp"
#include "plugin.hpp"
#include "scene_dawn.hpp"
#include <fstream>
#include <iostream>
#include <set>

namespace fs=std::filesystem;
using namespace visualizers;
using namespace visualizers::gpu;
using json=nlohmann::json;

namespace {
void require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
void modeLock(){
    terrain::ModeCycle cycle;
    require(cycle.frame(2.5,2,.2,3).from==1,"Incorrect starting mode");
    cycle.toggleLock();
    auto first=cycle.frame(2.6,2,.2,3);
    require(first.from==1&&first.to==1&&cycle.badge(2.6)==1,"Lock did not hold the displayed mode");
    cycle.next();
    require(cycle.frame(8,2,.2,3).from==1&&cycle.badge(8)==0,"Locked mode advanced or badge persisted");
    cycle.toggleLock();
    require(cycle.frame(8.1,2,.2,3).from==1&&cycle.badge(8.1)==-1,"Unlock did not show the open lock");
    const float fading=cycle.badge(9.9);
    require(fading<0&&fading>-1&&cycle.badge(10.2)==0,"Unlock badge did not fade");
    require(cycle.frame(10.1,2,.2,3).from==2,"Unlock did not resume automatic cycling");
}
fs::path sceneAssets(const fs::path& assets,const std::string& identifier){
    return assets/(identifier=="coralcanyon"?"undersea":identifier);
}
terrain::SceneIni defaults(const fs::path& assets,const std::string& identifier){
    terrain::SceneIni ini;
    ini.apply(terrain::readSceneFile(sceneAssets(assets,identifier)/(identifier+".ini"),32768));
    return ini;
}
MusicFrame signal(){
    MusicFrame frame;
    frame.rms=.25;
    frame.spectrum[4]=.26f;
    frame.spectrum[112]=.22f;
    for(size_t n=0;n<frame.wave.size();++n)
        frame.wave[n]=.5f*std::sin(float(n)*.085f)+.21f*std::sin(float(n)*.42f);
    return frame;
}
void dumpImage(const fs::path& path,const std::vector<uint16_t>& pixels,unsigned width,unsigned height){
    std::ofstream image(path,std::ios::binary);
    image<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(size_t n=0;n<pixels.size();n+=4)
        for(unsigned c=0;c<3;++c){
            const float sample=decodeHalf(pixels[n+c]);
            require(std::isfinite(sample),"Scene output contains a non-finite pixel");
            image.put(static_cast<char>(std::lround(std::clamp(sample,0.f,1.f)*255)));
        }
    require(bool(image),"Cannot save scene preview");
}
void terrainTiming(Context& context,const fs::path& assets,json& result){
    auto settings=defaults(assets,"terrain");
    settings.apply("[camera]\nseconds=2\ntransition_fraction=0\n[height]\nreturn_seconds=.5\n"
                   "[width]\nreturn_seconds=.75\n");
    terrain::DawnScene scene(context,terrain::readSceneFile(assets/"terrain/terrain.wgsl",65536),96,64);
    scene.setMotion(terrain::TerrainMotion::fromIni(settings));
    auto audio=signal();
    const auto step=[&](double time,unsigned beat){
        audio.beatCount=beat;
        const auto mode=scene.update(audio,time);
        context.readRgba16(scene.texture(),96,64);
        return mode;
    };
    require(step(0,0)==0&&near(scene.uniforms()[7],0),"Flyover starts at baseline height");
    require(step(.1,1)==0&&near(scene.uniforms()[7],1)&&near(scene.uniforms()[11],0),"Height did not jump to 200%");
    require(step(.35,1)==0&&near(scene.uniforms()[7],.5),"Height did not interpolate");
    require(step(.6,1)==0&&near(scene.uniforms()[7],0),"Height did not return at half a second");
    require(step(4.0,2)==2&&near(scene.uniforms()[11],1)&&near(scene.uniforms()[7],0),"Width did not jump on Sidewall");
    require(step(4.375,2)==2&&near(scene.uniforms()[11],.5),"Custom width timing was ignored");
    require(step(4.75,2)==2&&near(scene.uniforms()[11],0),"Width did not restore");
    std::set<unsigned> seen;
    for(unsigned mode=0;mode<8;++mode)seen.insert(step(mode*2.,2));
    require(seen.size()==8,"Terrain camera cycle omitted a mode");
    settings.apply("[width]\nenabled=false\n[flyover]\nenabled=false\n[underside]\nenabled=false\n");
    scene.setMotion(terrain::TerrainMotion::fromIni(settings));
    require(step(0,3)==2&&near(scene.uniforms()[11],0),"Disabled camera or width effect remained active");
    scene.nextMode();step(.1,3);
    require(step(.85,3)==3,"Terrain space control did not advance to the next enabled camera");
    auto maximum=defaults(assets,"terrain");
    maximum.apply("[camera]\nseconds=2\ntransition_fraction=.2\n[height]\npeak_percent=450\n[width]\npeak_percent=400\n");
    scene.setMotion(terrain::TerrainMotion::fromIni(maximum));
    std::vector<uint16_t> saturated(256*128*4);
    for(size_t index=0;index<saturated.size();index+=4){
        saturated[index]=saturated[index+2]=saturated[index+3]=encodeHalf(1);
        saturated[index+1]=encodeHalf(2);
    }
    scene.setHistory(saturated,.5);
    audio=signal();audio.rms=1;audio.beatEnvelope=2;audio.wave.fill(1);
    for(unsigned mode=0;mode<8;++mode){
        for(double phase:{.0,1.85}){
            audio.beatCount=mode+1;
            audio.beat=phase==0;
            scene.update(audio,mode*2.+phase);
            const auto pixels=context.readRgba16(scene.texture(),96,64);
            std::set<uint16_t> shades;
            for(size_t index=0;index<pixels.size();index+=4)shades.insert(pixels[index]);
            require(shades.size()>32,"Saturated Terrain geometry blanketed a camera or transition");
        }
    }
    result["terrain"]={{"angles",8},{"height_peak_percent",200},{"height_seconds",.5},{"width_seconds_override",.75}};
}
void beachMotion(const fs::path& assets,json& result){
    auto settings=defaults(assets,"beach");
    terrain::AudioPalette beatPalette(settings),timedPalette(settings);
    for(unsigned n=0;n<3;++n)
        require(near(beatPalette.update(n,.1,true),.52f),"Beach palette changed before the fourth beat");
    require(beatPalette.update(3,.1,true)>.52f,"Beach palette did not respond to beats");
    require(near(timedPalette.update(59.9,.1,false),.52f)&&timedPalette.update(60,.1,false)>.52f,
            "Beach palette did not rotate on its one-minute timer");
    terrain::BeachMotion motion(settings);
    terrain::SceneUniforms u;
    auto audio=signal();
    double previous=-1;
    unsigned spawns=0;
    bool low=false,high=false,far=false,prop=false,oceanRain=false,retreat=false;
    for(unsigned n=0;n<700;++n){
        const double time=n*.01;
        audio.beat=n==101;
        audio.beatCount=n>=101?1:0;
        motion.update(audio,time,.01,u);
        if(n==101)require(u.effects[0]>.99f,"Beach breaker did not trigger");
        for(const auto& wave:u.waves){
            retreat=retreat||wave[0]>1;
            if(wave[0]!=0)continue;
            if(previous>=0)require(time-previous>=.299&&time-previous<=.711,"Wave spawn interval outside configured range");
            previous=time;++spawns;
        }
        for(size_t i=0;i<u.rain.size();++i){
            if(u.rain[i][3]<0)continue;
            low=low||near(u.rainStyle[i][0],.55f);
            high=high||near(u.rainStyle[i][0],.22f);
            far=far||u.rain[i][2]-u.camera[2]>60;
            prop=prop||u.rain[i][1]>.70f;
            oceanRain=oceanRain||u.rainStyle[i][2]>.5f;
        }
    }
    require(spawns>9&&retreat&&low&&high&&far&&prop&&oceanRain,"Beach lacks wave chains, separate rain bands or ocean rain");
    require(u.effects[0]==0&&u.detail[3]>u.camera[1],"Beach breaker or rain altitude is incorrect");
    auto anchored=settings;
    anchored.apply("[camera]\nspeed=0\nseconds=600\n[spray]\nrainbow_chance=1\n"
                   "[ocean_life]\nfish_interval=1\ndolphin_interval=2\n");
    terrain::BeachMotion sequence(anchored);
    float previousDistance=100;
    unsigned rockHits=0;
    bool fish=false,dolphin=false;
    for(unsigned beat=1;beat<=20;++beat){
        const double time=(beat-1)*.46;
        audio.beatCount=beat;audio.beat=true;
        sequence.update(audio,time,.016,u);
        fish=fish||u.wildlife[0]<0&&u.wildlife[1]==0;
        dolphin=dolphin||u.wildlife[0]==0;
        audio.beat=false;
        sequence.update(audio,time+.145,.016,u);
        const auto newest=std::find_if(u.impacts.begin(),u.impacts.end(),[](const auto& impact){
            return impact[1]>0&&impact[1]<.06f;
        });
        require((newest!=u.impacts.end())==(beat%4==0),"Beach rock spray did not follow fourth-beat cadence");
        if(newest!=u.impacts.end()){
            const float distance=std::abs((*newest)[0]-u.camera[2]);
            require(distance<previousDistance&&(*newest)[3]>=1,"Beach spray did not approach or mark its mist rainbow");
            previousDistance=distance;
            ++rockHits;
        }
        require(u.buildings[0]>1&&std::abs(u.buildings[1])>.01f,"Buildings did not grow and shimmy");
    }
    require(rockHits==5&&fish&&dolphin&&u.wildlife[2]>0&&u.ocean[0]>0,"Beach wildlife or offshore swells are missing");
    terrain::BeachMotion preview(anchored);
    auto quiet=MusicFrame{};
    preview.update(quiet,0.,.02,u);
    preview.previewDolphin();
    preview.update(quiet,1.,.02,u);
    require(u.wildlife[0]==0&&u.beachDolphin[3]==0,"Beach dolphin preview did not start without a beat");
    preview.update(quiet,1.8,.02,u);
    const auto placement=u.beachDolphin;
    require(u.wildlife[0]>0&&placement[3]==1,"Beach dolphin preview did not frame the leap");
    preview.update(quiet,2.6,.02,u);
    require(u.beachDolphin==placement,"Beach dolphin moved its shot during the leap");
    preview.update(quiet,3.5,.02,u);
    require(u.wildlife[0]<0&&u.beachDolphin[3]==0,"Beach did not return from the dolphin preview");
    quiet.beatCount=1;
    preview.update(quiet,3.6,.02,u);
    require(u.wildlife[0]==0&&u.beachDolphin[3]==0,"Dolphin preview changed the pending beat jump");
    auto oldProfile=anchored;
    oldProfile.apply("[environment]\nbushes=true\n[ocean_life]\ndolphins=false\n");
    terrain::BeachMotion disabledWildlife(oldProfile);
    disabledWildlife.previewDolphin();
    disabledWildlife.update(quiet,0.,.02,u);
    require(u.wildlife[0]<0&&!(unsigned(u.features[0])&(1u<<5)),
            "Disabled dolphins or shrubs from an older Beach profile were rendered");
    sequence.update(audio,12,.016,u);
    require(near(u.buildings[0],1)&&near(u.buildings[1],0),"Buildings did not settle after the beat");
    const auto dives=[&](double time){
        sequence.update(audio,time,.016,u);
        return std::array{u.beachTurtles[0][0],u.beachTurtles[1][0],u.beachTurtles[2][0]};
    };
    require(dives(20.)==std::array{0.f,0.f,0.f},"Beach turtles started underwater");
    require(dives(28.)==std::array{2.8f,0.f,0.f},"Beach turtles did not dive independently");
    require(dives(49.)==std::array{2.8f,2.8f,0.f},"Beach turtles surfaced before their dive elapsed");
    require(dives(55.)==std::array{0.f,2.8f,0.f},"Beach turtle did not resurface after 30 seconds");
    require(dives(66.)==std::array{0.f,2.8f,2.8f},"Third Beach turtle did not follow its own schedule");
    require(dives(0.)==std::array{0.f,0.f,0.f},"Beach turtles did not reset after a rewind");
    for(unsigned angle=0;angle<terrain::BeachCameras.size();++angle){
        motion.update(audio,24.*angle,.1,u);
        require(u.detail[3]-u.camera[1]>=3.99f,"Rain starts below an enabled Beach camera");
        if(angle==4||angle==6)require(u.view[1]>1.2f,"Overhead camera is not looking down");
        if(angle>=3&&angle<=6)require((u.view[0]>0)==(angle<=4),"Transverse Beach cameras face the wrong shore");
    }
    settings.apply("[waves]\nenabled=false\n[breaker]\nenabled=false\n[rain]\nenabled=false\n"
                   "[environment]\nanimals=false\n[shoreline]\nenabled=false\n[promenade]\nenabled=false\n"
                   "[palette]\nenabled=false\n");
    terrain::BeachMotion disabled(settings);
    disabled.update(audio,0,.5,u);
    require(u.effects[0]==0&&u.waves[0][0]<0&&u.rain[0][3]<0&&!(unsigned(u.features[0])&1)
            &&near(u.camera[0],-8.6f)&&near(u.output[0],.52f),"Beach disabled controls were ignored");
    result["beach"]={{"wave_spawns_in_fixture",spawns},{"camera_angles",terrain::BeachCameras.size()},{"rain_above_camera",true},
                     {"approaching_impacts",rockHits},{"beats_per_impact",4},{"beat_jumps",true},
                     {"beat_buildings",true},{"note_splash_radii",{.55,.22}},{"horizon_rain",far},
                     {"prop_rain",prop},{"ocean_rain",oceanRain},{"turtle_dive_seconds",30},
                     {"palette_beats",4},{"palette_seconds",60}};
}
void beachWater(Context& context,const fs::path& assets,const fs::path& output,json& result){
    auto options=defaults(assets,"beach");
    options.apply("[camera]\nspeed=0\n[waves]\nenabled=false\n[ocean_life]\nturtles=false\n"
                  "[palette]\nenabled=false\n");
    terrain::BeachMotion motion(options);
    terrain::SceneUniforms u{};
    MusicFrame silence{};
    motion.update(silence,0.,.1,u);
    u.camera={-7.4f,6.3f,0.f,1.19f};
    u.view={0.f,.75f,0.f,0.f};
    constexpr unsigned w=360,h=224;
    terrain::EnvironmentTarget scene(context,terrain::readSceneFile(assets/"beach/beach.wgsl",256*1024),w,h);
    const auto render=[&](const char* name){
        scene.draw(u);
        auto pixels=context.readRgba16(scene.texture(),w,h);
        if(name)dumpImage(output/name,pixels,w,h);
        return pixels;
    };
    const auto difference=[](const auto& a,const auto& b){
        unsigned changed=0;
        for(size_t n=0;n<a.size();n+=4){
            const float delta=std::abs(decodeHalf(a[n])-decodeHalf(b[n]))+
                std::abs(decodeHalf(a[n+1])-decodeHalf(b[n+1]))+
                std::abs(decodeHalf(a[n+2])-decodeHalf(b[n+2]));
            if(delta>.035f)++changed;
        }
        return changed;
    };
    u.rain[0]={-7.4f,0.f,6.2f,u.weather[0]-.06f};
    u.rainStyle[0]={.72f,1.f,1.f,0.f};
    const auto falling=render(nullptr);
    u.beachWater[3]=0.f;
    require(difference(falling,render(nullptr))==0,"Rain displaced Beach water before it hit");
    u.rain[0][3]=u.weather[0]+.31f;
    const auto settled=render(nullptr);
    u.beachWater[3]=1.f;
    const auto rain=render("beach-water-rain.ppm");
    const auto rainPixels=difference(settled,rain);
    require(rainPixels>30&&rainPixels<w*h/9,"Beach rain did not create a localized surface ripple");
    u.rainStyle[0][2]=0.f;
    const auto landRain=render(nullptr);
    u.beachWater[3]=0.f;
    require(difference(landRain,render(nullptr))==0,"Rain on land altered Beach water");
    u.rain[0][3]=-1.f;
    u.impacts[0]={7.2f,.70f,1.3f,.71f};
    const auto quiet=render(nullptr);
    u.beachWater[3]=1.f;
    const auto breaker=render("beach-water-rock.ppm");
    const auto rockPixels=difference(quiet,breaker);
    require(rockPixels>60&&rockPixels<w*h/6,"Rock spray did not locally disturb Beach water");
    u.impacts[0][1]=-1.f;
    const auto lit=render("beach-water-caustics.ppm");
    u.beachWater[1]=0.f;
    const auto causticPixels=difference(lit,render(nullptr));
    require(causticPixels>350,"Beach caustics did not light the refracted seabed");
    auto chase=defaults(assets,"beach");
    for(const char* camera:terrain::BeachCameras){
        if(std::string_view(camera)=="shoreline"||std::string_view(camera)=="turtle_follow")continue;
        chase.apply("["+std::string(camera)+"]\nenabled=false\n");
    }
    chase.apply("[camera]\nseconds=2\nspeed=0\ntransition_fraction=0\n"
                "[palette]\nenabled=false\n[rain]\nenabled=false\n");
    terrain::BeachMotion turtleCamera(chase);
    turtleCamera.update(silence,0.,.02,u);
    turtleCamera.update(silence,2.,.02,u);
    require(near(u.view[3],1)&&u.beachTurtles[0][0]==0,"Beach follow camera did not begin above water");
    const auto beforeDive=render("beach-turtle-surface.ppm");
    turtleCamera.update(silence,15.,.02,u);
    require(near(u.view[3],1)&&u.beachTurtles[0][0]>2.5f,"Beach follow camera left before the turtle dived");
    const auto underwater=render("beach-turtle-dive.ppm");
    require(difference(beforeDive,underwater)>w*h/4,"Beach dive did not change the rendering");
    unsigned darkSurface=0;
    for(unsigned n=0;n<w*(h/3);++n){
        const float light=std::max({decodeHalf(underwater[n*4]),decodeHalf(underwater[n*4+1]),
                                    decodeHalf(underwater[n*4+2])});
        if(light<.08f)++darkSurface;
    }
    require(darkSurface<w*h/30,"Beach's underwater surface rendered black at grazing angles");
    u.beachWater[1]=0.f;
    const auto reefLight=difference(underwater,render(nullptr));
    require(reefLight>200,"Beach follow camera did not show underwater caustics");
    turtleCamera.update(silence,35.,.02,u);
    require(near(u.view[3],1)&&u.beachTurtles[0][0]==0,"Beach follow camera left before the turtle resurfaced");
    const auto resurfaced=render("beach-turtle-resurface.ppm");
    require(difference(underwater,resurfaced)>w*h/4,"Beach did not render the resurfacing");
    turtleCamera.update(silence,39.,.02,u);
    require(u.view[3]==0,"Beach did not resume camera rotation after a full dive");
    terrain::BeachMotion manual(chase);
    manual.update(silence,0.,.02,u);
    manual.nextMode();
    manual.update(silence,.1,.02,u);
    manual.update(silence,.9,.02,u);
    require(near(u.view[3],1),"Beach could not select the follow camera with Space");
    manual.previewDolphin();
    manual.update(silence,1.,.02,u);
    manual.update(silence,1.9,.02,u);
    const auto dolphinShot=render("beach-dolphin-close-up.ppm");
    // Match the preview's side-on camera while removing its animal and spray.
    const auto dolphin=u.beachDolphin;
    const float offset=(u.wildlife[0]-.5f)*.67f;
    const float dx=1.14f*offset-5.8f;
    const float dy=.73f+1.16f*std::sin(u.wildlife[0]*3.14159f)-3.65f;
    const float dz=(7.3f*offset+1.34f)*dolphin[2];
    u.camera={dolphin[0]+5.8f,3.65f,dolphin[1]-1.34f*dolphin[2],1.56f};
    u.view={std::atan2(dx,dz),std::atan2(-dy,std::hypot(dx,dz)),0,0};
    u.beachDolphin[3]=0;
    u.wildlife[0]=-1;
    const auto dolphinPixels=difference(dolphinShot,render(nullptr));
    require(dolphinPixels>250&&dolphinPixels<w*h/6,"Beach dolphin did not render in its close-up");
    result["beach_dolphin_pixels"]=dolphinPixels;
    auto chairOptions=defaults(assets,"beach");
    chairOptions.apply("[camera]\nspeed=0\nseconds=600\n[environment]\ndensity=0\n"
                       "[palette]\nenabled=false\n[rain]\nenabled=false\n");
    terrain::BeachMotion chair(chairOptions);
    chair.update(silence,0.,.02,u);
    require(u.beachVisitor[3]==0,"Beach visitor appeared before her first visit");
    silence.beatCount=1;
    chair.update(silence,7.,.02,u);
    const float beachChairZ=u.beachVisitor[0];
    chair.update(silence,7.16,.02,u);
    require(u.beachVisitor[3]==1&&u.beachVisitor[1]>.04f&&u.beachVisitor[0]==beachChairZ,
            "Beach visitor did not stay in place and nod to the beat");
    const float guestShore=u.features[2]*(std::sin(beachChairZ*.12f)+.65f*std::sin(beachChairZ*.041f+.42f));
    u.camera={guestShore+2.12f,1.82f,beachChairZ-1.9f,1.57f};
    u.view={.91f,.22f,0.f,0.f};
    const auto sitting=render("beach-chair.ppm");
    u.beachVisitor[3]=0;
    require(difference(sitting,render(nullptr))>40,"Beach visitor and chair did not render");
    chair.update(silence,21.,.02,u);
    require(u.beachVisitor[3]==0,"Beach visitor stayed after her visit");
    chair.update(silence,66.,.02,u);
    require(u.beachVisitor[3]==1,"Beach visitor did not return after a minute");
    result["beach_water"]={{"rain_affected_pixels",rainPixels},{"spray_affected_pixels",rockPixels},
        {"caustic_affected_pixels",causticPixels},{"underwater_caustic_affected_pixels",reefLight},
        {"turtle_follow",true},{"beach_chair",true}};
}
void underSeaMotion(const fs::path& assets,json& result){
    auto options=defaults(assets,"undersea");
    options.apply("[reef]\nseed=32\n[kelp]\nbreak_interval=125\n");
    terrain::UnderSeaMotion motion(options);
    auto audio=signal();
    terrain::SceneUniforms u;
    motion.update(audio,0,.1,u);
    require(u.reef[0]==32&&near(u.kelp[0][0],1)&&u.kelp.back()[3]<1&&u.marine[0]==0,
        "UnderSea starting reef, kelp or event is incorrect");
    motion.update(audio,124,.1,u);
    require(u.marine[0]==0,"UnderSea started the first timed event early");
    motion.update(audio,125,.1,u);
    require(u.marine[0]==1&&u.detachedKelp[1]==0&&u.kelp[0][0]==0,"UnderSea kelp did not detach and replant");
    motion.update(audio,185,.1,u);
    require(near(u.kelp[0][0],.5f)&&u.detachedKelp[1]<0,"UnderSea kelp did not regrow");
    terrain::UnderSeaMotion preview(options);
    preview.update(audio,0,.1,u);
    for(unsigned n=0;n<5;++n){
        preview.nextMode();preview.update(audio,n+1.,.1,u);
        require(u.marine[0]==float(n%4+1)&&u.marine[1]==0,
            "UnderSea Space did not cycle one event at a time");
    }
    preview.nextCamera();preview.update(audio,7.,.1,u);
    preview.update(audio,7.8,.1,u);
    require(u.view[0]>.99f&&std::abs(u.camera[0])>1.f,"UnderSea cannot switch to the gliding camera");
    const float previous=u.marine[2];
    audio.beatCount=1;audio.beat=true;
    preview.update(audio,7.9,.1,u);
    require(u.marine[2]-previous>.13f,"UnderSea fish did not accelerate on the beat");
    options.apply("[kelp]\nenabled=false\n[whale]\nenabled=false\n");
    terrain::UnderSeaMotion disabled(options);
    disabled.nextMode();disabled.update(audio,0,.1,u);
    require(u.marine[0]==2.f&&u.reef[1]==0,"UnderSea preview did not skip disabled events");
    disabled.nextMode();disabled.update(audio,1,.1,u);
    require(u.marine[0]==4.f,"UnderSea preview did not skip the disabled whale");
    result["undersea"]={{"events",4},{"seconds_to_regrow",120},{"cameras",2},{"beat_fish_speed",true}};
}
void reefCurrent(const fs::path& assets,json& result){
    auto options=defaults(assets,"coralcanyon");
    options.apply("[reef]\nseed=26\n[camera]\nseconds=0\n[current]\nreversal_seconds=16\n");
    terrain::UnderSeaMotion canyon(options,true);
    terrain::SceneUniforms u{};
    MusicFrame silence{},audio=signal();
    canyon.update(silence,0.,.02,u);
    const float start=u.camera[2];
    require(u.view[2]==1&&u.reefTerrain[3]==8&&u.reef[3]>=16,"CoralCanyon did not load its reef profile");
    canyon.update(silence,8.,.02,u);
    require(std::sin(u.reefCurrent[0])>.99f&&u.camera[2]>start+5.f,"CoralCanyon failed to glide with the incoming current");
    const float quiet=u.reefCurrent[2];
    audio.beatCount=1;audio.beat=true;
    const float clock=u.marine[2];
    canyon.update(audio,8.02,.02,u);
    require(u.reefCurrent[2]-quiet<.007f&&u.marine[2]-clock<.02f,"A beat abruptly changed the canyon current or fish");
    canyon.nextMode();
    canyon.update(silence,23.,.02,u);
    canyon.update(silence,24.,.02,u);
    require(std::sin(u.reefCurrent[0])<-.99f&&u.view[0]>.99f&&u.marine[0]==0,
            "CoralCanyon failed to reverse the current or switch the glide on Space");
    result["coralcanyon"]={{"current_reversal_seconds",16},{"forward_glide",true},{"cameras",2}};
}
void objectsMotion(const fs::path& assets,json& result){
    auto options=defaults(assets,"objects");
    terrain::ObjectsMotion motion(options);
    auto audio=signal();
    terrain::SceneUniforms u;
    for(unsigned n=0;n<5;++n){
        motion.update(audio,18.*n,.1,u);
        require(u.geometry[0]==float(n),"Objects cycle omitted a shape");
    }
    audio.beat=true;audio.beatCount=1;
    motion.update(audio,72.1,.1,u);
    require(u.geometry[1]>1.2f&&std::abs(u.effects[0])+std::abs(u.effects[1])>.02f,"Object beat scale or note articulation is missing");
    const auto turn=u.geometry[2];
    motion.update(audio,72.3,.2,u);
    require(u.geometry[2]!=turn,"Objects stopped responding to the waveform");
    motion.update(audio,72.61,.31,u);
    require(near(u.geometry[1],1),"Objects did not return to normal size");
    terrain::ObjectsMotion launch(options,3);
    launch.update(audio,0,.1,u);require(u.geometry[0]==3,"Objects did not use its startup slot");
    launch.nextMode();launch.update(audio,.1,.1,u);require(u.geometry[0]==4,"Objects did not switch on Space");
    options.apply("[armillary]\nenabled=false\n[beat]\nenabled=false\n[notes]\nenabled=false\n"
                  "[rotation]\nidle_speed=0\nwaveform_enabled=false\n[camera]\norbit_enabled=false\n");
    terrain::ObjectsMotion disabled(options);
    disabled.update(audio,0,.1,u);
    require(u.geometry[0]==1&&near(u.geometry[1],1)&&near(u.geometry[2],0)&&near(u.effects[0],0)
            &&near(u.effects[1],0)&&near(u.camera[2],.4f),"Objects disabled controls were ignored");
    result["objects"]={{"shapes",5},{"beat_scale",true},{"wave_rotation",true},{"note_twist",true}};
}
void renderPlugin(Context& context,const fs::path& module,const fs::path& assets,const fs::path& output,
                  const std::string& identifier,json& result){
    const bool reef=identifier=="undersea"||identifier=="coralcanyon";
    const std::string override=identifier=="objects"?"[objects]\nseconds_per_shape=1\n":
        identifier=="beach"?"[camera]\nseconds=2\ntransition_fraction=0\n":
        reef?"[reef]\nseed=32\n[camera]\nseconds=0\n":"";
    const auto preferences=json{{"sceneIni",override}}.dump();
    PluginLibrary library(module,identifier);
    const unsigned width=reef?640:360,height=reef?400:224;
    auto view=library.create(context,width,height,fs::absolute(sceneAssets(assets,identifier)),preferences);
    auto audio=signal();
    std::vector<uint16_t> previous;
    const unsigned frames=identifier=="objects"?5:identifier=="beach"?unsigned(terrain::BeachCameras.size()):
        reef?6:3;
    const std::array<double,6> seaTimes{0.,1.,4.,12.,26.,45.};
    const std::array<double,6> seaPreviews{0.,0.,1.,6.,14.,29.};
    for(unsigned n=0;n<frames;++n){
        const double time=identifier=="objects"?n*1.01:identifier=="beach"?n*2.:
            identifier=="coralcanyon"?n*9.0:reef?seaTimes[n]:n*.31;
        audio.beatCount=n?1:0;audio.beat=n==1;
        if(identifier=="undersea"){
            if(n==1){view->key('c');view->render(audio,0,.08);}
            if(n>=2){view->key(' ');view->render(audio,seaPreviews[n],.08);}
        }
        if(identifier=="coralcanyon"&&n==2){view->key(' ');view->render(audio,time-1.,.08);}
        view->render(audio,time,.08);
        auto pixels=context.readRgba16(view->image(),width,height);
        require(pixels!=previous,"Different scene frames produced identical output");
        std::set<uint16_t> values;
        for(size_t i=0;i<pixels.size();i+=4){
            require(pixels[i+3]==0x3c00,"Scene output alpha is invalid");
            values.insert(pixels[i]);
        }
        require(values.size()>64,"Scene shader returned a flat image");
        dumpImage(output/(identifier+"-"+std::to_string(n)+".ppm"),pixels,width,height);
        previous=std::move(pixels);
    }
    if(identifier=="beach"){
        view->render(audio,0,0);
        view->render(audio,.9,0);
        const auto withoutPreview=context.readRgba16(view->image(),width,height);
        view->render(audio,0,0);
        view->key('d');
        view->render(audio,0,0);
        view->render(audio,.9,0);
        const auto preview=context.readRgba16(view->image(),width,height);
        unsigned changed=0;
        for(size_t i=0;i<preview.size();i+=4)
            if(std::abs(decodeHalf(preview[i])-decodeHalf(withoutPreview[i]))>.06f)++changed;
        require(changed>width*height/5,"D did not trigger the Beach dolphin preview");
        dumpImage(output/"beach-dolphin-preview.ppm",preview,width,height);
        result["beach_dolphin_preview"]=true;
    }
    const auto badgeAt=[&](double time,float r,float g,float b){
        view->render(audio,time,.02);
        const auto pixels=context.readRgba16(view->image(),width,height);
        unsigned matches=0;
        for(unsigned y=height-33;y<height;++y)for(unsigned x=width-33;x<width;++x){
            const auto index=(y*width+x)*4;
            if(std::abs(decodeHalf(pixels[index])-r)<.004f&&std::abs(decodeHalf(pixels[index+1])-g)<.004f&&
               std::abs(decodeHalf(pixels[index+2])-b)<.004f)++matches;
        }
        return matches;
    };
    view->key('l');
    require(badgeAt(20,.85f,.98f,1.f)>4,"Locked badge did not render at the bottom right");
    view->key('l');
    require(badgeAt(21,1.f,.85f,.53f)>4,"Unlocked badge did not render at the bottom right");
    require(badgeAt(23.1,1.f,.85f,.53f)==0,"Unlocked badge remained after its fade");
    view->resize(192,112);
    view->render(audio,frames+1,.1);
    require(view->image().GetWidth()==192&&view->image().GetHeight()==112,"Scene resize did not replace its output");
    result[identifier+"_gpu_frames"]=frames;
}
}

int main(int argc,char** argv){
    try{
        const bool reef=argc==6&&std::string_view(argv[3])=="--reef";
        if(argc!=6)throw std::runtime_error("Expected assets, output and scene modules");
        const fs::path assets=argv[1],output=argv[2];
        fs::create_directories(output);
        Context context;
        json result{{"adapter",context.info()}};
        if(reef){
            underSeaMotion(assets,result);
            reefCurrent(assets,result);
            renderPlugin(context,argv[4],assets,output,"undersea",result);
            renderPlugin(context,argv[5],assets,output,"coralcanyon",result);
        }else{
            modeLock();
            terrainTiming(context,assets,result);
            beachMotion(assets,result);
            beachWater(context,assets,output,result);
            objectsMotion(assets,result);
            const std::array<std::string,3> identifiers={"terrain","beach","objects"};
            for(unsigned n=0;n<identifiers.size();++n)
                renderPlugin(context,argv[n+3],assets,output,identifiers[n],result);
        }
        result["result"]="pass";
        std::ofstream(output/"result.json")<<result.dump(2)<<'\n';
        std::cout<<result.dump(2)<<'\n';
        return 0;
    }catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}
}
