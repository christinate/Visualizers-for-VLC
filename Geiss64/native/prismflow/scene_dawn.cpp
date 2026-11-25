// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR engine.js/adapter.mjs, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "scene_dawn.hpp"
#include "text_raster.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace prismflow {namespace {
constexpr const char* conversionShader=R"(
@group(0) @binding(0) var source:texture_2d<f32>;
@vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {
 let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);
}
@fragment fn fs(@builtin(position) p:vec4f)->@location(0) vec4f {return textureLoad(source,vec2i(p.xy),0);}
)";
uint32_t seedDraw(const RandomFunction& random){return uint32_t(random()*4294967296.);}
uint32_t textUnits(const std::string& text){uint32_t count=0;for(uint8_t c:text)if((c&0xc0)!=0x80)count+=c>=0xf0?2:1;return count;}
}
uint32_t DawnScene::scaleFor(uint32_t width,uint32_t height)const{
    wgpu::Limits limits{};if(context_.device.GetLimits(&limits)!=wgpu::Status::Success||!width||!height||width>limits.maxTextureDimension2D||height>limits.maxTextureDimension2D||uint64_t(width)*height>16777216)throw std::invalid_argument("Prism Flow output exceeds device limits");
    const auto scale=options_.supersampling;if(uint64_t(width)*height*scale*scale>16777216||uint64_t(width)*scale>limits.maxTextureDimension2D||uint64_t(height)*scale>limits.maxTextureDimension2D)return 1;return scale;
}
DawnScene::DawnScene(GpuContext& context,const std::filesystem::path& shaders,uint32_t width,uint32_t height,const SceneOptions& options):context_(context),options_(options),rng_(options.seed),random_([this]{++randomCalls_;return rng_();}),dynamicSeed_(seedDraw(random_)),warpSeed_(seedDraw(random_)),assets_(context,shaders),dynamic_(context,assets_,dynamicSeed_),dots_(context,assets_),warpDots_(context,assets_,warpSeed_),bokeh_(context,assets_),feedback_(context,assets_,dynamic_,width,height,random_),presentation_(context,assets_),text_(context,assets_){
    if(!options_.supersampling||options_.supersampling>4||!std::isfinite(options_.headroom)||options_.headroom<1||options_.headroom>65504||(!options_.linear&&options_.headroom!=1))throw std::invalid_argument("Invalid native Prism Flow output settings");
    // Shared dynamic-noise and warp-dot seeds precede the Gaussian draw bank.
    // The presenter's initial index upload precedes Engine.seed's unused bank.
    supersampling_=scaleFor(width,height);if(supersampling_!=1)feedback_.resize(width*supersampling_,height*supersampling_,random_);else feedback_.fillIndices(random_);width_=width;height_=height;
    bokeh_.seedHistory(4);spawns_.reserve(GpuBokeh::capacity);unusedSeedDraws();convert_=visualizers::gpu::pipeline(context_,context_.shader(conversionShader),"vs","fs");targets(width,height);
}
DawnScene::~DawnScene(){images_.reset();presets_.worker.close();feedback_.abortFrame();}
void DawnScene::unusedSeedDraws(){for(uint64_t n=0;n<uint64_t(feedback_.width())*feedback_.height();++n)random_();}
void DawnScene::targets(uint32_t width,uint32_t height){
    const auto format=options_.linear?wgpu::TextureFormat::RGBA16Float:wgpu::TextureFormat::RGBA8Unorm;
    auto color=gpuTexture(context_,width,height,format,wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopySrc);auto output=context_.target(width,height);auto colorView=color.CreateView();auto outputView=output.CreateView();const std::array entry={bindTexture(0,colorView)};auto group=gpuBindings(context_,convert_.GetBindGroupLayout(0),entry);
    context_.checkErrors();conversion_=std::move(group);color_=std::move(color);output_=std::move(output);colorView_=std::move(colorView);outputView_=std::move(outputView);
}
void DawnScene::resize(uint32_t width,uint32_t height){
    const auto scale=scaleFor(width,height);if(width==width_&&height==height_&&scale==supersampling_)return;
    targets(width,height);feedback_.resize(width*scale,height*scale,random_);width_=width;height_=height;supersampling_=scale;unusedSeedDraws();
}
void DawnScene::embed(std::string text){if(text.size()>8192)throw std::invalid_argument("Embedded title exceeds its UTF-8 budget");embed_=std::move(text);}
void DawnScene::importImage(std::filesystem::path path,uint64_t token){
    if(!path.is_absolute()||!token)throw std::invalid_argument("Image import needs an absolute path and nonzero token");
    if(!images_)images_=std::make_unique<ImagePaletteWorker>();images_->request(std::move(path),token,seedDraw(random_));imageStatus_={ImageImportState::Pending,token,{}};
}
void DawnScene::cancelImageImport(uint64_t token){
    if(token&&token!=imageStatus_.token)return;
    if(images_)images_->cancel();if(imageStatus_.state==ImageImportState::Pending)imageStatus_={ImageImportState::Cancelled,imageStatus_.token,{}};
}
void DawnScene::key(uint32_t code,uint32_t modifiers,bool down){
    if(!down||modifiers)return;switch(code){case 32:presetTime_=presets_.history.next(presetTime_,false);break;case 13:case 1073741912:presetTime_=presets_.history.next(presetTime_,true);break;case 8:presetTime_=presets_.history.previous(presetTime_);break;case 'l':case 'L':locked_=!locked_;break;case 'm':case 'M':requests_.motion=true;break;case 'p':case 'P':cancelImageImport();requests_.palette=true;break;case 'w':case 'W':requests_.wave=true;break;case 'd':case 'D':requests_.dots=true;break;case 's':case 'S':requests_.shader=true;break;default:break;}
}
void DawnScene::render(const visualizers::MusicFrame& audio,double time,double dt,std::optional<double> referenceStarLifetime){
    if(!std::isfinite(time)||time<0||!std::isfinite(dt)||dt<0||dt>1)throw std::invalid_argument("Invalid Prism Flow frame time");if(frames_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Prism Flow frame counter exhausted");
    bool imageReady=false;if(images_)if(auto completed=images_->take()){
        if(imageStatus_.state==ImageImportState::Pending&&completed->token==imageStatus_.token){
            if(completed->palette){requests_.overwrite=std::move(*completed->palette);imageReady=true;}
            else imageStatus_={ImageImportState::Failed,completed->token,completed->error.empty()?"Native image import failed":completed->error};
        }
    }
    if(!locked_)presetTime_+=dt;const auto active=presets_.tick(presetTime_,random_);applyFrameRequests(presets_,active,requests_,random_);requests_={};if(imageReady)imageStatus_={ImageImportState::Applied,imageStatus_.token,{}};
    for(size_t n=0;n<spectrum_.size();++n)spectrum_[n]=float((double(audio.spectrum[n*2])+audio.spectrum[n*2+1])*.5);feedback_.uploadSpectrum(spectrum_);
    const auto stats=bokeh_.stats(time*1000);FrameInput controls;controls.width=feedback_.width();controls.height=feedback_.height();controls.shiftTime=presetTime_;controls.time=time;controls.fps=1./std::max(dt,.001);controls.frozen=frozen_;controls.paletteLocked=paletteLocked_;controls.headroom=options_.headroom;controls.averageStarLifetime=referenceStarLifetime.value_or(stats.mean.value_or(4));controls.beat=audio.beat?1:0;controls.embedUnits=textUnits(embed_);
    plan_=planner_.build(presets_.history,active,controls,{audio.wave,audio.rms,audio.visualEnergy},random_,std::move(plan_));presentation_.uploadPalette(plan_.palette);bokeh_.palette(plan_.palette);bokeh_.follow(plan_.follow);bokeh_.frame({time,.5,plan_.dots.stars_opacity,width_,height_});
    for(size_t slot=0;slot<8;++slot)feedback_.bindMap(slot,plan_.warps[slot]?plan_.warps[slot]->map:plan_.warps[0]->map);
    if(plan_.title){const auto& title=*plan_.title;visualizers::TextRasterOptions options;options.family="Arial";options.fontPixels=title.fontPixels;options.supersample=1;const auto mask=visualizers::rasterizeText(embed_,options);text_.uploadMask(mask.image.width,mask.image.height,1,mask.image.rgba);text_.showTimed(title.x,title.y,1,{time,time+.8,.5});titleX_=title.x;titleY_=title.y;}embed_.clear();
    if(text_.hasMask()&&text_.intensity(time)>=1){pendingBurn_=true;planner_.titleBurned();}
    spawns_.clear();for(const auto& star:plan_.stars)spawns_.push_back({star.x,star.y,star.z,star.z2,star.background,star.palettePosition,star.fadeIn,star.hold,star.fadeOut});bokeh_.add(spawns_,feedback_.width(),feedback_.height(),time);
    if(!frozen_){FeedbackFrame frame;frame.strengths=plan_.strengths;frame.shiftX=plan_.shiftX;frame.shiftY=plan_.shiftY;frame.darkening=plan_.darkening;frame.time=time;frame.oversample=supersampling_;frame.shader=plan_.shader;frame.noiseX=plan_.noiseX;frame.noiseY=plan_.noiseY;
        try{const auto& encoder=feedback_.beginFrame(frame);warpDots_.encode(encoder,feedback_,{time,plan_.dots.fading_dots,plan_.pointSize});bokeh_.encode(encoder,feedback_);feedback_.encodeWarp();dots_.dimensions(feedback_.width(),feedback_.height());dots_.upload(plan_.points);dots_.draw(encoder,feedback_.view());warpDots_.draw(encoder,feedback_.view(),dots_);
            if(pendingBurn_){text_.draw(encoder,feedback_.view(),feedback_.width(),feedback_.height(),titleX_,titleY_,1);text_.clear();pendingBurn_=false;}feedback_.submit();
        }catch(...){feedback_.abortFrame();throw;}
    }
    PresentationFrame frame;frame.width=width_;frame.height=height_;frame.oversample=supersampling_;frame.layer1Density=plan_.shader.layer1_density;frame.layer2Brightness=plan_.shader.layer2_brightness;frame.background=plan_.background;frame.headroom=options_.headroom;const auto title=text_.rect(time);if(title.enabled)frame.title=title.title;
    presentation_.prepare(feedback_.texture(),frame,color_.GetFormat(),text_.presenterView());bokeh_.prepareDraw(color_.GetFormat(),plan_.background);auto encoder=context_.device.CreateCommandEncoder();wgpu::RenderPassColorAttachment attachment{};attachment.view=colorView_;attachment.loadOp=wgpu::LoadOp::Clear;attachment.storeOp=wgpu::StoreOp::Store;attachment.clearValue={0,0,0,1};wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&attachment;auto pass=encoder.BeginRenderPass(&descriptor);presentation_.draw(pass);bokeh_.draw(pass);presentation_.drawPalette(pass);pass.End();
    attachment.view=outputView_;auto converted=encoder.BeginRenderPass(&descriptor);converted.SetPipeline(convert_);converted.SetBindGroup(0,conversion_);converted.Draw(3);converted.End();const auto commands=encoder.Finish();context_.queue.Submit(1,&commands);context_.checkErrors();++frames_;
}
}
