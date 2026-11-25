// SPDX-License-Identifier: GPL-2.0-or-later
#include "context.hpp"
#include "shader_cache.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <chrono>
#include <stdexcept>
#if defined(_WIN32)
#include <dawn/native/D3DBackend.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif
namespace visualizers::gpu {
std::string string(wgpu::StringView value){
    if(!value.data)return {};
    constexpr size_t limit=4096;size_t count=0;
    if(value.length==WGPU_STRLEN){while(count<limit&&value.data[count])++count;}
    else count=std::min(limit,value.length);
    // Driver diagnostics must not allocate in proportion to unbounded messages.
    // Keep a complete UTF-8 prefix if the limit cuts through a code point.
    if(count==limit){size_t start=count-1;while(start&&((uint8_t(value.data[start])&0xc0)==0x80))--start;
        const auto first=uint8_t(value.data[start]);const size_t length=first<0x80?1:first<0xe0?2:first<0xf0?3:4;
        if(start+length>count)count=start;
    }
    return std::string(value.data,count);
}
void Diagnostics::add(const std::string& error){
    std::lock_guard lock(mutex);
    if(errors.size()<32)errors.push_back(string(wgpu::StringView(error.data(),error.size())));
}
void Diagnostics::check(){
    std::lock_guard lock(mutex);
    if(!errors.empty())throw std::runtime_error(errors.front());
}
void Context::pulse(){
    if(startup_&&!startup_.callback(startup_.user))throw std::runtime_error("Shader startup cancelled");
}
void Context::wait(wgpu::Future future,const char* operation){
    // Callback state is heap-owned, so timeout/cancellation cannot retain a stack pointer.
    const WGPUFuture raw{future.id};const bool tracked=trackFuture(raw)==0;
    auto status=wgpu::WaitStatus::TimedOut;
    if(startup_){
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
        do{
            pulse();
            status=instance.WaitAny(future,24'000'000ull);
        }while(status==wgpu::WaitStatus::TimedOut&&std::chrono::steady_clock::now()<deadline);
    }else status=instance.WaitAny(future,30'000'000'000ull);
    if(status!=wgpu::WaitStatus::Success)throw std::runtime_error(std::string(operation)+
        (startup_?" did not complete within three minutes":" did not complete within 30 seconds"));
    if(tracked)forgetFuture(raw);
    else throw std::runtime_error("Native GPU future limit exceeded");
}
Context::Context(wgpu::BackendType backend,bool software,const SurfaceFactory& surfaceFactory,ShaderCache* cache){
    wgpu::InstanceFeatureName feature=wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor descriptor{};descriptor.requiredFeatureCount=1;descriptor.requiredFeatures=&feature;
#if defined(__linux__)
    const char* experimentalApis="allow_experimental_apis";
    wgpu::DawnTogglesDescriptor toggles{};
    waylandColorManagement_=surfaceFactory &&
        (backend==wgpu::BackendType::Vulkan || backend==wgpu::BackendType::Undefined) &&
        dlsym(RTLD_DEFAULT,"dawnVisualizerWaylandExtendedSrgbV1")!=nullptr;
    if(waylandColorManagement_){
        toggles.enabledToggleCount=1;
        toggles.enabledToggles=&experimentalApis;
        descriptor.nextInChain=&toggles;
    }
#endif
    instance=wgpu::CreateInstance(&descriptor);
    if(!instance)throw std::runtime_error("Could not create a native Dawn instance");
    if(surfaceFactory){surface=surfaceFactory(instance);if(!surface)throw std::runtime_error("Could not create native presentation surface");}
    struct AdapterResult {wgpu::Adapter value;std::string error;};
    wgpu::RequestAdapterOptions options{};
    options.backendType=backend;options.powerPreference=wgpu::PowerPreference::HighPerformance;options.forceFallbackAdapter=software;
    options.compatibleSurface=surface;
    const auto request=[&](const wgpu::RequestAdapterOptions& choice){
        auto result=std::make_shared<AdapterResult>();
        wait(instance.RequestAdapter(&choice,wgpu::CallbackMode::WaitAnyOnly,
            [result](wgpu::RequestAdapterStatus status,wgpu::Adapter value,wgpu::StringView message){
                if(status==wgpu::RequestAdapterStatus::Success)result->value=std::move(value);
                else result->error=string(message);
            }),"Adapter request");
        return result;
    };
    std::shared_ptr<AdapterResult> requested;
#if defined(_WIN32)
    const bool canUseWarp=backend==wgpu::BackendType::Undefined||backend==wgpu::BackendType::D3D12;
    if(!software||!canUseWarp)requested=request(options);
    if(canUseWarp&&(!requested||!requested->value)){
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
        DXGI_ADAPTER_DESC1 description{};
        // Dawn's D3D12 backend does not implement forceFallbackAdapter.
        // Select Microsoft's WARP adapter by its DXGI identifier instead.
        if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&
            SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))&&SUCCEEDED(warp->GetDesc1(&description))){
            dawn::native::d3d::RequestAdapterOptionsLUID luid;
            luid.adapterLUID=description.AdapterLuid;
            options.nextInChain=&luid;
            options.backendType=wgpu::BackendType::D3D12;
            options.forceFallbackAdapter=false;
            auto fallback=request(options);
            if(!requested||fallback->value)requested=std::move(fallback);
        }
    }
#else
    requested=request(options);
#endif
    if(!requested)throw std::runtime_error("Microsoft WARP could not be opened as a Direct3D 12 adapter");
    adapter=std::move(requested->value);
    if(!adapter)throw std::runtime_error("No compatible native adapter: "+requested->error);
    struct DeviceResult {wgpu::Device value;std::string error;};
    auto result=std::make_shared<DeviceResult>();
    auto diagnostics=diagnostics_;
    wgpu::DeviceDescriptor deviceDescriptor{};
    WGPUDawnCacheDeviceDescriptor cacheDescriptor=WGPU_DAWN_CACHE_DEVICE_DESCRIPTOR_INIT;
    wgpu::DawnTogglesDescriptor cacheValidation{};
    const char* validate="blob_cache_hash_validation";
    if(cache){
        cache->attach(cacheDescriptor,info().dump());
        cacheValidation.enabledToggleCount=1;
        cacheValidation.enabledToggles=&validate;
        cacheDescriptor.chain.next=reinterpret_cast<WGPUChainedStruct*>(&cacheValidation);
        deviceDescriptor.nextInChain=reinterpret_cast<const wgpu::ChainedStruct*>(&cacheDescriptor);
    }
    deviceDescriptor.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
        [diagnostics](const wgpu::Device&,wgpu::DeviceLostReason reason,wgpu::StringView message){
            if(reason!=wgpu::DeviceLostReason::Destroyed&&reason!=wgpu::DeviceLostReason::CallbackCancelled)
                diagnostics->add("Device lost: "+string(message));
        });
    // Repeatable callbacks use explicit userdata; Context owns it through device teardown.
    deviceDescriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device&,wgpu::ErrorType,wgpu::StringView message,Diagnostics* state){state->add("GPU error: "+string(message));},diagnostics.get());
    wait(adapter.RequestDevice(&deviceDescriptor,wgpu::CallbackMode::WaitAnyOnly,
        [result](wgpu::RequestDeviceStatus status,wgpu::Device value,wgpu::StringView message){
            if(status==wgpu::RequestDeviceStatus::Success)result->value=std::move(value);
            else result->error=string(message);
        }),"Device request");
    device=std::move(result->value);
    if(!device)throw std::runtime_error("Could not create native device: "+result->error);
    queue=device.GetQueue();
}
Context::~Context(){
    closing_=true;
    if(surface)surface.Unconfigure();
    surface=nullptr;
    queue=nullptr;
    if(device&&ownsDevice_)device.Destroy();
    bool completed=true;
    // Destroying the device makes pending operations ready, but WaitAnyOnly
    // callbacks still need delivery. Their resources can retain the instance.
    for(auto& slot:futures_){
        const auto future=slot;if(!future.id)continue;
        if(instance.WaitAny(wgpu::Future{future.id},0)!=wgpu::WaitStatus::Success){completed=false;break;}
        forgetFuture(future);
    }
    if(!completed){
        // An unresponsive driver must not make callback code disappear. Keep
        // affected modules mapped until this separate renderer process exits.
        for(const auto& module:modules_)if(module.pin)module.pin(module.value.get());
    }
    device=nullptr;adapter=nullptr;instance=nullptr;
    // Plugin callback trampolines must remain executable through cancellation.
    modules_.clear();
}
void Context::keepModule(const std::shared_ptr<void>& module,void (*pin)(void*)){
    for(const auto& value:modules_)if(value.value==module)return;
    if(!module||modules_.size()>=32)throw std::runtime_error("Invalid or excessive native module registration");
    modules_.push_back({module,pin});
}
int32_t Context::trackFuture(WGPUFuture future)noexcept{
    if(trackService_)return trackService_(futureUserdata_,future);
    if(closing_||!future.id)return -1;
    for(const auto& slot:futures_)if(slot.id==future.id)return 0;
    for(auto& slot:futures_)if(!slot.id){slot=future;return 0;}
    return -1;
}
void Context::forgetFuture(WGPUFuture future)noexcept{
    if(forgetService_){forgetService_(futureUserdata_,future);return;}
    for(auto& slot:futures_)if(slot.id==future.id){slot.id=0;return;}
}
Context::Context(const BorrowedGpu& borrowed):ownsDevice_(false),startup_(borrowed.startup){
    if(!borrowed.instance||!borrowed.adapter||!borrowed.device||!borrowed.queue)throw std::runtime_error("Borrowed GPU handles are incomplete");
    // WebGPU C++ constructors AddRef; destruction releases these references
    // without destroying the underlying host-owned device.
    instance=wgpu::Instance(borrowed.instance);adapter=wgpu::Adapter(borrowed.adapter);
    device=wgpu::Device(borrowed.device);queue=wgpu::Queue(borrowed.queue);
    futureUserdata_=borrowed.future_userdata;trackService_=borrowed.track_future;forgetService_=borrowed.forget_future;
}
nlohmann::json Context::info()const{
    wgpu::AdapterInfo value{};
    if(adapter.GetInfo(&value)!=wgpu::Status::Success)throw std::runtime_error("Adapter information unavailable");
    const char* backend="unknown";
    switch(value.backendType){case wgpu::BackendType::D3D12:backend="D3D12";break;case wgpu::BackendType::Metal:backend="Metal";break;case wgpu::BackendType::Vulkan:backend="Vulkan";break;case wgpu::BackendType::OpenGL:backend="OpenGL";break;case wgpu::BackendType::OpenGLES:backend="OpenGLES";break;default:break;}
    return {{"backend",backend},{"vendor",string(value.vendor)},{"architecture",string(value.architecture)},
        {"device",string(value.device)},{"description",string(value.description)},
        {"vendor_id",value.vendorID},{"device_id",value.deviceID},{"software",value.adapterType==wgpu::AdapterType::CPU}};
}
wgpu::ShaderModule Context::shader(const std::string& source){
    wgpu::ShaderSourceWGSL code{};code.code=wgpu::StringView(source.data(),source.size());
    wgpu::ShaderModuleDescriptor descriptor{};descriptor.nextInChain=&code;
    auto module=device.CreateShaderModule(&descriptor);
    auto error=std::make_shared<std::string>();
    wait(module.GetCompilationInfo(wgpu::CallbackMode::WaitAnyOnly,
        [error](wgpu::CompilationInfoRequestStatus status,const wgpu::CompilationInfo* info){
            if(status!=wgpu::CompilationInfoRequestStatus::Success||!info){*error="Shader diagnostics unavailable";return;}
            unsigned count=0;for(size_t n=0;n<info->messageCount;++n)if(info->messages[n].type==wgpu::CompilationMessageType::Error){
                *error+=string(info->messages[n].message)+"\n";if(++count==32)break;
            }
        }),"Shader compilation");
    if(!error->empty())throw std::runtime_error(*error);
    checkErrors();return module;
}
wgpu::Texture Context::target(unsigned width,unsigned height){
    wgpu::Limits limits{};
    if(device.GetLimits(&limits)!=wgpu::Status::Success)throw std::runtime_error("Cannot query native texture limits");
    if(!width||!height||width>limits.maxTextureDimension2D||height>limits.maxTextureDimension2D||uint64_t(width)*height>16'777'216)
        throw std::runtime_error("Render dimensions exceed device limits or the 16-million-pixel budget");
    wgpu::TextureDescriptor descriptor{};descriptor.dimension=wgpu::TextureDimension::e2D;
    descriptor.size={width,height,1};descriptor.format=wgpu::TextureFormat::RGBA16Float;
    descriptor.usage=wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc|wgpu::TextureUsage::TextureBinding;
    return device.CreateTexture(&descriptor);
}
std::vector<uint16_t> Context::readRgba16(const wgpu::Texture& texture,unsigned width,unsigned height){
    const uint32_t rowBytes=(width*8+255)&~255u;
    wgpu::BufferDescriptor bufferDescriptor{};bufferDescriptor.size=uint64_t(rowBytes)*height;
    bufferDescriptor.usage=wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::MapRead;
    auto buffer=device.CreateBuffer(&bufferDescriptor);
    auto encoder=device.CreateCommandEncoder();
    wgpu::TexelCopyTextureInfo source{};source.texture=texture;source.aspect=wgpu::TextureAspect::All;
    wgpu::TexelCopyBufferInfo destination{};destination.buffer=buffer;destination.layout.bytesPerRow=rowBytes;destination.layout.rowsPerImage=height;
    wgpu::Extent3D size{width,height,1};encoder.CopyTextureToBuffer(&source,&destination,&size);
    auto commands=encoder.Finish();queue.Submit(1,&commands);
    auto error=std::make_shared<std::string>();
    wait(buffer.MapAsync(wgpu::MapMode::Read,0,bufferDescriptor.size,wgpu::CallbackMode::WaitAnyOnly,
        [error](wgpu::MapAsyncStatus status,wgpu::StringView message){if(status!=wgpu::MapAsyncStatus::Success)*error="Readback failed: "+string(message);}),"GPU readback");
    if(!error->empty())throw std::runtime_error(*error);
    const auto* mapped=static_cast<const unsigned char*>(buffer.GetConstMappedRange());
    if(!mapped)throw std::runtime_error("GPU readback returned no mapped data");
    std::vector<uint16_t> result(size_t(width)*height*4);
    for(unsigned y=0;y<height;++y)std::memcpy(result.data()+size_t(y)*width*4,mapped+size_t(y)*rowBytes,width*8);
    buffer.Unmap();checkErrors();return result;
}
wgpu::RenderPipeline pipeline(Context& context,const wgpu::ShaderModule& shader,const char* vertex,const char* fragment,wgpu::TextureFormat format){
    wgpu::ColorTargetState color{};color.format=format;color.writeMask=wgpu::ColorWriteMask::All;
    wgpu::FragmentState fragmentState{};fragmentState.module=shader;fragmentState.entryPoint=fragment;fragmentState.targetCount=1;fragmentState.targets=&color;
    wgpu::RenderPipelineDescriptor descriptor{};descriptor.vertex.module=shader;descriptor.vertex.entryPoint=vertex;descriptor.fragment=&fragmentState;
    descriptor.primitive.topology=wgpu::PrimitiveTopology::TriangleList;
    if(context.hasStartupPulse()){
        struct Result {wgpu::RenderPipeline pipeline;std::string error;};
        auto result=std::make_shared<Result>();
        context.wait(context.device.CreateRenderPipelineAsync(&descriptor,wgpu::CallbackMode::WaitAnyOnly,
            [result](wgpu::CreatePipelineAsyncStatus status,wgpu::RenderPipeline value,wgpu::StringView error){
                if(status==wgpu::CreatePipelineAsyncStatus::Success)result->pipeline=std::move(value);
                else result->error=string(error);
            }),"Beach render pipeline compilation");
        if(!result->pipeline)throw std::runtime_error("Beach shader compilation failed: "+result->error);
        return std::move(result->pipeline);
    }
    return context.device.CreateRenderPipeline(&descriptor);
}
void render(Context& context,const wgpu::RenderPipeline& pipeline,const wgpu::TextureView& target,const wgpu::BindGroup& bindings){
    wgpu::RenderPassColorAttachment color{};color.view=target;color.loadOp=wgpu::LoadOp::Clear;color.storeOp=wgpu::StoreOp::Store;color.clearValue={0,0,0,1};
    wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;
    auto encoder=context.device.CreateCommandEncoder();auto pass=encoder.BeginRenderPass(&descriptor);
    pass.SetPipeline(pipeline);if(bindings)pass.SetBindGroup(0,bindings);pass.Draw(3);pass.End();
    auto commands=encoder.Finish();context.queue.Submit(1,&commands);
}
float decodeHalf(uint16_t value){
    const float sign=(value&0x8000)?-1.f:1.f;const unsigned exponent=(value>>10)&31,mantissa=value&1023;
    if(!exponent)return sign*std::ldexp(float(mantissa),-24);
    if(exponent==31)return mantissa?NAN:sign*INFINITY;
    return sign*std::ldexp(1.f+float(mantissa)/1024.f,int(exponent)-15);
}
uint16_t encodeHalf(float value){
    if(!std::isfinite(value))return 0;
    const uint32_t bits=std::bit_cast<uint32_t>(value);const uint16_t sign=(bits>>16)&0x8000;
    const int exponent=int((bits>>23)&255)-127;const uint32_t mantissa=bits&0x7fffff;
    if(exponent<-25)return sign;
    if(exponent>15)return sign|0x7bff;
    // Float16 subnormals use the implicit leading one. Preserve them and
    // apply round-to-nearest-even, including the zero/subnormal boundary.
    const unsigned shift=exponent<-14?unsigned(-exponent-1):13;
    const uint32_t significand=exponent<-14?mantissa|0x800000u:mantissa;
    uint32_t rounded=significand>>shift;const uint32_t remainder=significand&((1u<<shift)-1),halfway=1u<<(shift-1);
    if(remainder>halfway||(remainder==halfway&&(rounded&1)))++rounded;
    const uint32_t base=exponent<-14?0:uint32_t(exponent+15)<<10;
    // Keep the existing finite saturation/nonfinite-to-zero host policy.
    return sign|uint16_t(std::min(0x7bffu,base+rounded));
}
}
