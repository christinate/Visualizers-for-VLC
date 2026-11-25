// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
#include <webgpu/webgpu_cpp.h>

namespace visualizers::gpu {
struct DisplayPresentation {
    bool hdrEnabled=false;
    bool linearTransport=false;
    std::string reason="Native HDR presentation is unavailable";
    bool edr=false;
    float headroom=1, potentialHeadroom=1;
    bool compositorManaged=false;
};
struct PresentationChoice {
    bool hdr=false;
    wgpu::TextureFormat format=wgpu::TextureFormat::Undefined;
    std::string reason;
};
inline PresentationChoice choosePresentation(bool requested,const DisplayPresentation& display,
        bool float16Surface,wgpu::TextureFormat sdrFormat,bool linearSupported=true){
    if(!requested)return {false,sdrFormat,"HDR is disabled in settings"};
    if(!linearSupported)return {false,sdrFormat,"This visualizer package declares SDR output only"};
    if(!display.linearTransport)return {false,sdrFormat,display.reason};
    if(!display.edr&&!display.hdrEnabled&&!display.compositorManaged)return {false,sdrFormat,display.reason};
    if(!float16Surface)return {false,sdrFormat,"The native surface does not support RGBA16Float"};
    // EDR's current headroom can be 1 until a layer requests EDR, or when the
    // user selects maximum brightness. Keep a capable requested session linear
    // in float16 so it can tone-map to SDR and follow later headroom changes.
    if(display.edr)return {display.hdrEnabled,wgpu::TextureFormat::RGBA16Float,
        display.hdrEnabled?"Experimental Metal EDR output":"EDR headroom is 1; presenting tone-mapped SDR in the linear surface"};
    if(display.compositorManaged)return {true,wgpu::TextureFormat::RGBA16Float,
        "Experimental Vulkan extended-linear sRGB output"};
    return {true,wgpu::TextureFormat::RGBA16Float,"Experimental Windows scRGB output"};
}
// Plugins' linear output uses 203-nit reference white. Windows' floating-point
// DXGI swapchains and Vulkan extended-linear sRGB presentation use the
// 80-nit scRGB reference white. Metal EDR uses its own reference white.
inline constexpr float scRgbWhiteScale=203.f/80.f;
inline float presentationWhiteScale(const DisplayPresentation& display){return display.edr?1.f:scRgbWhiteScale;}
inline uint32_t presentationOutputMode(const PresentationChoice& choice,bool linearSource){
    return choice.hdr?1u:linearSource?(choice.format==wgpu::TextureFormat::RGBA16Float?3u:2u):0u;
}
inline constexpr const char* presentationShader=R"(
@group(0) @binding(0) var image:texture_2d<f32>;
@group(0) @binding(1) var linearSampler:sampler;
struct Output { mode:u32, whiteScale:f32, reserved:vec2f }
@group(0) @binding(2) var<uniform> output:Output;
struct Vertex { @builtin(position) position:vec4f, @location(0) uv:vec2f }
@vertex fn vertex(@builtin(vertex_index) index:u32)->Vertex {
 let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
 var v:Vertex;v.position=vec4f(p[index],0,1);
 v.uv=p[index]*vec2f(.5,-.5)+vec2f(.5);return v;
}
@fragment fn fragment(input:Vertex)->@location(0) vec4f {
 var color=textureSample(image,linearSampler,input.uv).rgb;
 if(output.mode==1u){color=clamp(color*output.whiteScale,vec3f(-65504),vec3f(65504));}
 if(output.mode==2u){
   color=max(vec3f(0),color);
   color=pow(color/(vec3f(1)+color),vec3f(1.0/2.2));
 }
 if(output.mode==3u){
   color=max(vec3f(0),color);
   color=color/(vec3f(1)+color);
 }
 return vec4f(color,1);
})";
}
