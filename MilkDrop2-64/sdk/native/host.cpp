#include "window_control.hpp"
#include "native_runtime.hpp"
#include "catalog.hpp"
#include "settings.hpp"
#include "visualizer_api.h"
#include "shared_audio.hpp"
#include <SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <mach/mach.h>
#else
#include <GL/gl.h>
#endif
#include <dlfcn.h>
#include <signal.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <functional>
namespace fs=std::filesystem;using native::json;
struct Scope {std::function<void()> exit;~Scope(){exit();}};
static void message(const std::string& value,bool test){const auto diagnostic=suite::catalogDiagnostic<json>(value);if(test)std::cerr<<diagnostic<<'\n';else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Visualizer",diagnostic.c_str(),nullptr);}
static uint64_t residentBytes(){
#ifdef __APPLE__
    mach_task_basic_info_data_t info{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,(task_info_t)&info,&count)!=KERN_SUCCESS)return 0;return info.resident_size;
#else
    std::ifstream in("/proc/self/statm");uint64_t total=0,resident=0;in>>total>>resident;return resident*sysconf(_SC_PAGESIZE);
#endif
}
int main(int argc,char** argv){
    fs::path profile=native::defaultProfile(),suite=fs::absolute(argv[0]).parent_path(),capture;std::string id;unsigned parent=0,capturePid=0;std::string lineInput;std::vector<std::string> originalArguments(argv+1,argv+argc);uint64_t quitAfter=0;bool demo=false,test=false,background=false,stress=false,edit=false,close=false,forceClassic=false,captureServiceTest=false,captureGeneratedTest=false,captureCancelTest=false;unsigned captureReselections=0;
    try{
    if(const char* override=getenv("VLC_VISUALIZERS_SUITE"))suite=override;
    for(int n=1;n<argc;++n){std::string a=argv[n];if(a=="--classic")forceClassic=true;else if(a=="--line-in"){lineInput=n+1<argc&&std::string(argv[n+1]).rfind("--",0)!=0?argv[++n]:"default";}else if(a=="--close"&&n+1<argc){id=argv[++n];close=true;}else if(a=="--visualizer"&&n+1<argc)id=argv[++n];else if(a=="--settings"&&n+1<argc){id=argv[++n];edit=true;}else if(a=="--profile"&&n+1<argc)profile=argv[++n];else if(a=="--suite"&&n+1<argc)suite=argv[++n];else if(a=="--vlc-pid"&&n+1<argc)parent=std::stoul(argv[++n]);else if(a=="--quit-after"&&n+1<argc)quitAfter=std::stoull(argv[++n]);else if(a=="--capture"&&n+1<argc)capture=argv[++n];else if(a=="--demo")demo=true;else if(a=="--self-test")test=true;else if(a=="--capture-service-test")captureServiceTest=true;else if(a=="--capture-generated-test")captureGeneratedTest=true;else if(a=="--capture-cancel-test")captureCancelTest=true;else if(a=="--background")background=true;else if(a=="--stress")stress=true;}
        if((captureServiceTest&&!test)||((captureGeneratedTest||captureCancelTest)&&!captureServiceTest)||((captureServiceTest||captureGeneratedTest)&&parent))throw std::runtime_error("Capture automation requires a standalone --self-test");
        if(close){suite::closeWindows(suite,id,parent);return 0;}
        fs::create_directories(profile);auto cache=native::read(suite/"installed-visualizers.json");json entry;for(const auto& value:cache.at("visualizers"))if(value.at("id")==id)entry=value;
        if(entry.is_null())throw std::runtime_error("No registered visualizer matches this ID; scan after installation");
        auto package=native::contained(suite,entry.at("package").get<std::string>());auto fresh=native::validate(suite,package);if(fresh.at("id")!=id)throw std::runtime_error("Package changed since scanning");entry=std::move(fresh);
        auto settings=native::readSettings(profile,id);
        if(edit&&fs::is_regular_file(suite::nativeRuntimePath(suite))){std::vector<std::string> args={"--settings",id,"--profile",profile.string()};if(test)args.push_back("--self-test");return suite::nativeRuntime(suite,args,true);}
        if(entry.at("adapter")=="dawn"){
            bool classic=forceClassic;try{classic=classic||native::read(profile/(id+".json")).value("engine",std::string("modern"))=="classic";}catch(...){}
            if(!classic)return suite::nativeRuntime(suite,originalArguments,true);
            const auto compatibility=entry.at("classic");entry["entrypoint"]=compatibility.at("entrypoint");entry["adapter"]=compatibility.at("adapter");
            if(entry.at("adapter")!="opengl"||!native::binaryMatches(native::contained(package,entry.at("entrypoint").get<std::string>())))throw std::runtime_error("Classic renderer is missing or incompatible");
        }
        if(entry.at("adapter")=="webgpu"){bool classic=forceClassic;try{classic=classic||native::read(profile/(id+".json")).value("engine",std::string("modern"))=="classic";}catch(...){}if(!classic&&!edit)throw std::runtime_error("Install the native version of this visualizer to open it with this suite");if(!edit){auto compatibility=entry.at("classic");entry["entrypoint"]=compatibility.at("entrypoint");entry["adapter"]="opengl";if(!native::binaryMatches(native::contained(package,entry.at("entrypoint").get<std::string>())))throw std::runtime_error("Classic renderer is missing or incompatible");}}
        if(edit){if(native::editSettings(settings,entry.at("name"),argc,argv))native::saveSettings(profile,id,settings);return 0;}
        fs::path logFile=profile/"last-run.log";std::ofstream(logFile)<<"VLC Visualizers native: "<<native::platform()<<' '<<native::architecture()<<"\n";
        auto log=[&](const std::string& line){std::ofstream(logFile,std::ios::app)<<line<<'\n';};
        if(background)SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
#ifdef __APPLE__
        SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,"0");
#endif
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER))throw std::runtime_error(SDL_GetError());Scope quitSDL{[]{SDL_Quit();}};
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
        SDL_Window* window=SDL_CreateWindow(entry.at("name").get<std::string>().c_str(),100,100,settings.width,settings.height,SDL_WINDOW_OPENGL|SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(background?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));
        if(!window)throw std::runtime_error(SDL_GetError());
        suite::WindowControl windowControl(suite,id,parent);Scope closeWindow{[&]{SDL_DestroyWindow(window);}};
        SDL_GLContext context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(SDL_GetError());Scope closeContext{[&]{SDL_GL_DeleteContext(context);}};
        SDL_GL_SetSwapInterval(0);
        if(background)SDL_ShowWindow(window);
        void* dll=dlopen(native::contained(package,entry.at("entrypoint").get<std::string>()).c_str(),RTLD_NOW|RTLD_LOCAL);if(!dll)throw std::runtime_error(dlerror());Scope closeDll{[&]{dlclose(dll);}};
        auto getApi=(VisualizerGetApiFn)dlsym(dll,"VisualizerGetApi");const VisualizerApi* api=getApi?getApi(VISUALIZER_API_VERSION):nullptr;
        if(!api||api->size<sizeof(VisualizerApi)||api->api_version!=VISUALIZER_API_VERSION||!api->create||!api->destroy||!api->render||!api->resize)throw std::runtime_error("Renderer API 2 is unavailable");
        std::unique_ptr<suite::SharedAudio> shared;
        auto selectInput=[&]{if(parent)throw std::runtime_error("Audio is supplied by the media player; open a standalone instance to select a recording input");capturePid=getpid();demo=false;shared.reset();std::vector<std::string> args={"--visualizer",id,"--capture-for",std::to_string(capturePid),"--profile",profile.string()};if(!lineInput.empty())args.insert(args.end(),{"--line-in",lineInput});if(captureServiceTest)args.insert(args.end(),{"--self-test","--capture-test","--seconds","20"});if(captureGeneratedTest)args.push_back("--capture-test-generated");if(captureCancelTest)args.push_back("--capture-test-cancel");suite::nativeRuntime(suite,args);};
        if(!demo&&!parent)selectInput();
        int pixelWidth,pixelHeight;SDL_GL_GetDrawableSize(window,&pixelWidth,&pixelHeight);std::string assets=(package/"assets").string();VisualizerCreateInfo info{sizeof(info),VISUALIZER_API_VERSION,pixelWidth,pixelHeight,assets.c_str()};
        void* renderer=api->create(&info);if(!renderer)throw std::runtime_error("Renderer initialization failed");Scope closeRenderer{[&]{api->destroy(renderer);}};
        bool fullscreen=false;int oldX=100,oldY=100,oldW=settings.width,oldH=settings.height;
        auto toggle=[&]{
            if(!fullscreen){
                SDL_GetWindowPosition(window,&oldX,&oldY);SDL_GetWindowSize(window,&oldW,&oldH);
                if(SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP))throw std::runtime_error(SDL_GetError());
                fullscreen=true;
            }else{
                if(SDL_SetWindowFullscreen(window,0))throw std::runtime_error(SDL_GetError());
                SDL_SetWindowPosition(window,oldX,oldY);SDL_SetWindowSize(window,oldW,oldH);fullscreen=false;
            }
            if(settings.vsync&&SDL_GL_SetSwapInterval(fullscreen?1:0))log("Could not change the OpenGL swap interval");
        };
        if(settings.fullscreen)toggle();log(std::string("startup_fullscreen=")+(fullscreen?"1":"0"));log("HDR output=SDR");log(std::string("OpenGL=")+(const char*)glGetString(GL_VERSION));
        int initialX,initialY,initialW,initialH;SDL_GetWindowPosition(window,&initialX,&initialY);SDL_GetWindowSize(window,&initialW,&initialH);const bool initialFull=fullscreen;
        static suite::AudioRing snapshot{};uint8_t wave[2][576],spectrum[2][576];float pcm[1152];
        uint64_t started=suite::milliseconds(),next=started,nextMetric=started,frames=0;bool running=true,firstToggle=false,secondToggle=false,checks=false,captured=false;double peak=0;int lastW=0,lastH=0;
        while(running){
            if(windowControl.closeRequested())break;
            SDL_Event event;while(SDL_PollEvent(&event)){if(event.type==SDL_DROPFILE||event.type==SDL_DROPTEXT){SDL_free(event.drop.file);continue;}if(event.type==SDL_QUIT||(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_RIGHT))running=false;else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT&&event.button.clicks>=2&&(event.button.clicks%2)==0)toggle();else if(event.type==SDL_KEYDOWN||event.type==SDL_KEYUP){const bool down=event.type==SDL_KEYDOWN;auto key=event.key.keysym.sym;if(down&&key==SDLK_l&&(event.key.keysym.mod&(KMOD_CTRL|KMOD_GUI))){try{selectInput();}catch(const std::exception& error){message(error.what(),test);}}else if(down&&(key==SDLK_F11||(key==SDLK_RETURN&&(event.key.keysym.mod&KMOD_ALT))))toggle();else if(down&&key==SDLK_ESCAPE){running=false;}else if(api->key)api->key(renderer,key,down);}}
            auto now=suite::milliseconds();if(!running||(quitAfter&&now-started>=quitAfter)||(parent&&kill(parent,0)&&errno==ESRCH))break;
            if(now<next){SDL_Delay(unsigned(std::min<uint64_t>(next-now,5)));continue;}next=now+(1000+settings.fps-1)/settings.fps;
            if(demo){float input[2048];for(int n=0;n<1024;++n){double t=(now-started)*.001+n/44100.;input[n*2]=.55f*std::sin(t*1382.3);input[n*2+1]=.5f*std::sin(t*2073.45);}suite::write(snapshot,input,1024,2,44100,now);}
            else {auto audioPid=capturePid?capturePid:parent;if((!shared||!shared->ring)&&audioPid)shared=std::make_unique<suite::SharedAudio>(audioPid,false);if(shared&&shared->ring&&shared->lock()){std::memcpy(&snapshot,shared->ring,sizeof(snapshot));shared->unlock();}}
            suite::analyze(snapshot,now,wave,spectrum);for(int n=0;n<576;++n)for(int ch=0;ch<2;++ch){pcm[n*2+ch]=float(static_cast<int8_t>(wave[ch][n]))/128.f;peak=std::max(peak,double(std::abs(pcm[n*2+ch])));}
            SDL_GL_GetDrawableSize(window,&pixelWidth,&pixelHeight);if(pixelWidth!=lastW||pixelHeight!=lastH){api->resize(renderer,pixelWidth,pixelHeight);lastW=pixelWidth;lastH=pixelHeight;}glViewport(0,0,pixelWidth,pixelHeight);
            VisualizerAudio audio{sizeof(audio),576,44100,2,pcm,(now-started)*.001};if(api->render(renderer,&audio))throw std::runtime_error("Renderer reported failure");++frames;
            if(!capture.empty()&&!captured&&frames>=60){std::vector<unsigned char> pixels(size_t(pixelWidth)*pixelHeight*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,pixelWidth,pixelHeight,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(capture,std::ios::binary);out<<"P6\n"<<pixelWidth<<' '<<pixelHeight<<"\n255\n";for(int y=pixelHeight-1;y>=0;--y)out.write((char*)pixels.data()+size_t(y)*pixelWidth*3,pixelWidth*3);captured=true;}
            SDL_GL_SwapWindow(window);
            if(captureServiceTest&&captureReselections<3&&frames>=(captureReselections+1)*45){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_l;e.key.keysym.mod=KMOD_CTRL;SDL_PushEvent(&e);++captureReselections;}
            if(stress&&frames%180==0&&api->key)api->key(renderer,' ',1);
            if(test){if(frames>=15&&!firstToggle){SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=2;SDL_PushEvent(&e);firstToggle=true;}else if(frames>=30&&!secondToggle){SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=4;SDL_PushEvent(&e);secondToggle=true;}else if(frames>=45&&!checks){int x,y,w,h;SDL_GetWindowPosition(window,&x,&y);SDL_GetWindowSize(window,&w,&h);checks=initialFull==fullscreen&&initialX==x&&initialY==y&&initialW==w&&initialH==h&&(SDL_GetWindowFlags(window)&SDL_WINDOW_BORDERLESS);log(std::string("double-click restore/borderless=")+(checks?"pass":"FAIL"));if(!checks)throw std::runtime_error("Window self-test failed");if(api->key&&api->key_state){int before=api->key_state(renderer);api->key(renderer,'l',1);int after=api->key_state(renderer);api->key(renderer,'l',1);bool ok=before!=after&&before==api->key_state(renderer);log(std::string("native key round-trip=")+(ok?"pass":"FAIL"));if(!ok)throw std::runtime_error("Keyboard self-test failed");}}}
            if(test&&now>=nextMetric){std::ofstream metrics(profile/"metrics.jsonl",std::ios::app);metrics<<json{{"elapsed_ms",now-started},{"frames",frames},{"resident_bytes",residentBytes()}}.dump()<<'\n';nextMetric=now+5000;}
        }
        if(captureServiceTest)log("capture_reselections="+std::to_string(captureReselections));
        const int result=frames&&(!test||checks)?0:8;log("frames="+std::to_string(frames)+"; audio_peak="+std::to_string(peak));log("exit="+std::to_string(result));return result;
    }catch(const std::exception& error){message(error.what(),test);return 1;}
}
