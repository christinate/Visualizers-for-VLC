// SPDX-License-Identifier: MIT
#include "scene_dawn.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace terrain {
using namespace visualizers::gpu;
namespace {
using Vec=std::array<double,3>;
struct Camera {Vec eye,look;};
Camera camera(unsigned mode,double z){
    const double center=std::sin(z*.055)*3,front=std::sin((z+24)*.055)*3;
    switch(mode){
        case 0:return {{center+4,29,z},{front,2,z+25}};
        case 1:return {{center-5,-18,z},{front,-7,z+24}};
        case 2:return {{center+17,26,z},{front+1,2,z+23}};
        case 3:return {{center,2+std::sin(z*.04)*.5,z},{front,2+std::sin((z+24)*.04)*.5,z+24}};
        case 4:return {{center-13,23,z},{front+8,6,z+27}};
        case 5:return {{center+std::sin(z*.018)*9,40,z},{front,-2,z+11}};
        case 6:return {{center+3,15,z},{front+3,1,z+23}};
        default:return {{front-17,26,z+24},{center,2,z+6}};
    }
}
Vec mix(const Vec& a,const Vec& b,double amount){Vec value{};for(unsigned n=0;n<3;++n)value[n]=a[n]+(b[n]-a[n])*amount;return value;}
}
TerrainMotion TerrainMotion::fromIni(const SceneIni& ini) {
    TerrainMotion value;
    for (size_t index=0;index<CameraNames.size();++index) {
        const auto prefix = std::string(CameraNames[index]) + ".";
        value.cameras[index] = ini.enabled(prefix+"enabled",true);
        value.heightViews[index] = ini.enabled(prefix+"height_effect",value.heightViews[index]);
        value.widthViews[index] = ini.enabled(prefix+"width_effect",value.widthViews[index]);
    }
    if (std::none_of(value.cameras.begin(),value.cameras.end(),[](bool enabled){return enabled;}))
        throw std::runtime_error("Enable at least one Terrain camera in terrain.ini");
    value.heightEnabled = ini.enabled("height.enabled",true);
    value.widthEnabled = ini.enabled("width.enabled",true);
    value.heightPeak = ini.number("height.peak_percent",200.f,100.f,450.f)/100;
    value.widthPeak = ini.number("width.peak_percent",200.f,100.f,400.f)/100;
    value.heightSeconds = ini.number("height.return_seconds",.5f,.01f,60.f);
    value.widthSeconds = ini.number("width.return_seconds",.5f,.01f,60.f);
    value.cameraSeconds = ini.number("camera.seconds",0.f,0.f,3600.f);
    value.transition = ini.number("camera.transition_fraction",.15,0.,.8);
    value.speed = ini.number("camera.speed",3.f,0.f,40.f);
    value.clearance = ini.number("camera.clearance",3.f,.5f,12.f);
    value.colorBeats = ini.number("palette.beats_per_change",0u,0u,128u);
    return value;
}
DawnScene::DawnScene(Context& context,const std::string& source,unsigned width,unsigned height)
    :context_(context),width_(width),height_(height){
    auto shader=context.shader(source);pipeline_=pipeline(context,shader,"vertex","fragment");
    target_=context.target(width,height);
    targetView_=target_.CreateView();
    wgpu::BufferDescriptor uniform{};uniform.size=sizeof(data_);uniform.usage=wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst;
    uniform_=context.device.CreateBuffer(&uniform);
    wgpu::TextureDescriptor history{};history.dimension=wgpu::TextureDimension::e2D;
    history.size={256,128,1};history.format=wgpu::TextureFormat::RGBA16Float;
    history.usage=wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst;
    history_=context.device.CreateTexture(&history);
    wgpu::SamplerDescriptor sampler{};sampler.magFilter=wgpu::FilterMode::Linear;sampler.minFilter=wgpu::FilterMode::Linear;
    sampler.mipmapFilter=wgpu::MipmapFilterMode::Nearest;sampler.addressModeU=wgpu::AddressMode::ClampToEdge;
    sampler.addressModeV=wgpu::AddressMode::Repeat;sampler.addressModeW=wgpu::AddressMode::ClampToEdge;
    std::array<wgpu::BindGroupEntry,3> entries{};
    entries[0].binding=0;entries[0].buffer=uniform_;entries[0].size=sizeof(data_);
    entries[1].binding=1;entries[1].textureView=history_.CreateView();
    entries[2].binding=2;entries[2].sampler=context.device.CreateSampler(&sampler);
    wgpu::BindGroupDescriptor group{};group.layout=pipeline_.GetBindGroupLayout(0);group.entryCount=entries.size();group.entries=entries.data();
    bindings_=context.device.CreateBindGroup(&group);context.checkErrors();
}
void DawnScene::setHistory(std::span<const uint16_t> data,float head){
    if(data.size()!=256*128*4||!std::isfinite(head))throw std::runtime_error("Invalid Terrain history");
    wgpu::TexelCopyTextureInfo destination{};destination.texture=history_;destination.aspect=wgpu::TextureAspect::All;
    wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=2048;layout.rowsPerImage=128;
    wgpu::Extent3D size{256,128,1};context_.queue.WriteTexture(&destination,data.data(),data.size_bytes(),&layout,&size);
    head_=head-std::floor(head);
}
void DawnScene::resize(unsigned width,unsigned height){
    if(width==width_&&height==height_)return;
    auto target=context_.target(width,height);context_.checkErrors();target_=std::move(target);targetView_=target_.CreateView();width_=width;height_=height;
}
void DawnScene::setOutput(bool linear,float peakNits){
    if(!std::isfinite(peakNits)||peakNits<80||peakNits>10000)throw std::runtime_error("Invalid Terrain peak luminance");
    linearOutput_=linear;peakNits_=peakNits;
}
unsigned DawnScene::update(const visualizers::MusicFrame& audio,double time,float cameraSeconds,unsigned colorBeats){
    if(!std::isfinite(time)||time<0||!colorBeats||!std::isfinite(cameraSeconds)||cameraSeconds<30||cameraSeconds>180||
       !std::isfinite(audio.rms)||audio.rms<0||!std::isfinite(audio.beatEnvelope))throw std::runtime_error("Invalid Terrain update parameters");
    if(motion_.colorBeats)colorBeats=motion_.colorBeats;
    beat_.update(time,audio.beatCount,audio.beat);
    const double tick=std::floor(time*30);
    if(tick!=lastHistory_){
        lastHistory_=tick;
        for(unsigned n=0;n<256;++n){
            rowData_[n*4]=encodeHalf(audio.wave[n*8]);
            rowData_[n*4+1]=encodeHalf(float(std::min(2.,audio.beatEnvelope*.9+audio.rms)));
            rowData_[n*4+2]=encodeHalf(std::abs(audio.wave[(n*8+256)%2048]));
            rowData_[n*4+3]=encodeHalf(float(audio.rms));
        }
        wgpu::TexelCopyTextureInfo destination{};destination.texture=history_;destination.origin={0,row_,0};
        wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=2048;layout.rowsPerImage=1;wgpu::Extent3D size{256,1,1};
        context_.queue.WriteTexture(&destination,rowData_.data(),sizeof(rowData_),&layout,&size);
        head_=(float(row_)+.5f)/128;row_=(row_+1)%128;
    }
    if(audio.beatCount<lastBeat_)lastBeat_=audio.beatCount;
    if(audio.beatCount-lastBeat_>=colorBeats){hue_=std::fmod(hue_+.137,1.);lastBeat_=audio.beatCount;}
    return draw(time,float(audio.beatEnvelope),float(audio.rms),audio.beatCount,cameraSeconds,colorBeats,float(hue_));
}
unsigned DawnScene::draw(double time,float beatEnvelope,float rms,uint64_t beatCount,float cameraSeconds,unsigned colorBeats,float hue){
    if(!std::isfinite(time)||time<0||!std::isfinite(beatEnvelope)||!std::isfinite(rms)||!std::isfinite(cameraSeconds)||cameraSeconds<30||cameraSeconds>180||!colorBeats)
        throw std::runtime_error("Invalid Terrain frame parameters");
    std::array<unsigned,8> enabled{};
    unsigned count=0;
    for(unsigned index=0;index<motion_.cameras.size();++index)
        if(motion_.cameras[index])enabled[count++]=index;
    const auto framing=cameras_.frame(time,motion_.cameraSeconds>0?motion_.cameraSeconds:cameraSeconds,motion_.transition,count);
    const unsigned current=enabled[framing.from],next=enabled[framing.to];
    const double blend=framing.blend;
    const auto weight=[&](const std::array<bool,8>& views){
        return float((views[current]?1.-blend:0.)+(views[next]?blend:0.));
    };
    data_[7]=motion_.heightEnabled?(motion_.heightPeak-1)*beat_.remaining(time,motion_.heightSeconds)*weight(motion_.heightViews):0;
    data_[11]=motion_.widthEnabled?(motion_.widthPeak-1)*beat_.remaining(time,motion_.widthSeconds)*weight(motion_.widthViews):0;
    const auto level=[](unsigned mode){return mode==1?-1:mode==3?0:1;};
    const auto protectedCamera=[&](unsigned mode){
        auto c=camera(mode,time*motion_.speed);
        if(level(mode)>0){
            const double axis=std::sin(c.eye[2]*.055)*3;
            const double ridge=3*std::pow((std::abs(c.eye[0]-axis)+1.)/5.,1.35);
            const double peak=motion_.heightEnabled&&motion_.heightViews[mode]?motion_.heightPeak:1.;
            c.eye[1]=std::max(c.eye[1],(ridge+13.5)*peak-2.5+motion_.clearance);
        }else if(level(mode)<0)c.eye[1]=std::min(c.eye[1],-10.-motion_.clearance);
        else{
            c.eye[1]+=(c.eye[1]+2.5)*data_[7];
            c.look[1]+=(c.look[1]+2.5)*data_[7];
        }
        return c;
    };
    const auto from=protectedCamera(current),to=protectedCamera(next);
    // The tunnel, underside and air above the ridge are disconnected.
    // Dissolve between those views rather than taking a path through rock.
    const bool dissolve=level(current)!=level(next)&&blend>0&&blend<1;
    const auto eye=dissolve?from.eye:mix(from.eye,to.eye,blend);
    const auto look=dissolve?from.look:mix(from.look,to.look,blend);
    data_[0]=float(width_);data_[1]=float(height_);data_[2]=float(time);data_[3]=head_;
    for(unsigned n=0;n<3;++n){data_[4+n]=float(eye[n]);data_[8+n]=float(look[n]);}
    for(unsigned n=0;n<3;++n){data_[20+n]=float(to.eye[n]);data_[24+n]=float(to.look[n]);}
    data_[23]=dissolve?float(blend):0;
    data_[27]=cameras_.badge(time);
    data_[12]=beatEnvelope;data_[13]=rms;data_[14]=float(beatCount);
    data_[16]=hue<0?std::fmod(float(beatCount/colorBeats)*.137f,1.f):hue;
    data_[17]=linearOutput_?1.f:0.f;data_[18]=peakNits_/203.f; // Display HDR capability/presentation remains a separate host task.
    context_.queue.WriteBuffer(uniform_,0,data_.data(),sizeof(data_));
    render(context_,pipeline_,targetView_,bindings_);return current;
}
}
