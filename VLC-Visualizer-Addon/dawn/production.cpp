// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings.hpp"
#include "diagnostic_text.hpp"
#include "capture_service.hpp"
#ifdef _WIN32
#include "catalog.hpp"
#include <shellapi.h>
namespace registry=catalog;
#else
#include "native_catalog.hpp"
namespace registry=native;
#endif
#include <SDL.h>
#include <charconv>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

int runNativeWindow(int,char**);
namespace fs=std::filesystem;
namespace {
fs::path path(const std::string& text){return fs::u8path(text);}
std::string utf8(const fs::path& p){const auto s=p.u8string();return {s.begin(),s.end()};}
fs::path contained(const fs::path& root,const std::string& p){return registry::contained(root,path(p));}
int run(std::vector<std::string> args){
    bool test=std::find(args.begin(),args.end(),"--self-test")!=args.end(),edit=false;fs::path suite,profile=visualizers::defaultSettingsProfile();std::string id;
    uint32_t captureOwner=0;bool captureAutomation=false,generatedCapture=false,captureCancel=false;
    try{
        std::vector<std::string> forwarded;bool output=false;
        for(size_t n=1;n<args.size();++n){const auto& a=args[n];
            auto value=[&]()->const std::string&{if(n+1==args.size())throw std::runtime_error("Missing argument: "+a);return args[++n];};
            if(a=="--suite")suite=fs::absolute(path(value()));
            else if(a=="--visualizer")id=value();
            else if(a=="--settings"){id=value();edit=true;}
            else if(a=="--capture-for"){
                const auto& text=value();const auto parsed=std::from_chars(text.data(),text.data()+text.size(),captureOwner);
                if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!captureOwner||captureOwner>0x7fffffff)throw std::runtime_error("Invalid capture owner process ID");
            }
            else if(a=="--capture-test")captureAutomation=true;
            else if(a=="--capture-test-generated")generatedCapture=true;
            else if(a=="--capture-test-cancel")captureCancel=true;
            else if(a=="--profile")profile=fs::absolute(path(value()));
            else if(a=="--line-in"){
                const auto input=n+1<args.size()&&args[n+1].rfind("--",0)!=0?args[++n]:std::string("default");
                forwarded.insert(forwarded.end(),{"--line-in",input});
            }
            else if(a=="--plugin"||a=="--assets"||a=="--terrain-shader"||a=="--name")throw std::runtime_error("Native plugin paths are selected by the installed manifest");
            else if(a=="--demo")forwarded.push_back("--synthetic");
            else if(a=="--quit-after"){
                const auto& text=value();uint64_t milliseconds=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),milliseconds);
                if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||milliseconds>86400000)throw std::runtime_error("Invalid --quit-after duration");
                forwarded.insert(forwarded.end(),{"--seconds",std::to_string(milliseconds/1000.)});
            }else{
                if(a=="--self-test")test=true;if(a=="--output-dir")output=true;
                forwarded.push_back(a);
                // Preserve a value as data even when it resembles an option.
                if(a=="--output-dir"||a=="--vlc-pid"||a=="--player-pid"||a=="--seconds"||a=="--time-scale"||a=="--close-test"||a=="--render-width"||a=="--render-height"||a=="--drop-image-test")forwarded.push_back(value());
            }
        }
        if(suite.empty()||id.empty())throw std::runtime_error("Provide --suite and a registered visualizer ID");
        const auto cache=registry::read(suite/"installed-visualizers.json");
        if(cache.at("schema_version")!=1||!cache.at("visualizers").is_array()||cache.at("visualizers").size()>256)throw std::runtime_error("Invalid installed visualizer catalog");
        nlohmann::json cached;for(const auto& entry:cache.at("visualizers"))if(entry.at("id")==id){cached=entry;break;}
        if(cached.is_null())throw std::runtime_error("No registered visualizer matches this ID; scan after installation");
        const auto package=contained(suite,cached.at("package").get<std::string>());
        // Menu/cache entries are hints. Recheck the current files in this
        // process before loading a renderer, including after frontend dispatch.
        const auto fresh=registry::validate(suite,package);
        if(fresh.at("id")!=id)throw std::runtime_error("Package changed since scanning");
        if(captureOwner){
            if(edit)throw std::runtime_error("Capture helper cannot edit settings");
            visualizers::CaptureServiceOptions options{suite,profile,{},id,{},captureOwner,test,captureAutomation,generatedCapture};
            options.cancelPicker=captureCancel;
            for(size_t n=0;n<forwarded.size();++n){const auto& a=forwarded[n];
                if(a=="--line-in")options.lineInput=forwarded.at(++n);
                else if(a=="--output-dir")options.output=fs::absolute(path(forwarded.at(++n)));
                else if(a=="--seconds")options.seconds=std::stod(forwarded.at(++n));
                else if(a!="--self-test"&&a!="--background")throw std::runtime_error("Unsupported capture-helper option: "+a);
            }
            return visualizers::runCaptureService(options);
        }
        if(captureAutomation||generatedCapture||captureCancel)throw std::runtime_error("Capture test flags require --capture-for");
        if(!edit&&fresh.at("adapter")!="dawn")throw std::runtime_error("This package requires its original engine host");
        if(!output)forwarded.insert(forwarded.end(),{"--output-dir",utf8(profile/("native-"+id))});
        forwarded.insert(forwarded.end(),{"--suite",utf8(suite),"--profile",utf8(profile)});
        if(edit){
            forwarded.insert(forwarded.end(),{"--settings",id,"--name",fresh.at("name").get<std::string>()});
            if(visualizers::hasSceneIni(id)&&fresh.contains("assets"))
                forwarded.insert(forwarded.end(),{"--assets",utf8(contained(package,fresh.at("assets").get<std::string>()))});
        }
        else{
            const auto module=contained(package,fresh.at("entrypoint").get<std::string>());
            const auto assets=contained(package,fresh.at("assets").get<std::string>());
            const auto capabilities=fresh.value("settings",nlohmann::json::object());
            if(!capabilities.is_object()||capabilities.value("hdr_output",nlohmann::json())!="native-float")forwarded.push_back("--sdr-only");
            forwarded.insert(forwarded.end(),{"--visualizer",id,"--name",fresh.at("name").get<std::string>(),"--plugin",utf8(module),"--assets",utf8(assets)});
        }
        forwarded.insert(forwarded.begin(),args.front());std::vector<char*> argv;
        for(auto& text:forwarded)argv.push_back(text.data());argv.push_back(nullptr);
        return runNativeWindow(int(forwarded.size()),argv.data());
    }catch(const std::exception& error){
        const auto diagnostic=visualizers::boundedDiagnostic(error.what());
        std::cerr<<diagnostic<<'\n';if(!test)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Visualizer",diagnostic.c_str(),nullptr);return 1;
    }
}
}
#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc=0;auto wide=CommandLineToArgvW(GetCommandLineW(),&argc);if(!wide)return 1;
    std::vector<std::string> args;try{for(int n=0;n<argc;++n)args.push_back(catalog::utf8(wide[n]));}catch(...){LocalFree(wide);return 1;}
    LocalFree(wide);return run(std::move(args));
}
#else
int main(int argc,char** argv){return run({argv,argv+argc});}
#endif
