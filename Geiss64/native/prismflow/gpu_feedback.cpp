// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_feedback.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace prismflow {
namespace {
double draw(const RandomFunction& random){
    if(!random)throw std::invalid_argument("Prism Flow noise requires a random source");const double value=random();
    if(!std::isfinite(value)||value<0||value>=1)throw std::invalid_argument("Prism Flow noise random value must be in [0,1)");return value;
}
float scalar(double value){if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())throw std::invalid_argument("Nonfinite or excessive feedback uniform");return float(value);}
constexpr auto feedbackUsage=wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc;
void dimensions(GpuContext& context,uint32_t width,uint32_t height){
    wgpu::Limits limits{};if(context.device.GetLimits(&limits)!=wgpu::Status::Success)throw std::runtime_error("Cannot query feedback texture limits");
    if(!width||!height||width>limits.maxTextureDimension2D||height>limits.maxTextureDimension2D||uint64_t(width)*height>16777216)throw std::invalid_argument("Invalid or excessive feedback dimensions");
}
}
void GpuFeedback::requireIdle()const{if(phase_!=Phase::Idle)throw std::logic_error("Prism Flow feedback frame is still open");}
GpuFeedback::GpuFeedback(GpuContext& context,GpuAssets& assets,GpuDynamicMaps& dynamic,uint32_t width,uint32_t height,const RandomFunction& random):context_(context),assets_(assets),dynamicMaps_(dynamic){
    dimensions(context_,width,height);slots_.fill(-1);warpPipeline_=assets_.render("warp",wgpu::TextureFormat::RGBA16Float);exposurePipeline_=assets_.compute("auto_exposure");
    for(auto& entry:cache_){entry.texture=gpuTexture(context_,kWarpMapSize,kWarpMapSize,wgpu::TextureFormat::RG16Float,feedbackUsage);entry.view=entry.texture.CreateView();}
    slotViews_.fill(cache_[0].view);
    wgpu::SamplerDescriptor sampler{};sampler.minFilter=sampler.magFilter=wgpu::FilterMode::Linear;sampler.mipmapFilter=wgpu::MipmapFilterMode::Nearest;
    warpSampler_=context_.device.CreateSampler(&sampler);sampler.addressModeU=sampler.addressModeV=wgpu::AddressMode::Repeat;indexSampler_=context_.device.CreateSampler(&sampler);
    uniforms_=gpuBuffer(context_,256,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    exposure_=gpuBuffer(context_,16,wgpu::BufferUsage::Storage|wgpu::BufferUsage::CopySrc);
    std::vector<uint8_t> bytes(size_t(kNoiseTexSize)*kNoiseTexSize*4);
    for(size_t n=0;n<noise_.size();++n){noise_[n]=gpuTexture(context_,kNoiseTexSize,kNoiseTexSize,wgpu::TextureFormat::RGBA8Unorm,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc);noiseViews_[n]=noise_[n].CreateView();
        for(auto& value:bytes){const double a=draw(random),b=draw(random),c=draw(random);value=uint8_t(((a+b+c)*(1./3))*255+.5);}
        wgpu::TexelCopyTextureInfo destination{};destination.texture=noise_[n];wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=kNoiseTexSize*4;layout.rowsPerImage=kNoiseTexSize;
        const wgpu::Extent3D extent{kNoiseTexSize,kNoiseTexSize,1};context_.queue.WriteTexture(&destination,bytes.data(),bytes.size(),&layout,&extent);
    }
    replaceFeedback(width,height);context_.checkErrors();
}
void GpuFeedback::replaceFeedback(uint32_t width,uint32_t height){
    dimensions(context_,width,height);
    std::array<wgpu::Texture,2> textures;std::array<wgpu::TextureView,2> views;
    for(size_t i=0;i<textures.size();++i){textures[i]=gpuTexture(context_,width,height,wgpu::TextureFormat::RGBA16Float,feedbackUsage);views[i]=textures[i].CreateView();}
    // Groups release their old views before the old textures leave scope.
    warpGroups_={};exposureGroups_={};feedback_=std::move(textures);feedbackViews_=std::move(views);width_=width;height_=height;ping_=0;bindingsDirty_=true;
}
void GpuFeedback::rebuildBindings(){
    for(unsigned old=0;old<2;++old){const std::array exposureEntries={bindTexture(0,feedbackViews_[old]),bindBuffer(1,uniforms_),bindBuffer(2,exposure_)};
        exposureGroups_[old]=gpuBindings(context_,exposurePipeline_.GetBindGroupLayout(0),exposureEntries);
        for(unsigned noise=0;noise<4;++noise){std::array<wgpu::BindGroupEntry,14> entries;entries[0]=bindTexture(0,feedbackViews_[old]);
            for(unsigned slot=0;slot<kGpuWarpSlots;++slot)entries[slot+1]=bindTexture(slot+1,slotViews_[slot]);
            entries[9]=bindSampler(9,warpSampler_);entries[10]=bindBuffer(10,uniforms_);entries[11]=bindTexture(11,noiseViews_[noise]);entries[12]=bindBuffer(12,exposure_);entries[13]=bindSampler(13,indexSampler_);
            warpGroups_[old][noise]=gpuBindings(context_,warpPipeline_.GetBindGroupLayout(0),entries);}}
    bindingsDirty_=false;context_.checkErrors();
}
void GpuFeedback::bindMap(size_t slot,const MotionMap& map){
    requireIdle();if(slot>=kGpuWarpSlots||!map.identity||map.width!=kWarpMapSize||map.height!=kWarpMapSize)throw std::invalid_argument("Invalid Prism Flow warp-map binding");
    const bool dynamic=map.isDynamic();if(dynamic?!map.displacement.empty()||map.dynamic.size()!=kDynamicMotionParamCount:map.displacement.size()!=size_t(kWarpMapSize)*kWarpMapSize*2)throw std::invalid_argument("Cannot bind an incomplete or pruned warp map");
    size_t index=cache_.size();for(size_t i=0;i<cache_.size();++i)if(cache_[i].identity==map.identity){index=i;break;}
    if(index==cache_.size()){
        for(size_t i=0;i<cache_.size();++i)if(!cache_[i].identity){index=i;break;}
        if(index==cache_.size()){uint64_t oldest=std::numeric_limits<uint64_t>::max();for(size_t i=0;i<cache_.size();++i){
            if(std::find(slots_.begin(),slots_.end(),int32_t(i))!=slots_.end())continue;if(cache_[i].lastUsed<oldest){oldest=cache_[i].lastUsed;index=i;}}}
        if(index==cache_.size())throw std::runtime_error("Prism Flow warp cache has no evictable entry");
        // Validate/build before replacing a live cache descriptor. Queue uploads
        // happen between submitted frames, never inside an unfinished frame.
        std::optional<DynamicMapState> state;
        if(dynamic)state=dynamicMaps_.create(map.mode,map.dynamic);
        else{
            std::vector<uint16_t> values(map.displacement.size());for(size_t i=0;i<values.size();++i){const float value=map.displacement[i];
                if(!std::isfinite(value)||std::abs(value)>65504)throw std::invalid_argument("Static warp pixels must be finite half-float values");values[i]=visualizers::gpu::encodeHalf(value);}
            uploadHalfTexture(context_,cache_[index].texture,values);
        }
        auto& entry=cache_[index];entry.dynamic=std::move(state);entry.identity=map.identity;entry.mode=map.mode;entry.seed=map.seed;
    }else if(cache_[index].mode!=map.mode||cache_[index].seed!=map.seed||bool(cache_[index].dynamic)!=dynamic)throw std::invalid_argument("A cached motion map was changed without a new identity");
    if(cacheClock_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Prism Flow warp cache clock exhausted");cache_[index].lastUsed=++cacheClock_;
    slots_[slot]=int32_t(index);if(slotViews_[slot].Get()!=cache_[index].view.Get()){slotViews_[slot]=cache_[index].view;bindingsDirty_=true;}
}
void GpuFeedback::uploadIndices(std::span<const uint16_t> rgba){requireIdle();if(rgba.size()!=size_t(width_)*height_*4)throw std::invalid_argument("Prism Flow feedback upload has the wrong size");
    for(auto value:rgba)if((value&0x7c00)==0x7c00)throw std::invalid_argument("Prism Flow feedback upload must be finite");
    for(const auto& texture:feedback_)uploadHalfTexture(context_,texture,rgba);
}
void GpuFeedback::uploadIndices(std::span<const uint8_t> monochrome){requireIdle();if(monochrome.size()!=size_t(width_)*height_)throw std::invalid_argument("Prism Flow index upload has the wrong size");
    std::vector<uint16_t> values(monochrome.size()*4);for(size_t n=0;n<monochrome.size();++n){const auto value=visualizers::gpu::encodeHalf(float(monochrome[n]/255.));std::fill_n(values.begin()+n*4,4,value);}uploadIndices(values);
}
void GpuFeedback::fillIndices(const RandomFunction& random){requireIdle();std::vector<uint8_t> values(size_t(width_)*height_);for(auto& value:values)value=uint8_t(draw(random)*40);uploadIndices(values);}
void GpuFeedback::resize(uint32_t width,uint32_t height,const RandomFunction& random){requireIdle();if(width==width_&&height==height_)return;replaceFeedback(width,height);fillIndices(random);}
std::array<float,36> GpuFeedback::uniformData(uint32_t width,uint32_t height,const FeedbackFrame& frame){
    if(!width||!height||uint64_t(width)*height>16777216||frame.oversample<=0||frame.noiseX>=256||frame.noiseY>=256)throw std::invalid_argument("Invalid Prism Flow feedback dimensions/scale/noise offset");
    const double aspect=double(width)/height,x=aspect>1?2:2*aspect,y=aspect>1?2/aspect:2;
    std::array<float,36> values{};for(unsigned i=0;i<8;++i)values[i]=scalar(frame.strengths[i]);
    values[8]=scalar(frame.shiftX);values[9]=scalar(frame.shiftY);values[10]=values[11]=kWarpMapSize;
    values[12]=float(width);values[13]=float(height);values[14]=float(1./width);values[15]=float(1./height);
    values[16]=float(x);values[17]=float(y);values[18]=float(1./x);values[19]=float(1./y);values[20]=float(frame.noiseX);values[21]=float(frame.noiseY);values[22]=scalar(frame.darkening);values[23]=scalar(frame.time);
    const auto& p=frame.shader;values[24]=scalar(p.rx_diff_grey_scott_str);values[25]=scalar(p.rx_diff_grey_scott_str2);values[26]=scalar(frame.diffusionColorization);values[27]=scalar(p.rx_diff_barkley_str);
    values[28]=scalar(p.frequency_injector_str);values[29]=scalar(p.frequency_injector_freq_scale);values[30]=scalar(p.painterly_str);values[31]=scalar(p.sharpen_str);
    values[32]=scalar(p.noise_str);values[33]=scalar(frame.oversample);values[34]=scalar(1./frame.oversample);values[35]=scalar(p.layer2_motion_scale);return values;
}
const wgpu::CommandEncoder& GpuFeedback::beginFrame(const FeedbackFrame& frame){
    requireIdle();const auto data=uniformData(width_,height_,frame);for(unsigned slot=0;slot<8;++slot)if(frame.strengths[slot]!=0&&slots_[slot]<0)throw std::invalid_argument("A nonzero warp strength has no bound motion map");
    if(bindingsDirty_)rebuildBindings();context_.queue.WriteBuffer(uniforms_,0,data.data(),sizeof(data));auto encoder=context_.device.CreateCommandEncoder();
    std::array<DynamicMapDraw,kGpuWarpSlots> draws;size_t count=0;
    for(auto index:slots_){if(index<0)continue;const auto& entry=cache_[index];if(entry.dynamic)draws[count++]={&*entry.dynamic,entry.view};}
    dynamicMaps_.encode(encoder,std::span(draws).first(count),frame.time);priorPing_=ping_;encoder_=std::move(encoder);phase_=Phase::Prepared;return encoder_;
}
void GpuFeedback::encodeWarp(){
    if(phase_!=Phase::Prepared)throw std::logic_error("Prism Flow warp needs one prepared frame");
    auto compute=encoder_.BeginComputePass();compute.SetPipeline(exposurePipeline_);compute.SetBindGroup(0,exposureGroups_[ping_]);compute.DispatchWorkgroups(1);compute.End();
    const auto next=ping_^1;wgpu::RenderPassColorAttachment color{};color.view=feedbackViews_[next];color.loadOp=wgpu::LoadOp::Load;color.storeOp=wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;auto pass=encoder_.BeginRenderPass(&descriptor);pass.SetPipeline(warpPipeline_);pass.SetBindGroup(0,warpGroups_[ping_][frame_&3]);pass.Draw(3);pass.End();
    ping_=next;phase_=Phase::Warped;
}
void GpuFeedback::submit(){
    if(phase_!=Phase::Warped)throw std::logic_error("Prism Flow submit needs one warped frame");const auto commands=encoder_.Finish();context_.queue.Submit(1,&commands);encoder_=nullptr;phase_=Phase::Idle;++frame_;context_.checkErrors();
}
void GpuFeedback::abortFrame()noexcept{if(phase_!=Phase::Idle)ping_=priorPing_;encoder_=nullptr;phase_=Phase::Idle;}
size_t GpuFeedback::populatedMaps()const{return size_t(std::count_if(cache_.begin(),cache_.end(),[](const auto& entry){return entry.identity!=0;}));}
size_t GpuFeedback::dynamicInstances()const{return size_t(std::count_if(cache_.begin(),cache_.end(),[](const auto& entry){return entry.dynamic.has_value();}));}
const wgpu::Texture& GpuFeedback::slotTexture(size_t slot)const{if(slot>=slots_.size()||slots_[slot]<0)throw std::out_of_range("Prism Flow warp slot is not bound");return cache_[slots_[slot]].texture;}
}
