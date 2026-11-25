// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_bokeh.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
namespace prismflow {
namespace {
float scalar(double v){if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid bokeh scalar");return float(v);}
void dimensions(uint32_t w,uint32_t h){if(!w||!h||uint64_t(w)*h>16777216)throw std::invalid_argument("Invalid bokeh dimensions");}
int formatIndex(wgpu::TextureFormat format){switch(format){case wgpu::TextureFormat::RGBA16Float:return 0;case wgpu::TextureFormat::RGBA8Unorm:return 1;case wgpu::TextureFormat::BGRA8Unorm:return 2;default:throw std::invalid_argument("Unsupported bokeh output format");}}
std::array<double,4> bounds(uint32_t w,uint32_t h){const double aspect=double(w)/h,x=aspect>1?2:2*aspect,y=aspect>1?2/aspect:2;
    return {((-1.*(1./x)+.5)*w+.5)/w,((-1.*(1./y)+.5)*h+.5)/h,((1.*(1./x)+.5)*w+.5)/w,((1.*(1./y)+.5)*h+.5)/h};}
}
struct GpuBokeh::Readback {
    wgpu::Buffer buffer;wgpu::Future future{};uint64_t generation=0;bool complete=false,success=false;
    std::string error;
};
GpuBokeh::GpuBokeh(GpuContext& context,GpuAssets& assets):context_(context),assets_(assets),spawnData_(capacity*12,0){
    pending_.reserve(capacity);constexpr auto storage=wgpu::BufferUsage::Storage|wgpu::BufferUsage::CopySrc;
    particles_=gpuBuffer(context_,capacity*48,storage|wgpu::BufferUsage::CopyDst);spawns_=gpuBuffer(context_,capacity*48,storage|wgpu::BufferUsage::CopyDst);
    alive_=gpuBuffer(context_,4,storage|wgpu::BufferUsage::CopyDst);events_=gpuBuffer(context_,eventCount*8,storage);
    history_=gpuBuffer(context_,16+historySize*4,storage|wgpu::BufferUsage::CopyDst);snapshot_=gpuBuffer(context_,16,storage);
    cursor_=gpuBuffer(context_,4,storage|wgpu::BufferUsage::CopyDst);update_=gpuBuffer(context_,80,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::CopySrc);
    draw_=gpuBuffer(context_,32,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::CopySrc);palette_=gpuBuffer(context_,4096,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    updatePipeline_=assets_.compute("bokeh_advection");deathPipeline_=assets_.compute("bokeh_deaths");statsPipeline_=assets_.compute("bokeh_stats");
    const std::array deaths={bindBuffer(0,events_),bindBuffer(1,history_)};deathGroup_=gpuBindings(context_,deathPipeline_.GetBindGroupLayout(0),deaths);
    const std::array stats={bindBuffer(0,history_),bindBuffer(1,alive_),bindBuffer(2,snapshot_)};statsGroup_=gpuBindings(context_,statsPipeline_.GetBindGroupLayout(0),stats);
    readback_=std::make_shared<Readback>();readback_->buffer=gpuBuffer(context_,16,wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst);context_.checkErrors();
}
GpuBokeh::~GpuBokeh(){
    // The callback captures only heap state, never this. The host's tracked
    // future keeps module code resident through cancellation/device teardown.
    if(readback_){readback_->buffer.Destroy();if(readback_->future.id&&context_.instance.WaitAny(readback_->future,0)==wgpu::WaitStatus::Success)context_.forgetFuture(WGPUFuture{readback_->future.id});}
}
uint32_t GpuBokeh::add(std::span<const BokehSpawn> points,uint32_t width,uint32_t height,double birth){
    if(!width||!height||!std::isfinite(birth))return 0;dimensions(width,height);const double aspect=double(width)/height,xRange=aspect>1?2:2*aspect,yRange=aspect>1?2/aspect:2;
    uint32_t accepted=0;for(const auto& p:points){if(pending_.size()==capacity)break;const double z2=p.z2.value_or(p.z);
        const std::array values={p.x,p.y,p.z,z2,p.palettePosition,p.fadeIn,p.hold,p.fadeOut};
        if(!std::all_of(values.begin(),values.end(),[](double v){return std::isfinite(v);})||p.z<0||p.z>1||z2<0||z2>1||p.palettePosition<0||p.palettePosition>1||std::min({p.fadeIn,p.hold,p.fadeOut})<0||p.fadeIn+p.hold+p.fadeOut<=0)continue;
        // Keep JS double precision until expiry filtering and the actual upload.
        const double x=((p.x*(1./xRange)+.5)*width+.5)/width,y=((p.y*(1./yRange)+.5)*height+.5)/height;
        const std::array<double,12> packed{x,y,p.z,1,p.palettePosition,p.useBackground?1.:0.,birth,p.fadeIn,p.hold,p.fadeOut,p.z,z2};
        if(!std::all_of(packed.begin(),packed.end(),[](double v){return std::isfinite(v)&&std::abs(v)<=std::numeric_limits<float>::max();})||!std::isfinite(float(birth+p.fadeIn+p.hold+p.fadeOut)))continue;
        pending_.push_back(packed);++accepted;
    }return accepted;
}
void GpuBokeh::frame(const BokehFrame& f){dimensions(f.width,f.height);scalar(f.time);if(!std::isfinite(f.focus))throw std::invalid_argument("Invalid bokeh focal distance");scalar(f.opacity);frame_=f;frame_.focus=std::clamp(f.focus,0.,1.);}
void GpuBokeh::follow(std::span<const double> values){if(values.size()!=8)throw std::invalid_argument("Bokeh follow weights require eight values");std::array<float,8> next;for(unsigned n=0;n<8;++n)next[n]=scalar(values[n]);follow_=next;}
void GpuBokeh::palette(std::span<const float> values){if(values.size()!=1024)throw std::invalid_argument("Bokeh palette requires 256 RGBA entries");for(float v:values)if(!std::isfinite(v))throw std::invalid_argument("Bokeh palette must be finite");context_.queue.WriteBuffer(palette_,0,values.data(),values.size_bytes());}
void GpuBokeh::seedHistory(double lifetime){const float seed=scalar(lifetime);if(seed<=0||double(seed)*historySize>std::numeric_limits<float>::max())throw std::invalid_argument("Bokeh lifetime must be positive with a finite window sum");
    if(generation_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Bokeh history generation exhausted");std::array<uint32_t,4+historySize> data{};data[1]=historySize;std::fill(data.begin()+4,data.end(),std::bit_cast<uint32_t>(seed));
    context_.queue.WriteBuffer(history_,0,data.data(),sizeof(data));++generation_;stats_.samples=historySize;stats_.mean=seed;lastRequest_=-INFINITY;
}
void GpuBokeh::encode(const wgpu::CommandEncoder& encoder,const GpuFeedback& feedback){
    if(!encoder||feedback.phase()!=GpuFeedback::Phase::Prepared)throw std::logic_error("Bokeh update requires a prepared feedback frame");
    bool dirty=!updateGroup_||boundSampler_.Get()!=feedback.mapSampler().Get()||boundUniforms_.Get()!=feedback.uniformBuffer().Get();for(unsigned n=0;n<8;++n)dirty=dirty||boundViews_[n].Get()!=feedback.mapViews()[n].Get();
    if(dirty){std::array<wgpu::BindGroupEntry,17> e;e[0]=bindBuffer(0,particles_);e[1]=bindBuffer(2,update_);e[2]=bindBuffer(3,feedback.uniformBuffer());for(unsigned n=0;n<8;++n)e[n+3]=bindTexture(n+4,feedback.mapViews()[n]);
        e[11]=bindSampler(12,feedback.mapSampler());e[12]=bindBuffer(13,spawns_);e[13]=bindBuffer(14,cursor_);e[14]=bindBuffer(15,alive_);e[15]=bindBuffer(16,events_);e[16]=bindBuffer(17,history_);
        updateGroup_=gpuBindings(context_,updatePipeline_.GetBindGroupLayout(0),e);boundViews_=feedback.mapViews();boundSampler_=feedback.mapSampler();boundUniforms_=feedback.uniformBuffer();}
    uint32_t count=0;for(const auto& p:pending_){if(frame_.time>=p[6]+p[7]+p[8]+p[9])continue;for(unsigned n=0;n<12;++n)spawnData_[size_t(count)*12+n]=float(p[n]);++count;}
    if(count)context_.queue.WriteBuffer(spawns_,0,spawnData_.data(),size_t(count)*48);pending_.clear();const uint32_t zero=0;context_.queue.WriteBuffer(cursor_,0,&zero,4);
    std::array<uint32_t,20> data{};data[0]=std::bit_cast<uint32_t>(float(frame_.time));data[1]=std::bit_cast<uint32_t>(float(frame_.focus));data[2]=std::bit_cast<uint32_t>(float(frame_.width));data[3]=std::bit_cast<uint32_t>(float(frame_.height));data[4]=count;
    const auto b=bounds(feedback.width(),feedback.height());for(unsigned n=0;n<4;++n)data[n+8]=std::bit_cast<uint32_t>(float(b[n]));for(unsigned n=0;n<8;++n)data[n+12]=std::bit_cast<uint32_t>(follow_[n]);context_.queue.WriteBuffer(update_,0,data.data(),sizeof(data));
    encoder.ClearBuffer(alive_);auto pass=encoder.BeginComputePass();pass.SetPipeline(updatePipeline_);pass.SetBindGroup(0,updateGroup_);pass.DispatchWorkgroups(64);pass.End();
    auto deaths=encoder.BeginComputePass();deaths.SetPipeline(deathPipeline_);deaths.SetBindGroup(0,deathGroup_);deaths.DispatchWorkgroups(1);deaths.End();initialized_=true;
}
void GpuBokeh::prepareDraw(wgpu::TextureFormat format,const Color& background){
    const int index=formatIndex(format);std::array<float,8> data{float(frame_.width),float(frame_.height),float(frame_.focus),float(frame_.time),scalar(background[0]),scalar(background[1]),scalar(background[2]),float(frame_.opacity)};
    if(!drawPipelines_[index]){drawPipelines_[index]=assets_.render("bokeh_draw",format);const std::array e={bindBuffer(0,particles_),bindBuffer(1,draw_),bindBuffer(2,palette_)};drawGroups_[index]=gpuBindings(context_,drawPipelines_[index].GetBindGroupLayout(0),e);}
    context_.queue.WriteBuffer(draw_,0,data.data(),sizeof(data));drawIndex_=index;
}
void GpuBokeh::draw(const wgpu::RenderPassEncoder& pass)const{if(!initialized_)return;if(drawIndex_<0)throw std::logic_error("Bokeh draw needs prepared output controls");pass.SetPipeline(drawPipelines_[drawIndex_]);pass.SetBindGroup(0,drawGroups_[drawIndex_]);pass.Draw(4,capacity);}
bool GpuBokeh::statsPending()const{return readback_&&readback_->future.id;}
void GpuBokeh::pollStats(){
    const auto& state=readback_;if(!state->future.id)return;
    if(context_.instance.WaitAny(state->future,0)!=wgpu::WaitStatus::Success)return;
    context_.forgetFuture(WGPUFuture{state->future.id});state->future={};
    if(state->success){const auto* mapped=state->buffer.GetConstMappedRange(0,16);if(mapped){std::array<uint32_t,4> data;std::memcpy(data.data(),mapped,16);const float sum=std::bit_cast<float>(data[2]);
        if(data[0]<=capacity&&data[1]<=historySize&&std::isfinite(sum)&&sum>=0){stats_.error.clear();stats_.alive=data[0];if(state->generation==generation_){stats_.samples=data[1];stats_.mean=data[1]?std::optional<double>(double(sum)/data[1]):std::nullopt;}}
        else stats_.error="Invalid bokeh statistics snapshot";}else stats_.error="Bokeh statistics mapping returned no data";state->buffer.Unmap();
    }else if(stats_.error.empty())stats_.error=state->error.empty()?"Bokeh statistics readback failed":state->error;
}
BokehStats GpuBokeh::stats(double now){
    if(!std::isfinite(now))throw std::invalid_argument("Invalid bokeh monotonic clock");pollStats();if(!initialized_||statsPending()||now-lastRequest_<30)return stats_;
    if(statsRequests_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Bokeh statistics request counter exhausted");++statsRequests_;
    const auto state=readback_;state->complete=false;state->success=false;state->error.clear();state->generation=generation_;lastRequest_=now;
    const auto encoder=context_.device.CreateCommandEncoder();auto pass=encoder.BeginComputePass();pass.SetPipeline(statsPipeline_);pass.SetBindGroup(0,statsGroup_);pass.DispatchWorkgroups(1);pass.End();encoder.CopyBufferToBuffer(snapshot_,0,state->buffer,0,16);const auto commands=encoder.Finish();context_.queue.Submit(1,&commands);
    state->future=state->buffer.MapAsync(wgpu::MapMode::Read,0,16,wgpu::CallbackMode::WaitAnyOnly,[state](wgpu::MapAsyncStatus status,wgpu::StringView message){state->complete=true;state->success=status==wgpu::MapAsyncStatus::Success;if(!state->success)state->error=visualizers::gpu::string(message);});
    if(context_.trackFuture(WGPUFuture{state->future.id})!=0){
        // Quota exhaustion is exceptional. Cancel then deliver this untracked
        // callback before reporting the failure; normal refreshes never wait.
        state->buffer.Destroy();try{context_.wait(state->future,"Rejected bokeh statistics cancellation");}catch(const std::exception& error){stats_.error=error.what();}
        if(!state->complete)throw std::runtime_error("Bokeh statistics cancellation remains pending");state->future={};
        state->buffer=gpuBuffer(context_,16,wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst);
    }return stats_;
}
}
