// SPDX-License-Identifier: GPL-2.0-or-later
#include "window.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace visualizers::gpu;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){nlohmann::json report;std::filesystem::path output;
    try{
        if(argc!=2)throw std::runtime_error("Expected report filename");output=argv[1];
        NativeWindow window("Native presentation regression",96,96,true);
        Context context(wgpu::BackendType::Undefined,false,[&](const wgpu::Instance& instance){return window.createSurface(instance);});
        report=context.info();window.validatePresentation(report);
        // Inject only display observations, never a different GPU/surface. No
        // production command-line flag can override the OS capability query.
        bool displayHdr=true;unsigned frames=0;
        {
        Presenter presenter(context,window,true,true,[&]{return DisplayPresentation{displayHdr,true,displayHdr?"Fixture HDR":"Fixture SDR"};});
        require(presenter.linearSource(),"This test requires an actual float16 surface");
        // Dawn initializes this texture to black, keeping the synthetic display
        // transitions visually neutral even when the physical display is SDR.
        auto source=context.target(8,8);
        for(bool enabled:{true,false,true}){
            displayHdr=enabled;const auto start=SDL_GetTicks64();bool observed=false;
            while(SDL_GetTicks64()-start<5000){
                SDL_Event event{};while(SDL_PollEvent(&event)){}
                const auto size=window.size();presenter.resize(size[0],size[1]);
                if(presenter.present(source))++frames;
                if(presenter.presentationInfo().at("hdr_active").get<bool>()==enabled){observed=true;break;}
                SDL_Delay(5);
            }
            require(observed,"Presentation did not follow the display change");
            require(presenter.linearSource(),"Display fallback changed the renderer encoding");
            report["transitions"].push_back(presenter.presentationInfo());
        }
        presenter.drain();require(presenter.configurations()>=3,"Display changes did not reconfigure the real surface");
        require(presenter.maximumFramesInFlight()<=2,"Presentation exceeded its submission bound");
        report["surface_configurations"]=presenter.configurations();
        }
        displayHdr=false;
        {
            Presenter presenter(context,window,true,true,[&]{return DisplayPresentation{displayHdr,true,"Fixture SDR"};});
            require(!presenter.linearSource(),"An SDR-started session requested linear plugin output");
            displayHdr=true;const auto size=window.size();presenter.resize(size[0],size[1]);
            require(!presenter.presentationInfo().at("hdr_active").get<bool>()&&!presenter.linearSource(),"An SDR-started session changed its output encoding");
            require(presenter.presentationInfo().at("reason").get<std::string>().find("reopen")!=std::string::npos,"New HDR availability did not explain reopening");
            auto source=context.target(8,8);require(presenter.present(source),"SDR session did not present after a display change");presenter.drain();
            report["sdr_started_session"]=presenter.presentationInfo();
        }
        {
            Presenter presenter(context,window,true,false,[]{return DisplayPresentation{true,true,"Fixture HDR"};});
            require(!presenter.linearSource(),"SDR-only third-party plugin was asked for linear output");
            const auto size=window.size();presenter.resize(size[0],size[1]);
            require(!presenter.presentationInfo().at("hdr_active").get<bool>(),"SDR-only third-party plugin enabled HDR");
            report["sdr_only_plugin"]=presenter.presentationInfo();
        }
#ifdef __APPLE__
        {
            bool enabled=false;
            Presenter presenter(context,window,true,true,[&]{return DisplayPresentation{enabled,true,"Fixture EDR",true,enabled?4.f:1.f,4.f};});
            require(presenter.linearSource(),"Headroom-one EDR session lost linear plugin output");
            auto source=context.target(8,8);
            for(bool next:{false,true,false}){
                enabled=next;const auto start=SDL_GetTicks64();bool observed=false;
                while(SDL_GetTicks64()-start<5000){
                    SDL_Event event{};while(SDL_PollEvent(&event)){}
                    const auto size=window.size();presenter.resize(size[0],size[1]);
                    if(presenter.present(source))++frames;
                    auto info=presenter.presentationInfo();
                    if(info.at("hdr_active")==enabled&&info.at("output_mode")==unsigned(enabled?1:3)){
                        require(info.at("metal_edr_requested")==true&&info.at("metal_surface_float16")==true,"Metal layer did not retain float16 EDR transport");
                        require(info.at("metal_extended_linear_srgb")==true,"Metal layer did not use extended linear sRGB");
                        require(info.at("white_scale")==1.f,"Metal layer used the Windows white scale");
                        observed=true;report["edr_headroom_transitions"].push_back(info);break;
                    }
                    SDL_Delay(5);
                }
                require(observed,"EDR transfer did not update when only headroom changed");
            }
            presenter.drain();require(presenter.configurations()==1,"Headroom-only changes recreated the EDR surface");
            require(presenter.maximumFramesInFlight()<=2,"EDR exceeded the submission bound");
        }
#endif
        report["frames"]=frames;report["result"]="pass";
        report["scope"]="Real float16/SDR surface reconfiguration with injected display states and black frames; physical HDR remains unverified";
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();}
    if(!output.empty()){std::filesystem::create_directories(output.parent_path());std::ofstream(output)<<report.dump(2)<<'\n';}
    std::cout<<report.dump(2)<<'\n';return report.value("result","")=="pass"?0:1;
}
