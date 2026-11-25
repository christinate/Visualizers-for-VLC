// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu_support.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
namespace prismflow {
using json=nlohmann::json;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string readFile(const std::filesystem::path& file,size_t bound){
    require(std::filesystem::file_size(file)<=bound,"Prism Flow GPU input exceeds its bound");
    std::ifstream input(file,std::ios::binary);require(bool(input),"Cannot open Prism Flow GPU input");return {std::istreambuf_iterator<char>(input),{}};
}
uint32_t halfChannels(wgpu::TextureFormat format){
    if(format==wgpu::TextureFormat::R16Float)return 1;if(format==wgpu::TextureFormat::RG16Float)return 2;
    if(format==wgpu::TextureFormat::RGBA16Float)return 4;throw std::invalid_argument("Expected an R16F, RG16F or RGBA16F texture");
}
bool assetName(const std::string& name){return !name.empty()&&name.size()<=64&&std::all_of(name.begin(),name.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});}
}
wgpu::TextureFormat textureFormat(const std::string& value){
    if(value=="rgba16float")return wgpu::TextureFormat::RGBA16Float;if(value=="rg16float")return wgpu::TextureFormat::RG16Float;
    if(value=="rgba8unorm")return wgpu::TextureFormat::RGBA8Unorm;if(value=="bgra8unorm")return wgpu::TextureFormat::BGRA8Unorm;
    throw std::runtime_error("Unknown Prism Flow shader target format");
}
void checkGpuScope(GpuContext& context,const char* operation){
    struct Scope {bool success=false;std::string error;};auto scope=std::make_shared<Scope>();
    context.wait(context.device.PopErrorScope(wgpu::CallbackMode::WaitAnyOnly,[scope](wgpu::PopErrorScopeStatus status,wgpu::ErrorType type,wgpu::StringView message){
        scope->success=status==wgpu::PopErrorScopeStatus::Success;if(type!=wgpu::ErrorType::NoError)scope->error=visualizers::gpu::string(message);
    }),operation);
    require(scope->success,"Prism Flow GPU error scope did not complete");if(!scope->error.empty())throw std::runtime_error(scope->error);context.checkErrors();
}
GpuAssets::GpuAssets(GpuContext& context,const std::filesystem::path& directory):context_(context),directory_(directory){
    const auto manifest=json::parse(readFile(directory/"manifest.json",64*1024));
    require(manifest.at("format_version")==1&&manifest.at("shaders").is_array()&&!manifest.at("shaders").empty()&&manifest.at("shaders").size()<=64,"Invalid native shader manifest");
    std::set<std::string> names;
    for(const auto& entry:manifest.at("shaders")){
        ShaderSpec value;value.name=entry.at("name");value.file=entry.at("file");value.kind=entry.at("kind");
        require(assetName(value.name)&&value.file==value.name+".wgsl"&&names.insert(value.name).second,"Invalid or duplicate native shader asset");
        if(value.kind=="compute"){value.entry=entry.at("entry");require(assetName(value.entry),"Invalid compute entry point");}
        else{
            require(value.kind=="render","Invalid native shader kind");value.vertex=entry.at("vertex");value.fragment=entry.at("fragment");value.topology=entry.at("topology");
            value.formats=entry.at("formats").get<std::vector<std::string>>();value.blend=entry.value("blend",std::string());value.layout=entry.value("layout",std::string());value.vertexBuffers=entry.value("vertex_buffers",std::string());
            require(assetName(value.vertex)&&assetName(value.fragment)&&!value.formats.empty()&&value.formats.size()<=4,"Invalid render entry points/formats");
            require(value.topology=="triangle-list"||value.topology=="triangle-strip","Invalid primitive topology");
            require(value.blend.empty()||value.blend=="additive"||value.blend=="bokeh","Invalid native blend policy");
            require(value.layout.empty()||value.layout=="dynamic","Invalid native pipeline layout");require(value.vertexBuffers.empty()||value.vertexBuffers=="dots","Invalid native vertex layout");
            std::set<std::string> formats;for(const auto& format:value.formats){textureFormat(format);require(formats.insert(format).second,"Duplicate native target format");}
        }
        specs_.push_back(std::move(value));
    }
    std::array<wgpu::BindGroupLayoutEntry,7> entries{};
    for(unsigned i=0;i<entries.size();++i){auto& entry=entries[i];entry.binding=i;entry.visibility=wgpu::ShaderStage::Fragment;
        if(i==0)entry.buffer.type=wgpu::BufferBindingType::Uniform;
        else if(i==4||i==6)entry.sampler.type=wgpu::SamplerBindingType::Filtering;
        else{entry.texture.sampleType=wgpu::TextureSampleType::Float;entry.texture.viewDimension=i==1?wgpu::TextureViewDimension::e3D:wgpu::TextureViewDimension::e2D;}}
    wgpu::BindGroupLayoutDescriptor group{};group.entryCount=entries.size();group.entries=entries.data();dynamicGroup_=context.device.CreateBindGroupLayout(&group);
    wgpu::PipelineLayoutDescriptor layout{};layout.bindGroupLayoutCount=1;layout.bindGroupLayouts=&dynamicGroup_;dynamicLayout_=context.device.CreatePipelineLayout(&layout);
    context.checkErrors();
}
const ShaderSpec& GpuAssets::spec(const std::string& name)const{
    for(const auto& value:specs_)if(value.name==name)return value;throw std::invalid_argument("Unknown Prism Flow shader");
}
wgpu::ShaderModule GpuAssets::module(const ShaderSpec& value){
    if(const auto found=modules_.find(value.name);found!=modules_.end())return found->second;
    return modules_.emplace(value.name,context_.shader(readFile(directory_/value.file,1024*1024))).first->second;
}
wgpu::ComputePipeline GpuAssets::compute(const std::string& name){
    if(const auto found=computes_.find(name);found!=computes_.end())return found->second;
    const auto& value=spec(name);require(value.kind=="compute","Expected a compute shader");const auto shader=module(value);
    context_.device.PushErrorScope(wgpu::ErrorFilter::Validation);wgpu::ComputePipelineDescriptor descriptor{};descriptor.compute.module=shader;descriptor.compute.entryPoint=value.entry.c_str();
    auto pipeline=context_.device.CreateComputePipeline(&descriptor);checkGpuScope(context_,"Prism Flow compute pipeline");require(bool(pipeline),"Missing Prism Flow compute pipeline");return computes_.emplace(name,std::move(pipeline)).first->second;
}
wgpu::RenderPipeline GpuAssets::render(const std::string& name,wgpu::TextureFormat format){
    const std::string key=name+':'+std::to_string(uint32_t(format));if(const auto found=renders_.find(key);found!=renders_.end())return found->second;
    const auto& value=spec(name);require(value.kind=="render","Expected a render shader");
    require(std::any_of(value.formats.begin(),value.formats.end(),[&](const auto& f){return textureFormat(f)==format;}),"Prism Flow target format is not declared");
    const auto shader=module(value);wgpu::BlendState blend{};blend.color.operation=blend.alpha.operation=wgpu::BlendOperation::Add;
    blend.color.srcFactor=blend.color.dstFactor=blend.alpha.srcFactor=blend.alpha.dstFactor=wgpu::BlendFactor::One;
    wgpu::ColorTargetState color{};color.format=format;if(!value.blend.empty())color.blend=&blend;
    if(value.blend=="bokeh"){color.writeMask=wgpu::ColorWriteMask::Red|wgpu::ColorWriteMask::Green|wgpu::ColorWriteMask::Blue;blend.alpha.srcFactor=wgpu::BlendFactor::Zero;}
    std::array<wgpu::VertexAttribute,2> attributes{};attributes[0].shaderLocation=0;attributes[0].format=wgpu::VertexFormat::Float32x2;attributes[1].shaderLocation=1;attributes[1].format=wgpu::VertexFormat::Float32x4;
    std::array<wgpu::VertexBufferLayout,2> buffers{};for(unsigned i=0;i<2;++i){buffers[i].arrayStride=i?16:8;buffers[i].stepMode=i?wgpu::VertexStepMode::Instance:wgpu::VertexStepMode::Vertex;buffers[i].attributeCount=1;buffers[i].attributes=&attributes[i];}
    wgpu::FragmentState fragment{};fragment.module=shader;fragment.entryPoint=value.fragment.c_str();fragment.targetCount=1;fragment.targets=&color;
    wgpu::RenderPipelineDescriptor descriptor{};descriptor.vertex.module=shader;descriptor.vertex.entryPoint=value.vertex.c_str();descriptor.fragment=&fragment;
    if(value.vertexBuffers=="dots"){descriptor.vertex.bufferCount=2;descriptor.vertex.buffers=buffers.data();}if(value.layout=="dynamic")descriptor.layout=dynamicLayout_;
    descriptor.primitive.topology=value.topology=="triangle-strip"?wgpu::PrimitiveTopology::TriangleStrip:wgpu::PrimitiveTopology::TriangleList;
    context_.device.PushErrorScope(wgpu::ErrorFilter::Validation);auto pipeline=context_.device.CreateRenderPipeline(&descriptor);checkGpuScope(context_,"Prism Flow render pipeline");require(bool(pipeline),"Missing Prism Flow render pipeline");return renders_.emplace(key,std::move(pipeline)).first->second;
}
wgpu::Buffer gpuBuffer(GpuContext& context,uint64_t bytes,wgpu::BufferUsage usage){
    if(!bytes||bytes%4||bytes>128*1024*1024)throw std::invalid_argument("Prism Flow GPU buffer exceeds its budget");
    wgpu::BufferDescriptor descriptor{};descriptor.size=bytes;descriptor.usage=usage;return context.device.CreateBuffer(&descriptor);
}
wgpu::Texture gpuTexture(GpuContext& context,uint32_t width,uint32_t height,wgpu::TextureFormat format,wgpu::TextureUsage usage,uint32_t depth,wgpu::TextureDimension dimension){
    wgpu::Limits limits{};if(context.device.GetLimits(&limits)!=wgpu::Status::Success)throw std::runtime_error("Cannot query GPU texture limits");
    const uint32_t limit=dimension==wgpu::TextureDimension::e3D?limits.maxTextureDimension3D:limits.maxTextureDimension2D;
    if(!width||!height||!depth||width>limit||height>limit||depth>limit||uint64_t(width)*height*depth>16777216||
        (dimension!=wgpu::TextureDimension::e2D&&dimension!=wgpu::TextureDimension::e3D)||(dimension==wgpu::TextureDimension::e2D&&depth!=1))throw std::invalid_argument("Prism Flow texture exceeds its dimension/texel budget");
    wgpu::TextureDescriptor descriptor{};descriptor.size={width,height,depth};descriptor.dimension=dimension;descriptor.format=format;descriptor.usage=usage;return context.device.CreateTexture(&descriptor);
}
wgpu::BindGroupEntry bindBuffer(uint32_t binding,const wgpu::Buffer& buffer){wgpu::BindGroupEntry entry{};entry.binding=binding;entry.buffer=buffer;entry.size=buffer.GetSize();return entry;}
wgpu::BindGroupEntry bindTexture(uint32_t binding,const wgpu::TextureView& view){wgpu::BindGroupEntry entry{};entry.binding=binding;entry.textureView=view;return entry;}
wgpu::BindGroupEntry bindSampler(uint32_t binding,const wgpu::Sampler& sampler){wgpu::BindGroupEntry entry{};entry.binding=binding;entry.sampler=sampler;return entry;}
wgpu::BindGroup gpuBindings(GpuContext& context,const wgpu::BindGroupLayout& layout,std::span<const wgpu::BindGroupEntry> entries){
    if(entries.empty()||entries.size()>32)throw std::invalid_argument("Invalid Prism Flow binding count");wgpu::BindGroupDescriptor descriptor{};descriptor.layout=layout;descriptor.entryCount=entries.size();descriptor.entries=entries.data();return context.device.CreateBindGroup(&descriptor);
}
void uploadHalfTexture(GpuContext& context,const wgpu::Texture& texture,std::span<const uint16_t> values){
    const uint32_t channels=halfChannels(texture.GetFormat()),width=texture.GetWidth(),height=texture.GetHeight(),depth=texture.GetDepthOrArrayLayers();
    if(values.size()!=uint64_t(width)*height*depth*channels)throw std::invalid_argument("Half-float texture upload size differs");
    wgpu::TexelCopyTextureInfo destination{};destination.texture=texture;wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=width*channels*2;layout.rowsPerImage=height;
    const wgpu::Extent3D extent{width,height,depth};context.queue.WriteTexture(&destination,values.data(),values.size_bytes(),&layout,&extent);
}
std::vector<uint16_t> readHalfTexture(GpuContext& context,const wgpu::Texture& texture){
    const uint32_t channels=halfChannels(texture.GetFormat()),width=texture.GetWidth(),height=texture.GetHeight(),depth=texture.GetDepthOrArrayLayers();
    if(!width||!height||!depth||uint64_t(width)*height*depth>16777216)throw std::invalid_argument("Half-float readback exceeds its texel budget");
    const uint32_t rowBytes=(width*channels*2+255)&~255u;const uint64_t bytes=uint64_t(rowBytes)*height*depth;auto buffer=gpuBuffer(context,bytes,wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::MapRead);
    wgpu::TexelCopyTextureInfo source{};source.texture=texture;wgpu::TexelCopyBufferInfo destination{};destination.buffer=buffer;destination.layout.bytesPerRow=rowBytes;destination.layout.rowsPerImage=height;
    const wgpu::Extent3D extent{width,height,depth};auto encoder=context.device.CreateCommandEncoder();encoder.CopyTextureToBuffer(&source,&destination,&extent);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
    auto error=std::make_shared<std::string>();context.wait(buffer.MapAsync(wgpu::MapMode::Read,0,bytes,wgpu::CallbackMode::WaitAnyOnly,[error](wgpu::MapAsyncStatus status,wgpu::StringView message){
        if(status!=wgpu::MapAsyncStatus::Success)*error="Prism Flow readback failed: "+visualizers::gpu::string(message);
    }),"Prism Flow texture readback");if(!error->empty())throw std::runtime_error(*error);
    std::vector<uint16_t> values(size_t(width)*height*depth*channels);const auto* mapped=static_cast<const unsigned char*>(buffer.GetConstMappedRange());
    require(mapped!=nullptr,"Half-float readback returned no mapped data");
    for(size_t row=0;row<size_t(height)*depth;++row)std::memcpy(values.data()+row*width*channels,mapped+row*rowBytes,size_t(width)*channels*2);
    buffer.Unmap();context.checkErrors();return values;
}
std::vector<uint8_t> readByteTexture(GpuContext& context,const wgpu::Texture& texture){
    if(texture.GetFormat()!=wgpu::TextureFormat::RGBA8Unorm&&texture.GetFormat()!=wgpu::TextureFormat::BGRA8Unorm)throw std::invalid_argument("Expected an RGBA8 or BGRA8 texture");
    const uint32_t width=texture.GetWidth(),height=texture.GetHeight(),depth=texture.GetDepthOrArrayLayers();
    if(!width||!height||!depth||uint64_t(width)*height*depth>16777216)throw std::invalid_argument("Byte-texture readback exceeds its texel budget");
    const uint32_t rowBytes=(width*4+255)&~255u;const uint64_t bytes=uint64_t(rowBytes)*height*depth;auto buffer=gpuBuffer(context,bytes,wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::MapRead);
    wgpu::TexelCopyTextureInfo source{};source.texture=texture;wgpu::TexelCopyBufferInfo destination{};destination.buffer=buffer;destination.layout.bytesPerRow=rowBytes;destination.layout.rowsPerImage=height;
    const wgpu::Extent3D extent{width,height,depth};auto encoder=context.device.CreateCommandEncoder();encoder.CopyTextureToBuffer(&source,&destination,&extent);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
    auto error=std::make_shared<std::string>();context.wait(buffer.MapAsync(wgpu::MapMode::Read,0,bytes,wgpu::CallbackMode::WaitAnyOnly,[error](wgpu::MapAsyncStatus status,wgpu::StringView message){
        if(status!=wgpu::MapAsyncStatus::Success)*error="Prism Flow byte readback failed: "+visualizers::gpu::string(message);
    }),"Prism Flow byte-texture readback");if(!error->empty())throw std::runtime_error(*error);
    const auto* mapped=static_cast<const uint8_t*>(buffer.GetConstMappedRange());require(mapped!=nullptr,"Byte readback returned no mapped data");std::vector<uint8_t> result(size_t(width)*height*depth*4);
    for(size_t row=0;row<size_t(height)*depth;++row)std::memcpy(result.data()+row*width*4,mapped+row*rowBytes,size_t(width)*4);buffer.Unmap();context.checkErrors();return result;
}
std::vector<uint8_t> readGpuBuffer(GpuContext& context,const wgpu::Buffer& source){
    const uint64_t bytes=source.GetSize();auto staging=gpuBuffer(context,bytes,wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst);auto encoder=context.device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(source,0,staging,0,bytes);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);auto error=std::make_shared<std::string>();
    context.wait(staging.MapAsync(wgpu::MapMode::Read,0,bytes,wgpu::CallbackMode::WaitAnyOnly,[error](wgpu::MapAsyncStatus status,wgpu::StringView message){if(status!=wgpu::MapAsyncStatus::Success)*error=visualizers::gpu::string(message);}),"Prism Flow buffer readback");
    if(!error->empty())throw std::runtime_error(*error);const auto* mapped=static_cast<const uint8_t*>(staging.GetConstMappedRange());require(mapped!=nullptr,"Buffer readback returned no mapped data");std::vector<uint8_t> result(mapped,mapped+bytes);staging.Unmap();context.checkErrors();return result;
}
}
