// SPDX-License-Identifier: GPL-2.0-or-later
#include "window.hpp"
#include "music_analysis.hpp"
#include "plugin.hpp"
#include "audio_capture.hpp"
#include "player_audio.hpp"
#include "input_picker.hpp"
#include "settings_ui.hpp"
#include "diagnostic_text.hpp"
#include "window_control.hpp"
#include "shader_cache.hpp"
#include "text_raster.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <optional>
#include <charconv>
#include <limits>
#include <memory>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif

namespace fs=std::filesystem;
using namespace visualizers;
using namespace visualizers::gpu;
using json=nlohmann::json;

namespace {
class BeachProgress {
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window_{nullptr,SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer_{nullptr,SDL_DestroyRenderer};
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> title_{nullptr,SDL_DestroyTexture},note_{nullptr,SDL_DestroyTexture};
    std::function<bool()> closing_;
    uint64_t next_=0;
    unsigned updates_=0;
    bool cancelled_=false;
    bool preparing_=false;
    SDL_Texture* label(std::string_view text,double size,unsigned weight){
        TextRasterOptions options;options.fontPixels=size;options.weight=weight;options.supersample=1;
        const auto image=rasterizeText(text,options).image;
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(
            SDL_CreateTexture(renderer_.get(),SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
                int(image.width),int(image.height)),SDL_DestroyTexture);
        if(!texture||SDL_UpdateTexture(texture.get(),nullptr,image.rgba.data(),int(image.width*4))!=0)
            throw std::runtime_error("Could not draw the Beach startup label");
        SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_BLEND);
        return texture.release();
    }
    void draw(SDL_Texture* texture,int x,int y){
        SDL_Rect bounds{x,y,0,0};
        SDL_QueryTexture(texture,nullptr,nullptr,&bounds.w,&bounds.h);
        SDL_RenderCopy(renderer_.get(),texture,nullptr,&bounds);
    }
public:
    explicit BeachProgress(std::function<bool()> closing):closing_(std::move(closing)){
        window_.reset(SDL_CreateWindow("Beach shader preparation",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            432,150,SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI));
        if(window_)renderer_.reset(SDL_CreateRenderer(window_.get(),-1,SDL_RENDERER_SOFTWARE));
        if(!renderer_)throw std::runtime_error("Could not open Beach shader progress");
        SDL_RenderSetLogicalSize(renderer_.get(),432,150);
        title_.reset(label("Precompiling Beach shaders",20,700));
        note_.reset(label("This only happens the first time you run Beach.",14,400));
        SDL_SetTextureColorMod(note_.get(),187,208,222);
    }
    bool tick(){
        cancelled_=cancelled_||closing_();
        SDL_Event event{};
        while(SDL_PollEvent(&event)){
            const auto id=SDL_GetWindowID(window_.get());
            if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.windowID==id&&event.window.event==SDL_WINDOWEVENT_CLOSE)||
               (event.type==SDL_KEYDOWN&&event.key.windowID==id&&event.key.keysym.sym==SDLK_ESCAPE))cancelled_=true;
        }
        if(cancelled_)return false;
        const auto now=SDL_GetTicks64();
        if(now<next_)return true;
        next_=now+30;
        auto* r=renderer_.get();
        SDL_SetRenderDrawColor(r,15,33,46,255);SDL_RenderClear(r);
        draw(title_.get(),27,24);
        const SDL_Rect track{31,74,370,10};
        SDL_SetRenderDrawColor(r,44,67,82,255);SDL_RenderFillRect(r,&track);
        const int x=int((now/6)%493)-84;
        const SDL_Rect pulse{track.x+std::max(0,x),74,std::max(0,std::min(370,x+84)-std::max(0,x)),10};
        SDL_SetRenderDrawColor(r,79,213,200,255);
        if(preparing_){const SDL_Rect advanced{31,74,278,10};SDL_RenderFillRect(r,&advanced);}
        else SDL_RenderFillRect(r,&pulse);
        draw(note_.get(),27,97);
        SDL_RenderPresent(r);
        ++updates_;
        return true;
    }
    void preparing(){preparing_=true;next_=0;}
    bool cancelled()const{return cancelled_;}
    unsigned updates()const{return updates_;}
};
uint32_t unsignedArgument(const std::string& text){uint32_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())throw std::runtime_error("Invalid unsigned argument: "+text);return value;}
void exerciseExternalResize(NativeWindow& window,int width,int height){
#ifdef _WIN32
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window.handle(),&info)!=SDL_TRUE)throw std::runtime_error(SDL_GetError());
    if(!SetWindowPos(info.info.win.window,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))
        throw std::runtime_error("Cannot resize the visualizer window from the OS");
#else
    window.resize(width,height);
#endif
}
void mouse(SDL_Window* window,Uint8 button,Uint8 clicks){
    SDL_Event event{};event.type=button==SDL_BUTTON_RIGHT?SDL_MOUSEBUTTONUP:SDL_MOUSEBUTTONDOWN;
    event.button.windowID=SDL_GetWindowID(window);event.button.button=button;event.button.clicks=clicks;
    if(SDL_PushEvent(&event)<0)throw std::runtime_error(SDL_GetError());
}
void escape(SDL_Window* window){
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window);event.key.keysym.sym=SDLK_ESCAPE;
    if(SDL_PushEvent(&event)<0)throw std::runtime_error(SDL_GetError());
}
void dropImage(SDL_Window* window,const fs::path& file){
    const auto encoded=file.u8string();const std::string text(encoded.begin(),encoded.end());SDL_Event event{};event.type=SDL_DROPFILE;event.drop.windowID=SDL_GetWindowID(window);event.drop.file=SDL_strdup(text.c_str());
    if(!event.drop.file)throw std::bad_alloc();if(SDL_PushEvent(&event)<=0){SDL_free(event.drop.file);throw std::runtime_error("Could not queue native image drop");}
}
}
int runNativeWindow(int argc,char** argv){
    fs::path output="dawn-window",shaderPath,pluginPath,profilePath,controlSuite;bool test=false,exercise=false,startupFullscreen=false,background=false,software=false;
    uint32_t playerPid=0;bool playerTest=false,linearSupported=true;
    std::string settingsId,visualizerId="terrain",displayName;fs::path assetsPath,dropImageTest;bool widthLimit=false,heightLimit=false,effectKeyTest=false,stress=false;
    std::string lineInput;bool selectInput=false,synthetic=false,reselectionTest=false;
    unsigned renderWidth=640,renderHeight=360;
    double seconds=0,timeScale=1;std::string closeTest="escape";
    json report;
    try{
        for(int n=1;n<argc;++n){std::string a=argv[n];
            if(a=="--output-dir"&&n+1<argc)output=argv[++n];
            else if(a=="--terrain-shader"&&n+1<argc)shaderPath=argv[++n];
            else if(a=="--plugin"&&n+1<argc)pluginPath=argv[++n];
            else if(a=="--visualizer"&&n+1<argc)visualizerId=argv[++n];
            else if(a=="--name"&&n+1<argc)displayName=argv[++n];
            else if(a=="--assets"&&n+1<argc)assetsPath=argv[++n];
            else if(a=="--profile"&&n+1<argc){const std::string value=argv[++n];profilePath=fs::path(std::u8string(value.begin(),value.end()));}
            else if(a=="--settings"&&n+1<argc)settingsId=argv[++n];
            else if((a=="--vlc-pid"||a=="--player-pid")&&n+1<argc){playerPid=unsignedArgument(argv[++n]);if(!playerPid)throw std::runtime_error("Invalid player process ID");}
            else if(a=="--suite"&&n+1<argc)controlSuite=suite::controlPathFromUtf8(argv[++n]);
            else if(a=="--player-test")playerTest=true;
            else if(a=="--line-in")lineInput=n+1<argc&&std::string(argv[n+1]).rfind("--",0)!=0?argv[++n]:"default";
            else if(a=="--select-input")selectInput=true;
            else if(a=="--synthetic")synthetic=true;
            else if(a=="--reselection-test")reselectionTest=true;
            else if(a=="--effect-key-test")effectKeyTest=true;
            else if(a=="--drop-image-test"&&n+1<argc){const std::string value=argv[++n];dropImageTest=fs::absolute(fs::path(std::u8string(value.begin(),value.end())));}
            else if(a=="--seconds"&&n+1<argc)seconds=std::stod(argv[++n]);
            else if(a=="--time-scale"&&n+1<argc)timeScale=std::stod(argv[++n]);
            else if(a=="--close-test"&&n+1<argc)closeTest=argv[++n];
            else if(a=="--self-test")test=true;
            else if(a=="--exercise")exercise=true;
            else if(a=="--startup-fullscreen")startupFullscreen=true;
            else if(a=="--background")background=true;
            else if(a=="--stress")stress=true;
            else if(a=="--software")software=true;
            else if(a=="--sdr-only")linearSupported=false;
            else if(a=="--render-width"&&n+1<argc){renderWidth=unsignedArgument(argv[++n]);widthLimit=true;}
            else if(a=="--render-height"&&n+1<argc){renderHeight=unsignedArgument(argv[++n]);heightLimit=true;}
            else throw std::runtime_error("Unknown argument: "+a);
        }
        fs::create_directories(output);
        if(profilePath.empty())profilePath=test?fs::absolute(output)/"profile":defaultSettingsProfile();
        if(!settingsId.empty()){
            NativeUiSession ui;SettingsStore store(profilePath,settingsId);auto values=store.load().values;auto pending=SettingsJson::object();std::string notice;
            if(!assetsPath.empty()&&hasSceneIni(settingsId))values["sceneIni"]=store.sceneEditor(assetsPath);
            const auto& name=displayName.empty()?settingsId:displayName;
            SettingsAutomation automated;bool saved=false;
            for(;;){const auto edited=editNativeSettings(values,settingsId,name,[](){return true;},test?&automated:nullptr,notice);if(!edited)break;
                for(auto it=edited->begin();it!=edited->end();++it){pending[it.key()]=it.value();values[it.key()]=it.value();}
                try{store.savePatch(pending);saved=true;break;}catch(const std::exception& error){if(test)throw;notice=boundedDiagnostic(error.what());}
            }
            report={{"result","pass"},{"settings_saved",saved},{"visualizer",settingsId},{"native_settings",true},
                {"scene_ini_editor",values.contains("sceneIni")}};
            std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
        }
        if(pluginPath.empty())throw std::runtime_error("This visualizer requires a native plugin module");
        SettingsStore preferences(profilePath,visualizerId);const auto savedPreferences=preferences.load().values;
        const int windowWidth=savedPreferences.at("width"),windowHeight=savedPreferences.at("height"),frameLimit=savedPreferences.at("fps");
        const double cameraSeconds=savedPreferences.at("cameraSeconds");const unsigned colorBeats=savedPreferences.at("colorBeats");
        startupFullscreen=startupFullscreen||savedPreferences.at("fullscreen").get<bool>();
        if(!widthLimit)renderWidth=unsigned(windowWidth);if(!heightLimit)renderHeight=unsigned(windowHeight);
        if(assetsPath.empty()&&!shaderPath.empty())assetsPath=fs::absolute(shaderPath).parent_path();
        if(!pluginPath.empty()&&(assetsPath.empty()||!fs::is_directory(assetsPath)))throw std::runtime_error("Provide the native plugin assets directory with --assets");
        if(!std::isfinite(seconds)||seconds<0||seconds>86400||!std::isfinite(timeScale)||timeScale<=0||timeScale>1000)
            throw std::runtime_error("Invalid duration or time scale");
        if(closeTest!="escape"&&closeTest!="rightclick"&&closeTest!="menu"&&closeTest!="device-loss"&&closeTest!="player-exit")throw std::runtime_error("Invalid closure test");
        if(renderWidth<64||renderHeight<64||renderWidth>4096||renderHeight>4096)throw std::runtime_error("Invalid render dimensions");
        if(reselectionTest&&!test)throw std::runtime_error("Reselection validation requires --self-test");
        if(effectKeyTest&&(!test||pluginPath.empty()||visualizerId!="geiss"))throw std::runtime_error("Effect-key validation requires a Prism Flow module and --self-test");
        if(!dropImageTest.empty()&&(!test||pluginPath.empty()||visualizerId!="geiss"))throw std::runtime_error("Image-drop validation requires a Prism Flow module and --self-test");
        if(!dropImageTest.empty()&&!seconds)seconds=20;
        if(playerTest&&(!test||!playerPid))throw std::runtime_error("Player validation requires --self-test and a player process ID");
        if(closeTest=="player-exit"&&!playerPid)throw std::runtime_error("Player-exit validation requires a player process ID");
        if(playerPid&&(!lineInput.empty()||selectInput||synthetic||reselectionTest))throw std::runtime_error("Player audio cannot be combined with standalone input selection or generated audio");
        if(synthetic&&(!lineInput.empty()||selectInput))throw std::runtime_error("Choose either synthetic or recording input");
        std::unique_ptr<PlayerLifetime> playerLifetime;std::unique_ptr<PlayerAudio> playerAudio;
        if(playerPid){playerLifetime=std::make_unique<PlayerLifetime>(playerPid);if(playerLifetime->exited())throw std::runtime_error("Media player has already exited");playerAudio=std::make_unique<PlayerAudio>(playerPid);}
        {
            NativeUiSession ui;
            if(displayName.empty())displayName=visualizerId=="geiss"?"Prism Flow":"Rhythm Terrain";
            NativeWindow window(displayName.c_str(),windowWidth,windowHeight,background);
            SDL_EventState(SDL_DROPFILE,SDL_ENABLE);SDL_EventState(SDL_DROPTEXT,SDL_ENABLE);
            std::optional<ShaderCache> shaderCache;
            if(visualizerId=="beach")shaderCache.emplace(profilePath,pluginPath,assetsPath/"beach.wgsl");
            Context context(wgpu::BackendType::Undefined,software,[&](const wgpu::Instance& instance){return window.createSurface(instance);},
                            shaderCache?&*shaderCache:nullptr);
            report=context.info();report["startup_fullscreen"]=startupFullscreen;
            report["visualizer"]=visualizerId;
            report["settings"]={{"width",windowWidth},{"height",windowHeight},{"fps",frameLimit},{"cameraSeconds",cameraSeconds},{"colorBeats",colorBeats},{"hdr_requested",savedPreferences.at("hdr")},{"hdr_active",false}};
            const auto initialWindowedBounds=window.bounds();report["initial_windowed_bounds"]=initialWindowedBounds;
            report["borderless"]=(SDL_GetWindowFlags(window.handle())&SDL_WINDOW_BORDERLESS)!=0;
            window.validatePresentation(report);
            SDL_version sdl{};SDL_GetVersion(&sdl);report["sdl"]={sdl.major,sdl.minor,sdl.patch};
            const auto registeredSuite=controlSuite.empty()?fs::absolute(output):fs::absolute(controlSuite);
            suite::WindowControl control(registeredSuite,visualizerId,playerPid);
            Presenter presenter(context,window,savedPreferences.at("hdr").get<bool>(),linearSupported,{},savedPreferences.at("vsync").get<bool>());PluginLibrary library(pluginPath,visualizerId);
            report["presentation"]=presenter.presentationInfo();report["settings"]["hdr_active"]=report["presentation"]["hdr_active"];
            auto scenePreferences=savedPreferences;
            if(hasSceneIni(visualizerId))scenePreferences["sceneIni"]=preferences.sceneOverrides();
            if(visualizerId=="objects")scenePreferences["objectStartIndex"]=preferences.advanceObjectsStart();
            const bool cold=shaderCache&&!shaderCache->ready();
            const auto setupStart=SDL_GetTicks64();
            std::unique_ptr<BeachProgress> progress;
            if(cold){
                progress=std::make_unique<BeachProgress>([&]{
                    return control.closeRequested()||(playerLifetime&&playerLifetime->exited());
                });
                SDL_HideWindow(window.handle());
                progress->tick();
            }
            const StartupPulse pulse{progress?+[](void* value)->int32_t{
                return static_cast<BeachProgress*>(value)->tick()?1:0;
            }:nullptr,progress.get()};
            std::unique_ptr<PluginInstance> plugin;
            try{
                plugin=library.create(context,renderWidth,renderHeight,fs::absolute(assetsPath),scenePreferences.dump(),
                                      presenter.linearSource()?VIS_NATIVE_LINEAR_OUTPUT:0,pulse);
                if(progress){
                    progress->preparing();
                    context.setStartupPulse(pulse);
                    context.pulse();
                    plugin->render(MusicFrame{},0,0);
                    auto done=std::make_shared<wgpu::QueueWorkDoneStatus>(wgpu::QueueWorkDoneStatus::Error);
                    context.wait(context.queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,
                        [done](wgpu::QueueWorkDoneStatus status,wgpu::StringView){*done=status;}),"First Beach frame");
                    context.setStartupPulse({});
                    if(*done!=wgpu::QueueWorkDoneStatus::Success)throw std::runtime_error("The first Beach frame could not be prepared");
                }
                if(shaderCache)shaderCache->complete();
            }catch(...){
                context.setStartupPulse({});
                if(!progress||!progress->cancelled())throw;
                report["result"]="pass";report["closed_by"]="shader-startup-cancelled";
                std::ofstream(output/"result.json")<<report.dump(2)<<'\n';
                return 0;
            }
            if(shaderCache){
                report["beach_shader_startup"]={{"progress_shown",bool(progress)},
                    {"progress_updates",progress?progress->updates():0},
                    {"cache_hits",shaderCache->hits()},{"cache_writes",shaderCache->writes()},
                    {"milliseconds",SDL_GetTicks64()-setupStart}};
            }
            if(progress){progress.reset();SDL_ShowWindow(window.handle());}
            report["native_plugin_api"]=VIS_NATIVE_API_VERSION;
            auto rendererKey=[&](SDL_Keycode key,uint16_t modifiers){
                // Lock states are not accelerator modifiers. Num Lock/Caps
                // Lock must not disable the visualizer's ordinary controls.
                const auto mask=modifiers&(KMOD_SHIFT|KMOD_CTRL|KMOD_ALT|KMOD_GUI);
                plugin->key(uint32_t(key),mask);if(effectKeyTest)report["renderer_key_events"].push_back({{"key",key},{"modifiers",mask}});
            };
            uint64_t imageToken=0,imageRequests=0,imageFailures=0,lastImageToken=0;uint32_t lastImageState=VIS_NATIVE_COMMAND_IDLE;unsigned dropStage=0;std::optional<std::string> imageNotification;
            auto imageNotice=[&](const std::string& message){
                ++imageFailures;report["last_image_error"]=message.substr(0,4095);std::cerr<<message<<'\n';
                if(!test)imageNotification=message.substr(0,4095);
            };
            auto receiveDrop=[&](const SDL_Event& event){
                std::unique_ptr<char,decltype(&SDL_free)> text(event.drop.file,&SDL_free);
                if(event.type!=SDL_DROPFILE||event.drop.windowID!=SDL_GetWindowID(window.handle()))return;
                try{
                    if(!text)throw std::runtime_error("The dropped file has no filename");size_t bytes=0;while(bytes<=32768&&text.get()[bytes])++bytes;
                    if(bytes>32768)throw std::runtime_error("The image filename is too long");if(imageToken==std::numeric_limits<uint64_t>::max())throw std::runtime_error("Image request counter exhausted");
                    if(!plugin||!plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,std::string_view(text.get(),bytes),++imageToken))throw std::runtime_error("This visualizer does not support image palettes");++imageRequests;
                }catch(const std::exception& error){imageNotice(boundedDiagnostic(error.what()));}
            };
            // Capture stops/joins before renderer and GPU destruction.
            std::unique_ptr<AudioCapture> capture,pendingCapture;
            std::string activeInput,pendingInput;unsigned pickerOpens=0,inputChanges=0;
            const unsigned expectedInputChanges=lineInput.empty()?4:5;
            uint64_t frames=0,inputChangeFrame=0,playerPcmWindows=0;float playerPcmPeak=0;unsigned blockedInputRequests=0;bool playerKeysSent=false,readyWritten=false;
            auto rememberInput=[&](const std::string& identifier){try{preferences.savePatch({{"inputDevice",identifier}});}catch(const std::exception& error){report["settings_warning"]=boundedDiagnostic(error.what());}};
            auto prepareCapture=[&](const std::string& token){
                auto replacement=std::make_unique<AudioCapture>();const auto canonical=replacement->resolveInput(token);
                requestCapturePermission();const auto permission=capturePermission();
                if(permission==CapturePermission::Granted){replacement->start(canonical);capture=std::move(replacement);activeInput=canonical;pendingCapture.reset();pendingInput.clear();++inputChanges;inputChangeFrame=frames;rememberInput(canonical);}
                else if(permission==CapturePermission::Pending){pendingCapture=std::move(replacement);pendingInput=canonical;std::cerr<<capturePermissionMessage(permission)<<'\n';}
                else throw std::runtime_error(capturePermissionMessage(permission));
                report["source"]="native recording input";
            };
            report["source"]=playerPid?"media player PCM":"synthetic";report["owner_pid"]=playerPid;
            MusicAnalysis analysis;std::array<float,4096> pcm{};
            auto readCapture=[&]{
                capture->window(pcm);
                if(!capture->status().running)throw std::runtime_error("Recording device stopped or disconnected");
            };
            if(startupFullscreen)window.toggleFullscreen();
            const auto started=SDL_GetTicks64();double next=double(started);const double frameInterval=1000./frameLimit;
            unsigned stage=0,toggles=0,cameraMask=0;bool running=true,testSent=false,restored=false;
            const std::array<SDL_Keycode,8> effectKeys={SDLK_w,SDLK_p,SDLK_d,SDLK_s,SDLK_m,SDLK_l,SDLK_BACKSPACE,SDLK_SPACE};size_t effectSent=0;
            bool startupExitSent=false,startupBoundsChecked=false,startupBoundsRestored=false;
            std::array<int,4> restoreBounds{};std::string reason="duration";
            auto keepPickerOpen=[&](){
                if(playerLifetime&&playerLifetime->exited()){reason="player-exit";running=false;return false;}
                if(control.closeRequested()){reason="menu";running=false;return false;}
                SDL_Event event{};while(SDL_PollEvent(&event)){
                    if(event.type==SDL_DROPFILE||event.type==SDL_DROPTEXT){receiveDrop(event);continue;}
                    if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.windowID==SDL_GetWindowID(window.handle())&&event.window.event==SDL_WINDOWEVENT_CLOSE)){
                        running=false;reason="window-close";
                    }else if(event.type==SDL_KEYDOWN&&event.key.windowID==SDL_GetWindowID(window.handle())&&event.key.keysym.sym==SDLK_ESCAPE){running=false;reason="escape";}
                    else if(event.type==SDL_MOUSEBUTTONUP&&event.button.windowID==SDL_GetWindowID(window.handle())&&event.button.button==SDL_BUTTON_RIGHT){running=false;reason="rightclick";}
                }
                // The modal picker pauses rendering. Keep consuming PCM so
                // the current device stays live if the user cancels.
                if(running&&capture)readCapture();
                context.checkErrors();presenter.ready();return running;
            };
            auto chooseInput=[&](std::string notice=""){
                if(playerPid){if(!blockedInputRequests)std::cerr<<"Audio is supplied by the media player; open a standalone instance to choose recording input\n";++blockedInputRequests;return;}
                const bool restoreFullscreen=window.fullscreen();if(restoreFullscreen)window.toggleFullscreen();
                for(;;){
                    AudioCapture listing;const auto devices=listing.devices();PickerAutomation automatic;
                    if(reselectionTest){
                        automatic.row=0;
                        // Hold the dialog past the FIFO's capacity while the old input runs.
                        if(capture)automatic.pollsBeforeAction=30;
                    }
                    ++pickerOpens;
                    const auto current=pendingCapture?pendingInput:activeInput.empty()?savedPreferences.at("inputDevice").get<std::string>():activeInput;
                    const auto droppedBefore=reselectionTest&&capture?capture->status().dropped:0;
                    const auto choice=pickAudioInput(devices,current,keepPickerOpen,reselectionTest?&automatic:nullptr,notice);
                    if(reselectionTest&&capture&&capture->status().dropped!=droppedBefore)
                        throw std::runtime_error("Recording lost PCM while the input picker was open");
                    if(!choice)break;
                    try{prepareCapture(*choice);break;}
                    catch(const std::exception& error){if(reselectionTest)throw;notice=boundedDiagnostic(error.what());}
                }
                if(restoreFullscreen&&running)window.toggleFullscreen();
            };
            if(!lineInput.empty())prepareCapture(lineInput);
            if(selectInput||(!playerPid&&lineInput.empty()&&!test&&!synthetic)){
                chooseInput();if(running&&!capture&&!pendingCapture){running=false;reason="input-cancelled";}
            }
            while(running){
                if(playerLifetime&&playerLifetime->exited()){reason="player-exit";break;}
                if(control.closeRequested()){reason="menu";break;}
                SDL_Event event{};
                while(SDL_PollEvent(&event)){
                    if(event.type==SDL_DROPFILE||event.type==SDL_DROPTEXT){receiveDrop(event);continue;}
                    if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE)){running=false;reason="window-close";}
                    else if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_RIGHT){running=false;reason="rightclick";}
                    else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT&&event.button.clicks>=2&&event.button.clicks%2==0){window.toggleFullscreen();++toggles;}
                    else if(event.type==SDL_KEYDOWN&&event.key.windowID==SDL_GetWindowID(window.handle())){
                        if(event.key.keysym.sym==SDLK_ESCAPE){running=false;reason="escape";}
                        else if(event.key.keysym.sym==SDLK_F11||(event.key.keysym.sym==SDLK_RETURN&&(event.key.keysym.mod&KMOD_ALT))){window.toggleFullscreen();++toggles;}
                        else if(event.key.keysym.sym==SDLK_SPACE&&!event.key.repeat)rendererKey(SDLK_SPACE,event.key.keysym.mod);
                        else if(event.key.keysym.sym==SDLK_l&&(event.key.keysym.mod&(KMOD_CTRL|KMOD_GUI))&&!event.key.repeat)chooseInput();
                        else if(plugin&&!event.key.repeat)rendererKey(event.key.keysym.sym,event.key.keysym.mod);
                    }
                }
                if(!running)break;
                // Show recoverable notices from the outer event loop, never
                // recursively inside an audio/settings dialog's event pump.
                if(imageNotification){auto message=std::move(*imageNotification);imageNotification.reset();const bool restore=window.fullscreen();if(restore)window.toggleFullscreen();SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Cannot use image",message.c_str(),window.handle());if(restore)window.toggleFullscreen();}
                const auto now=SDL_GetTicks64();const double time=double(now-started)/1000;
                if(reselectionTest&&frames>=10&&pickerOpens<4&&!pendingCapture){
                    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window.handle());event.key.keysym.sym=SDLK_l;
                    event.key.keysym.mod=pickerOpens%2?KMOD_GUI:KMOD_CTRL;SDL_PushEvent(&event);
                }
                if(playerTest&&frames>=10&&!playerKeysSent){SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window.handle());event.key.keysym.sym=SDLK_l;event.key.keysym.mod=KMOD_CTRL;SDL_PushEvent(&event);playerKeysSent=true;}
                if(seconds&&time>=seconds)break;
                if(!dropImageTest.empty()&&dropStage==0&&frames>=10){dropImage(window.handle(),dropImageTest);dropStage=1;}
                if(effectKeyTest&&effectSent<effectKeys.size()&&frames>=(effectSent+1)*10){
                    SDL_Event key{};key.type=SDL_KEYDOWN;key.key.windowID=SDL_GetWindowID(window.handle());key.key.keysym.sym=effectKeys[effectSent++];key.key.keysym.mod=KMOD_NUM;SDL_PushEvent(&key);
                }
                if(exercise){
                    if(startupFullscreen&&!startupExitSent&&frames>=5){mouse(window.handle(),SDL_BUTTON_LEFT,2);startupExitSent=true;}
                    if(startupFullscreen&&startupExitSent&&!startupBoundsChecked&&frames>=10){startupBoundsRestored=window.bounds()==initialWindowedBounds;startupBoundsChecked=true;}
                    const unsigned offset=startupFullscreen?10:0;
                    if(stage==0&&frames>=10+offset){window.resize(320,240);++stage;}
                    else if(stage==1&&frames>=20+offset){exerciseExternalResize(window,800,450);++stage;}
                    else if(stage==2&&frames>=30+offset){
                        restoreBounds=window.bounds();
#ifdef _WIN32
                        if(restoreBounds[2]!=800||restoreBounds[3]!=450)
                            throw std::runtime_error("External Windows resize did not reach the requested bounds");
#endif
                        mouse(window.handle(),SDL_BUTTON_LEFT,2);++stage;
                    }
                    else if(stage==3&&frames>=40+offset){mouse(window.handle(),SDL_BUTTON_LEFT,2);++stage;}
                    else if(stage==4&&frames>=50+offset){restored=window.bounds()==restoreBounds;++stage;}
                }
                if(test&&closeTest!="player-exit"&&!testSent&&frames>=90&&(dropImageTest.empty()||dropStage==4)&&(!reselectionTest||(pickerOpens==4&&inputChanges==expectedInputChanges&&frames>=inputChangeFrame+30))){
                    testSent=true;
                    if(closeTest=="escape")escape(window.handle());
                    else if(closeTest=="rightclick")mouse(window.handle(),SDL_BUTTON_RIGHT,1);
                    else if(closeTest=="menu"){
                        if(suite::closeWindows(registeredSuite,visualizerId,playerPid)!=1)throw std::runtime_error("Native menu close did not match exactly one window");
                    }else context.device.ForceLoss(wgpu::DeviceLostReason::Unknown,"Native lifecycle test requested device loss");
                    continue;
                }
                context.checkErrors();
                if(double(now)<next){SDL_Delay(unsigned(std::max(1.,std::min(next-double(now),4.))));continue;}
                const auto size=window.size();presenter.resize(size[0],size[1]);
                if(!size[0]||!size[1]||!presenter.ready()){SDL_Delay(2);continue;}
                if(exercise){
                    auto& transitions=report["presentation_transitions"];
                    const auto info=presenter.presentationInfo();
                    const json display={{"fullscreen",info.at("fullscreen")},{"vsync_active",info.at("vsync_active")},
                                        {"present_mode",info.at("present_mode")}};
                    if(transitions.is_null())transitions=json::array();
                    if(transitions.empty()||transitions.back()!=display)transitions.push_back(display);
                }
                next+=(std::floor((double(now)-next)/frameInterval)+1)*frameInterval;
                const double factor=std::min({1.,double(renderWidth)/size[0],double(renderHeight)/size[1]});
                const auto width=std::max(1u,unsigned(std::lround(size[0]*factor))),height=std::max(1u,unsigned(std::lround(size[1]*factor)));
                plugin->resize(width,height);
                if(pendingCapture){
                    const auto permission=capturePermission();
                    if(permission==CapturePermission::Granted){pendingCapture->start(pendingInput);capture=std::move(pendingCapture);activeInput=pendingInput;pendingInput.clear();++inputChanges;inputChangeFrame=frames;rememberInput(activeInput);}
                    else if(permission!=CapturePermission::Pending)
                        throw std::runtime_error(capturePermissionMessage(permission));
                }
                BeatState playerBeat;
                if(playerAudio){if(playerAudio->window(pcm,&playerBeat))++playerPcmWindows;for(auto value:pcm)playerPcmPeak=std::max(playerPcmPeak,std::abs(value));}
                else if(capture)readCapture();
                else if(pendingCapture)pcm.fill(0);
                else for(unsigned n=0;n<2048;++n){
                    const double t=time+n/44100.,pulse=std::exp(-std::fmod(t,.5)*20),amplitude=.08+.7*pulse;
                    pcm[n*2]=float(amplitude*std::sin(t*std::numbers::pi*2*110));
                    pcm[n*2+1]=float(amplitude*std::sin(t*std::numbers::pi*2*165));
                }
                const auto& audio=analysis.update(pcm,time,playerAudio?&playerBeat:nullptr);unsigned camera=0;
                // Accelerated lifecycle fixtures cover every camera phase even
                // when a compositor pauses during startup/fullscreen changes.
                // Interactive animation and recording still use monotonic time.
                const double renderTime=(test?double(frames)/frameLimit:time)*timeScale;
                plugin->render(audio,renderTime,1./frameLimit);camera=unsigned(std::fmod(std::floor(renderTime/cameraSeconds),4.));
                if(plugin)if(const auto status=plugin->commandStatus()){
                    if(status->token&&(status->token!=lastImageToken||status->state!=lastImageState)){
                        auto& history=report["image_import_status"];if(history.is_null())history=json::array();if(history.size()==32)history.erase(history.begin());history.push_back({{"token",status->token},{"state",status->state}});lastImageToken=status->token;lastImageState=status->state;
                        if(status->state==VIS_NATIVE_COMMAND_FAILED)imageNotice(status->message.empty()?"The image could not be read":status->message);
                    }
                    if(!dropImageTest.empty()){
                        if(dropStage==1&&status->token==1&&status->state==VIS_NATIVE_COMMAND_APPLIED){dropImage(window.handle(),dropImageTest.parent_path()/"corrupt.png");dropStage=2;}
                        else if(dropStage==2&&status->token==2&&status->state==VIS_NATIVE_COMMAND_FAILED){dropImage(window.handle(),dropImageTest);dropImage(window.handle(),dropImageTest);dropStage=3;}
                        else if(dropStage==3&&status->token==4&&status->state==VIS_NATIVE_COMMAND_APPLIED)dropStage=4;
                    }
                }
                cameraMask|=1u<<camera;
                uint32_t encoding=0;const auto texture=plugin->image(&encoding);
                if(encoding!=(presenter.linearSource()?VIS_NATIVE_OUTPUT_LINEAR:VIS_NATIVE_OUTPUT_SRGB))throw std::runtime_error("Native plugin output encoding does not match its requested presentation mode");
                if(presenter.present(texture))++frames;
                if(stress&&frames&&frames%180==0)plugin->key(SDLK_SPACE,0);
                if(test&&playerPid&&frames>=30&&!readyWritten){std::ofstream(output/"ready.json")<<json{{"owner_pid",playerPid},{"frames",frames},{"player_pcm_windows",playerPcmWindows},{"player_pcm_peak",playerPcmPeak},{"input_picker_opens",pickerOpens}}.dump();readyWritten=true;}
                report["beats"]=audio.beatCount;
            }
            presenter.drain();
            report["presentation"]=presenter.presentationInfo();report["settings"]["hdr_active"]=report["presentation"]["hdr_active"];
            report["frames"]=frames;report["elapsed_ms"]=SDL_GetTicks64()-started;
            report["simulation_clock"]=test?"fixed-step":"monotonic";
            report["maximum_frames_in_flight"]=presenter.maximumFramesInFlight();report["surface_configurations"]=presenter.configurations();
            report["fullscreen_toggles"]=toggles;report["restored_bounds"]=restored;report["camera_input_mask"]=cameraMask;
            report["startup_bounds_restored"]=startupBoundsRestored;
            report["closed_by"]=reason;report["final_fullscreen"]=window.fullscreen();
            report["input_picker_opens"]=pickerOpens;report["recording_input_changes"]=inputChanges;report["recording_started"]=bool(capture);
            report["image_import_requests"]=imageRequests;report["image_import_failures"]=imageFailures;report["image_drop_test_stage"]=dropStage;
            report["recording_input_id"]=activeInput;
            report["player_pcm_windows"]=playerPcmWindows;report["player_pcm_peak"]=playerPcmPeak;report["player_input_requests_blocked"]=blockedInputRequests;
            if(capture){const auto status=capture->status();report["recording_device"]=status.device;report["recording_frames"]=status.frames;report["recording_dropped_frames"]=status.dropped;}
            if(reselectionTest&&pickerOpens!=4)throw std::runtime_error("Native input keyboard reselection did not run four times");
            if(effectKeyTest&&(effectSent!=effectKeys.size()||report["renderer_key_events"].size()!=effectKeys.size()))throw std::runtime_error("Native visualizer keyboard events were not all dispatched");
            if(!dropImageTest.empty()&&(dropStage!=4||imageRequests!=4||imageFailures!=1))throw std::runtime_error("Native image-drop application/recovery/newest-request validation did not complete");
            if(reselectionTest&&inputChanges!=expectedInputChanges)throw std::runtime_error("Native input reselection did not activate the expected capture streams");
            if(!frames&&reason!="input-cancelled"&&reason!="menu"&&reason!="escape"&&reason!="rightclick"&&reason!="window-close"&&reason!="player-exit")throw std::runtime_error("No native frames were presented");
            if(exercise&&(!restored||toggles!=(startupFullscreen?3u:2u)||stage!=5||(startupFullscreen&&!startupBoundsRestored)))throw std::runtime_error("Native resize/fullscreen exercise did not restore its window");
            if(test&&((closeTest!="player-exit"&&!testSent)||reason!=closeTest))throw std::runtime_error("Native close test did not use the requested closure path");
            if(playerTest&&(blockedInputRequests!=1||capture||pendingCapture||pickerOpens))throw std::runtime_error("Player mode allowed standalone capture selection");
            if(presenter.maximumFramesInFlight()>2)throw std::runtime_error("Native GPU submission limit exceeded");
        }
        report["result"]="pass";report["shutdown"]="renderer/presenter/context/window released";
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){
        const auto diagnostic=boundedDiagnostic(error.what());report["result"]="fail";report["error"]=diagnostic;
        std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';
        if(!test)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Native visualizer",diagnostic.c_str(),nullptr);
        return 1;
    }
}
