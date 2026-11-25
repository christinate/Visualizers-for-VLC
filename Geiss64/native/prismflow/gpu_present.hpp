// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_support.hpp"
#include "palette.hpp"
#include <optional>
namespace prismflow {
struct PresentationTitle {double left=0,top=0,width=0,height=0,intensity=0;};
struct PresentationFrame {
    uint32_t width=0,height=0;double oversample=1;
    double layer1Density=0,layer2Brightness=0,headroom=1;
    Color background{1,0,0};std::optional<PresentationTitle> title;
    bool calibrating=false;double selectedSquare=0,calibrationWidth=0;
    uint32_t swatchCount=0;std::optional<Color> swatchBackground;
};
// Private native helper. Prepare once per submitted frame, draw presentation,
// then optional bokeh and palette UI in the caller-owned output render pass.
// RGBA16F output is linear offscreen data; it does not enable display HDR.
class GpuPresentation {
    GpuContext& context_;GpuAssets& assets_;
    wgpu::Buffer uniforms_,uiUniforms_;
    wgpu::Texture palette_,transparent_;wgpu::TextureView paletteView_,transparentView_;
    wgpu::Sampler titleSampler_;wgpu::RenderPipeline pipeline_,uiPipeline_;
    wgpu::BindGroup bindings_,uiBindings_;bool prepared_=false;uint32_t swatches_=0;
public:
    GpuPresentation(GpuContext&,GpuAssets&);
    static std::array<uint8_t,1024> packPalette(std::span<const float>);
    static std::array<float,24> uniformData(const PresentationFrame&);
    static std::array<float,12> uiUniformData(const PresentationFrame&);
    void uploadPalette(std::span<const float>);
    void prepare(const wgpu::Texture& indices,const PresentationFrame&,wgpu::TextureFormat,const wgpu::TextureView& title={});
    void draw(const wgpu::RenderPassEncoder&)const;
    void drawPalette(const wgpu::RenderPassEncoder&)const;
    const wgpu::Texture& paletteTexture()const{return palette_;}
};
}
