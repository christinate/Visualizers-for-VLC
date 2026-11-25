// SPDX-License-Identifier: GPL-2.0-or-later
// Uses the renderer's pipeline bank; no separate test-only pipeline definitions.
#include "gpu_support.hpp"
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using namespace prismflow;using json=nlohmann::json;
int main(int argc,char** argv){fs::path output="prismflow-gpu-shaders",assets;std::string current;json report;
    try{
        wgpu::BackendType backend=wgpu::BackendType::Undefined;bool software=false;
        for(int n=1;n<argc;++n){const std::string option=argv[n];
            if(option=="--assets"&&n+1<argc)assets=argv[++n];else if(option=="--output"&&n+1<argc)output=argv[++n];else if(option=="--software")software=true;
            else if(option=="--backend"&&n+1<argc){const std::string name=argv[++n];if(name=="d3d12")backend=wgpu::BackendType::D3D12;else if(name=="metal")backend=wgpu::BackendType::Metal;else if(name=="vulkan")backend=wgpu::BackendType::Vulkan;else throw std::runtime_error("Unknown native backend");}
            else throw std::runtime_error("Unknown shader-probe argument");}
        if(assets.empty())throw std::invalid_argument("--assets is required");fs::create_directories(output);GpuContext context(backend,software);GpuAssets bank(context,assets);
        report=context.info();json results=json::array();unsigned renders=0,computes=0;
        for(const auto& entry:bank.specs()){
            current=entry.name;json result={{"name",current},{"wgsl_bytes",fs::file_size(assets/entry.file)},{"pipelines",json::array()}};
            if(entry.kind=="compute"){
                bank.compute(entry.name);++computes;result["pipelines"].push_back({{"kind",entry.kind},{"entry",entry.entry}});
            }else for(const auto& format:entry.formats){
                bank.render(entry.name,textureFormat(format));++renders;
                result["pipelines"].push_back({{"kind",entry.kind},{"format",format},{"vertex",entry.vertex},{"fragment",entry.fragment}});
            }
            results.push_back(std::move(result));
        }
        context.checkErrors();report["result"]="pass";report["scope"]="Native renderer pipeline-bank compilation; no complete Prism Flow frames rendered";
        report["shader_modules"]=bank.specs().size();report["render_pipelines"]=renders;report["compute_pipelines"]=computes;report["cases"]=std::move(results);
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
