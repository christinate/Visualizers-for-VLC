// SPDX-License-Identifier: GPL-2.0-or-later
// Native Dawn host support; no C++ ownership crosses the plugin C ABI.
#pragma once
#include "context.hpp"
#include <filesystem>
#include <span>
#include <unordered_map>
namespace prismflow {
using GpuContext=visualizers::gpu::Context;
struct ShaderSpec {
    std::string name,file,kind,entry,vertex,fragment,topology,blend,layout,vertexBuffers;
    std::vector<std::string> formats;
};
class GpuAssets {
    GpuContext& context_;
    std::filesystem::path directory_;
    std::vector<ShaderSpec> specs_;
    std::unordered_map<std::string,wgpu::ShaderModule> modules_;
    std::unordered_map<std::string,wgpu::RenderPipeline> renders_;
    std::unordered_map<std::string,wgpu::ComputePipeline> computes_;
    wgpu::BindGroupLayout dynamicGroup_;
    wgpu::PipelineLayout dynamicLayout_;
    const ShaderSpec& spec(const std::string&)const;
    wgpu::ShaderModule module(const ShaderSpec&);
public:
    GpuAssets(GpuContext&,const std::filesystem::path& directory);
    std::span<const ShaderSpec> specs()const{return specs_;}
    wgpu::RenderPipeline render(const std::string& name,wgpu::TextureFormat format);
    wgpu::ComputePipeline compute(const std::string& name);
    const wgpu::BindGroupLayout& dynamicGroup()const{return dynamicGroup_;}
};
wgpu::TextureFormat textureFormat(const std::string&);
void checkGpuScope(GpuContext&,const char* operation);
wgpu::Buffer gpuBuffer(GpuContext&,uint64_t bytes,wgpu::BufferUsage);
wgpu::Texture gpuTexture(GpuContext&,uint32_t width,uint32_t height,wgpu::TextureFormat,
    wgpu::TextureUsage,uint32_t depth=1,wgpu::TextureDimension dimension=wgpu::TextureDimension::e2D);
wgpu::BindGroupEntry bindBuffer(uint32_t,const wgpu::Buffer&);
wgpu::BindGroupEntry bindTexture(uint32_t,const wgpu::TextureView&);
wgpu::BindGroupEntry bindSampler(uint32_t,const wgpu::Sampler&);
wgpu::BindGroup gpuBindings(GpuContext&,const wgpu::BindGroupLayout&,std::span<const wgpu::BindGroupEntry>);
void uploadHalfTexture(GpuContext&,const wgpu::Texture&,std::span<const uint16_t>);
// Diagnostic readback only. Production frame encoding does not wait or map.
std::vector<uint16_t> readHalfTexture(GpuContext&,const wgpu::Texture&);
std::vector<uint8_t> readByteTexture(GpuContext&,const wgpu::Texture&);
std::vector<uint8_t> readGpuBuffer(GpuContext&,const wgpu::Buffer&);
}
