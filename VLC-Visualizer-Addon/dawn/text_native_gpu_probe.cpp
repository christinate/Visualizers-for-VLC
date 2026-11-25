// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include "gpu_text.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QGuiApplication>
#endif
namespace fs=std::filesystem;
using namespace visualizers;
using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

// Independent bilinear sampler, including the transparent area outside the
// quad. It tests orientation and logical supersampling, not font appearance.
static float expected(const RgbaImage& image,uint32_t ss,uint32_t dstW,uint32_t dstH,uint32_t x,uint32_t y){
    const auto u=prismflow::GpuText::uniformData(dstW,dstH,dstW*.5,dstH*.5,1,double(image.width)/ss,double(image.height)/ss);
    const double px=x+.5,py=y+.5;if(px<u[0]||py<u[1]||px>=u[0]+u[2]||py>=u[1]+u[3])return 0;
    const double fx=(px-u[0])/u[2]*image.width-.5,fy=(py-u[1])/u[3]*image.height-.5;
    const int ix=int(std::floor(fx)),iy=int(std::floor(fy));const double tx=fx-ix,ty=fy-iy;
    const auto alpha=[&](int xx,int yy){xx=std::clamp(xx,0,int(image.width)-1);yy=std::clamp(yy,0,int(image.height)-1);return double(image.rgba[(size_t(yy)*image.width+xx)*4+3])/255.;};
    return float((alpha(ix,iy)*(1-tx)+alpha(ix+1,iy)*tx)*(1-ty)+(alpha(ix,iy+1)*(1-tx)+alpha(ix+1,iy+1)*tx)*ty);
}
int main(int argc,char** argv){fs::path assets,output="native-font-gpu";json report;
    try{
#if !defined(_WIN32) && !defined(__APPLE__)
        QGuiApplication ui(argc,argv);ui.setQuitOnLastWindowClosed(false);
#endif
        for(int n=1;n<argc;++n){const std::string option=argv[n];if(option=="--assets"&&n+1<argc)assets=argv[++n];else if(option=="--output"&&n+1<argc)output=argv[++n];else throw std::invalid_argument("Unknown native-font GPU probe argument");}
        require(!assets.empty(),"Expected Prism Flow shader assets");fs::create_directories(output);
        gpu::Context context;prismflow::GpuAssets bank(context,assets);report=context.info();report["cases"]=json::array();
        const std::vector<std::string> corpus={"Prism Flow 64-bit","Prism • 日本語 🎵","مرحبا بالعالم","e\xcc\x81 & café","jgj AMW","                 AMW"};
        uint64_t maskBytes=0,components=0;double maximum=0,total=0;
        // Powers of two give exactly representable quad/texel phases for the
        // CPU oracle. Arbitrary fractional sampling belongs to the existing
        // original-GPU comparison (which also includes 3x supersampling).
        for(uint32_t ss:{1u,2u,4u})for(size_t n=0;n<corpus.size();++n){
            TextRasterOptions options;options.supersample=ss;if(n>=4){options.family="serif";options.weight=400;options.fontPixels=18.25;options.paddingPixels=0;}
            const auto raster=rasterizeText(corpus[n],options);const auto& im=raster.image;prismflow::GpuText text(context,bank);text.uploadMask(im.width,im.height,ss,im.rgba);
            require(prismflow::readByteTexture(context,text.maskTexture())==im.rgba,"Native glyph mask changed during GPU upload");maskBytes+=im.rgba.size();
            const uint32_t width=im.width/ss+24,height=im.height/ss+24;const auto target=context.target(width,height);const auto view=target.CreateView();
            const auto encoder=context.device.CreateCommandEncoder();wgpu::RenderPassColorAttachment color{};color.view=view;color.loadOp=wgpu::LoadOp::Clear;color.storeOp=wgpu::StoreOp::Store;color.clearValue={0,0,0,0};wgpu::RenderPassDescriptor d{};d.colorAttachmentCount=1;d.colorAttachments=&color;auto pass=encoder.BeginRenderPass(&d);pass.End();
            text.showImmediate(width*.5,height*.5,1);const auto rect=text.rect(0);require(rect.enabled&&rect.title.width==double(im.width)/ss&&rect.title.height==double(im.height)/ss,"Native glyph supersampling dimensions differ");
            text.draw(encoder,view,width,height,width*.5,height*.5,1);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
            const auto pixels=prismflow::readHalfTexture(context,target);double error=0,sum=0;uint64_t ink=0,outside=0;json worst;
            for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
                const float ideal=expected(im,ss,width,height,x,y),reference=gpu::decodeHalf(gpu::encodeHalf(ideal));
                // Permit the two enclosing UNORM8 values plus half-float
                // rounding. Lavapipe's filtered RGBA8 masks visibly exhibit
                // that quantization; the GPU/source comparison stays separate.
                const double low=std::floor(double(ideal)*255)/255,high=std::ceil(double(ideal)*255)/255;
                for(unsigned c=0;c<4;++c){const float actual=gpu::decodeHalf(pixels[(size_t(y)*width+x)*4+c]);require(std::isfinite(actual)&&actual>=0&&actual<=1,"Nonfinite/out-of-range native text stamp");const double e=std::abs(double(actual)-reference);if(e>error)worst={{"x",x},{"y",y},{"actual",actual},{"expected",reference}};error=std::max(error,e);sum+=e;ink+=actual>0;outside+=actual<low-.0005||actual>high+.0005;}
            }
            report["cases"].push_back({{"text",corpus[n]},{"ss",ss},{"max_error",error},{"mean_error",sum/pixels.size()}});components+=pixels.size();maximum=std::max(maximum,error);total+=sum;
            report["current_case"]={{"index",n},{"ss",ss},{"max_error",error},{"mean_error",sum/pixels.size()},{"nonzero_components",ink},{"outside_quantization_bound",outside},{"worst",worst},{"mask_width",im.width},{"mask_height",im.height}};
            require(ink>0&&!outside&&sum/pixels.size()<=.0002,"Native glyph stamp differs from the CPU bilinear reference");
        }
        context.checkErrors();report["result"]="pass";report["mask_bytes_exact"]=maskBytes;report["components_compared"]=components;report["maximum_error"]=maximum;report["mean_error"]=total/components;report["limits"]="CPU bilinear alpha's enclosing UNORM8 values +/- 0.0005, mean <= 0.0002; exact GPU mask upload";report["scope"]="Actual native glyph rasterization, exact GPU upload and original text shader versus independent CPU sampling at 1x/2x/4x; full renderer integration remains separate";
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");std::cerr<<summary.dump(2)<<'\n';return 1;}
}
