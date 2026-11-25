#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <psapi.h>
#include "shared_audio.hpp"
#include "presenter.hpp"
#include "catalog.hpp"
#include "window_control.hpp"
#include "native_runtime.hpp"
namespace fs=std::filesystem;
// The ABI is local to the two source ports, both built for x64 by this project.
struct VisModule {
    char* description; HWND hwndParent; HINSTANCE hDllInstance;
    int sRate,nCh,latencyMs,delayMs,spectrumNch,waveformNch;
    unsigned char spectrumData[2][576],waveformData[2][576];
    void(*Config)(VisModule*);int(*Init)(VisModule*);int(*Render)(VisModule*);void(*Quit)(VisModule*);void* userData;
};
struct VisHeader {int version;char* description;VisModule*(*getModule)(int);};
using GetHeader=VisHeader*(*)(HWND);
using GetFrame=const unsigned char*(*)(int*,int*);
using GeissKey=void(*)(UINT,WPARAM,LPARAM);
struct ScopeExit {std::function<void()> action;~ScopeExit(){action();}};
struct Entry {std::wstring id,name,library,adapter;fs::path package;};
struct Settings {int width=640,height=480,fps=60;bool fullscreen=false,hdr=false,classic=false,vsync=false;int peak=600;};
static fs::path base,profile,assets;
static std::vector<Entry> entries;
static Entry chosen;
static Settings settings;
static HWND window=nullptr,pluginWindow=nullptr,legacyParent=nullptr;
static WNDPROC pluginProc=nullptr;
static HMODULE library=nullptr;
static VisModule* module=nullptr;
static GetFrame getFrame=nullptr;
static GeissKey geissKey=nullptr;
static int(*keyState)()=nullptr;
static std::unique_ptr<suite::SharedAudio> shared;
static suite::AudioRing snapshot{};
static std::unique_ptr<Presenter> presenter;
static RECT restored{};
static bool fullscreen=false,running=true,demo=false;
static DWORD vlcPid=0,capturePid=0;
static std::wstring lineInput;
static std::vector<std::wstring> originalArguments;
static bool captureServiceTest=false,captureGeneratedTest=false,captureCancelTest=false;static unsigned captureReselections=0;
static void selectInput(){if(vlcPid){MessageBoxW(window,L"Audio is supplied by the media player. Open this visualizer on its own to select a recording input.",L"Audio input",MB_OK);return;}capturePid=GetCurrentProcessId();demo=false;shared.reset();std::vector<std::wstring> args={L"--visualizer",chosen.id,L"--capture-for",std::to_wstring(capturePid),L"--profile",profile.wstring()};if(!lineInput.empty())args.insert(args.end(),{L"--line-in",lineInput});if(captureServiceTest)args.insert(args.end(),{L"--self-test",L"--capture-test",L"--seconds",L"20"});if(captureGeneratedTest)args.push_back(L"--capture-test-generated");if(captureCancelTest)args.push_back(L"--capture-test-cancel");suite::nativeRuntime(base,args);}
static uint64_t frames=0,quitAfter=0;
static fs::path capture;
static bool selfTest=false,captured=false,windowChecks=false;
static bool background=false,stress=false;
static double audioPeak=0;
static void captureFrame(const unsigned char* pixels,int width,int height){
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(width)*height*4;
    file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
    std::ofstream out(capture,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(pixels),info.biSizeImage);
}
static std::wstring text(const fs::path& file,const wchar_t* section,const wchar_t* key,const wchar_t* fallback=L"") {
    wchar_t buf[1024];GetPrivateProfileStringW(section,key,fallback,buf,1024,file.c_str());return buf;
}
static bool validId(const std::wstring& s){return !s.empty()&&s.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789_-")==std::wstring::npos;}
static void loadCatalog() {
    auto cache=catalog::installed(base);
    for(const auto& cached:cache.at("visualizers")) {
        try {
            auto package=catalog::packageFor(base,cached);
            auto entry=catalog::validate(base,package);
            entries.push_back({catalog::wide(entry.at("id")),catalog::wide(entry.at("name")),
                catalog::contained(package,catalog::wide(entry.at("entrypoint"))).wstring(),
                catalog::wide(entry.at("adapter")),package});
        } catch(const std::exception&) { /* missing or changed packages are never loaded */ }
    }
}
static Settings readSettings(const Entry& e){Settings s;auto p=profile/L"settings.ini";auto section=e.id.c_str();s.width=std::clamp(GetPrivateProfileIntW(section,L"width",640,p.c_str()),320u,3840u);s.height=std::clamp(GetPrivateProfileIntW(section,L"height",480,p.c_str()),240u,2160u);s.fps=std::clamp(GetPrivateProfileIntW(section,L"fps",60,p.c_str()),15u,144u);s.fullscreen=GetPrivateProfileIntW(section,L"fullscreen",0,p.c_str())!=0;s.hdr=GetPrivateProfileIntW(section,L"hdr",0,p.c_str())!=0;s.peak=std::clamp(GetPrivateProfileIntW(section,L"peak",600,p.c_str()),200u,2000u);try{auto file=profile/(e.id+L".json");if(fs::exists(file)){auto j=catalog::read(file);s.width=std::clamp(j.value("width",s.width),320,3840);s.height=std::clamp(j.value("height",s.height),240,2160);s.fps=std::clamp(j.value("fps",s.fps),15,144);s.fullscreen=j.value("fullscreen",s.fullscreen);s.hdr=j.value("hdr",s.hdr);s.vsync=j.value("vsync",s.vsync);s.peak=std::clamp(j.value("peak",s.peak),200,2000);s.classic=j.value("engine",std::string("modern"))=="classic";}}catch(const std::exception&){}return s;}
static void put(const wchar_t* key,int value){WritePrivateProfileStringW(chosen.id.c_str(),key,std::to_wstring(value).c_str(),(profile/L"settings.ini").c_str());}
static HWND control(HWND parent,const wchar_t* type,const wchar_t* label,int id,int x,int y,int w,int h,DWORD style=0){auto c=CreateWindowExW(type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,(HMENU)INT_PTR(id),GetModuleHandleW(nullptr),nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return c;}
static LRESULT CALLBACK SettingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_CREATE){
        control(hwnd,L"BUTTON",L"Always open visualizer in full screen",100,20,20,410,25,BS_AUTOCHECKBOX);SendDlgItemMessageW(hwnd,100,BM_SETCHECK,settings.fullscreen,0);
        control(hwnd,L"STATIC",L"Window width",0,20,65,150,22);control(hwnd,L"EDIT",std::to_wstring(settings.width).c_str(),101,190,62,100,25,ES_NUMBER|WS_TABSTOP);
        control(hwnd,L"STATIC",L"Window height",0,20,100,150,22);control(hwnd,L"EDIT",std::to_wstring(settings.height).c_str(),102,190,97,100,25,ES_NUMBER|WS_TABSTOP);
        control(hwnd,L"STATIC",L"Frames per second",0,20,135,150,22);control(hwnd,L"EDIT",std::to_wstring(settings.fps).c_str(),103,190,132,100,25,ES_NUMBER|WS_TABSTOP);
        control(hwnd,L"BUTTON",L"HDR output — Experimental (SDR expansion)",104,20,175,410,25,BS_AUTOCHECKBOX);SendDlgItemMessageW(hwnd,104,BM_SETCHECK,settings.hdr,0);
        control(hwnd,L"STATIC",L"Peak brightness (nits)",0,20,215,160,22);control(hwnd,L"EDIT",std::to_wstring(settings.peak).c_str(),105,190,212,100,25,ES_NUMBER|WS_TABSTOP);
        control(hwnd,L"STATIC",L"HDR needs Windows HDR enabled on this display.\nThe original effects remain SDR; output uses scRGB.\nSettings take effect the next time this visualizer opens.",0,20,250,430,60);
        control(hwnd,L"BUTTON",L"Save",IDOK,265,325,85,28,BS_DEFPUSHBUTTON|WS_TABSTOP);control(hwnd,L"BUTTON",L"Cancel",IDCANCEL,360,325,85,28,WS_TABSTOP);return 0;
    }
    if(msg==WM_COMMAND){if(LOWORD(wp)==IDOK){BOOL ok[4];int w=GetDlgItemInt(hwnd,101,&ok[0],FALSE),h=GetDlgItemInt(hwnd,102,&ok[1],FALSE),f=GetDlgItemInt(hwnd,103,&ok[2],FALSE),peak=GetDlgItemInt(hwnd,105,&ok[3],FALSE);if(!ok[0]||!ok[1]||!ok[2]||!ok[3]||w<320||w>3840||h<240||h>2160||f<15||f>144||peak<200||peak>2000){MessageBoxW(hwnd,L"Use width 320–3840, height 240–2160, FPS 15–144, and peak 200–2000 nits.",L"Check settings",MB_OK);return 0;}put(L"width",w);put(L"height",h);put(L"fps",f);put(L"peak",peak);put(L"fullscreen",SendDlgItemMessageW(hwnd,100,BM_GETCHECK,0,0)==BST_CHECKED);put(L"hdr",SendDlgItemMessageW(hwnd,104,BM_GETCHECK,0,0)==BST_CHECKED);DestroyWindow(hwnd);}else if(LOWORD(wp)==IDCANCEL)DestroyWindow(hwnd);return 0;}
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);
}
static void toggleFullscreen(){
    if(!fullscreen){GetWindowRect(window,&restored);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&info);fullscreen=true;SetWindowPos(window,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,SWP_FRAMECHANGED);}
    else{fullscreen=false;SetWindowPos(window,HWND_NOTOPMOST,restored.left,restored.top,restored.right-restored.left,restored.bottom-restored.top,SWP_FRAMECHANGED);}
}
static void legacyMessage(UINT msg,WPARAM wp,LPARAM lp){if(geissKey)geissKey(msg,wp,lp);else if(pluginWindow&&pluginProc)CallWindowProcW(pluginProc,pluginWindow,msg,wp,lp);}
static LRESULT CALLBACK PluginProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_LBUTTONDBLCLK){toggleFullscreen();return 0;}if(msg==WM_CLOSE){running=false;return 0;}return CallWindowProcW(pluginProc,hwnd,msg,wp,lp);}
// The old renderers forward unhandled keys back to their media-player parent.
// Keep that protocol endpoint separate from the visible input window to avoid
// posting those same keys back into the renderer indefinitely.
static LRESULT CALLBACK LegacyParentProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_APP+42){toggleFullscreen();return 0;}
    if(msg==WM_CLOSE){running=false;return 0;}
    if(msg==WM_USER){switch(lp){case 0:return 0x5050;case 104:return 1;case 105:return 0;case 124:return 1;case 215:return 0;case 1336:return (LRESULT)assets.c_str();case 1335:return 0;case 101:return 1;default:return 0;}}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static LRESULT CALLBACK HostProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_APP+42){toggleFullscreen();return 0;}
    if(msg==WM_USER){switch(lp){case 0:return 0x5050;case 104:return 1;case 105:return 0;case 124:return 1;case 215:return 0;case 1336:return (LRESULT)assets.c_str();case 1335:return 0;case 101:return 1;default:return 0;}}
    if(msg==WM_LBUTTONDBLCLK){toggleFullscreen();return 0;}
    if(msg==WM_RBUTTONUP){running=false;return 0;}
    if(msg==WM_KEYDOWN&&wp=='L'&&GetKeyState(VK_CONTROL)<0){try{selectInput();}catch(const std::exception& e){MessageBoxA(hwnd,e.what(),"Audio input",MB_OK);}return 0;}
    if(msg==WM_CHAR&&wp==12)return 0;
    if(msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_CHAR||msg==WM_SYSKEYDOWN){if((msg==WM_KEYDOWN&&wp==VK_F11)||(msg==WM_SYSKEYDOWN&&wp==VK_RETURN)){toggleFullscreen();return 0;}if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){running=false;return 0;}if(msg==WM_SYSKEYDOWN&&wp==VK_F4){DestroyWindow(hwnd);return 0;}legacyMessage(msg,wp,lp);return 0;}
    if(msg==WM_LBUTTONDOWN){SetFocus(hwnd);if(GetKeyState(VK_MENU)<0){ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);}return 0;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_CLOSE){running=false;return 0;}
    if(msg==WM_DESTROY){running=false;PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void log(const std::string& line){std::ofstream(profile/L"last-run.log",std::ios::app)<<line<<"\n";}
static int runVisualizer(){
    bool initialized=false;
    DLL_DIRECTORY_COOKIE searchCookie=nullptr;
    ScopeExit cleanup{[&]{
        presenter.reset();
        if(pluginWindow&&IsWindow(pluginWindow)&&pluginProc)SetWindowLongPtrW(pluginWindow,GWLP_WNDPROC,(LONG_PTR)pluginProc);
        if(initialized&&module)module->Quit(module);
        module=nullptr;
        if(library){FreeLibrary(library);library=nullptr;}
        if(searchCookie)RemoveDllDirectory(searchCookie);
        if(legacyParent&&IsWindow(legacyParent))DestroyWindow(legacyParent);
        if(window&&IsWindow(window))DestroyWindow(window);
    }};
    window=CreateWindowExW(background?WS_EX_NOACTIVATE:0,L"VLCVisualizersWindow",chosen.name.c_str(),WS_POPUP|WS_VISIBLE,100,100,settings.width,settings.height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 2;
    suite::WindowControl windowControl(base,catalog::utf8(chosen.id),vlcPid);
    WNDCLASSW parentClass{};parentClass.lpfnWndProc=LegacyParentProc;parentClass.hInstance=GetModuleHandleW(nullptr);parentClass.lpszClassName=L"VLCVisualizersLegacyParent";RegisterClassW(&parentClass);
    legacyParent=CreateWindowW(parentClass.lpszClassName,L"VLC audio",0,0,0,0,0,nullptr,nullptr,parentClass.hInstance,nullptr);
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS);searchCookie=AddDllDirectory(chosen.package.c_str());
    library=LoadLibraryExW((base/chosen.library).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library){MessageBoxW(window,L"Could not load the x64 renderer. Check its D3DX runtime files.",L"Visualizers",MB_OK|MB_ICONERROR);return 3;}
    auto header=reinterpret_cast<GetHeader>(GetProcAddress(library,"winampVisGetHeader"));
    if(!header)return 4;
    auto info=header(legacyParent);if(!info||!(module=info->getModule(0)))return 4;
    module->hwndParent=legacyParent;module->hDllInstance=library;module->sRate=44100;module->nCh=2;
    if(module->Init(module)){log("Renderer initialization failed");return 5;}
    initialized=true;
    getFrame=reinterpret_cast<GetFrame>(GetProcAddress(library,"SuiteGetFrame"));
    keyState=reinterpret_cast<int(*)()>(GetProcAddress(library,"SuiteKeyState"));
    if(chosen.adapter==L"geiss"){
        geissKey=reinterpret_cast<GeissKey>(GetProcAddress(library,"SuiteKey"));
    }else{
        auto resize=reinterpret_cast<void(*)(int,int)>(GetProcAddress(library,"SuiteSetSize"));
        if(resize)resize(settings.width,settings.height);
        auto getWindow=reinterpret_cast<HWND(*)()>(GetProcAddress(library,"SuiteWindow"));
        if(getWindow)pluginWindow=getWindow();
        if(pluginWindow){pluginProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(pluginWindow,GWLP_WNDPROC,(LONG_PTR)PluginProc));ShowWindow(pluginWindow,SW_HIDE);}
    }
    if(!getFrame)return 6;
    presenter=std::make_unique<Presenter>(window,settings.hdr,settings.peak,settings.vsync);
    if(!presenter->valid()){MessageBoxW(window,L"Could not initialize Direct3D 11 presentation.",L"Visualizers",MB_OK);return 7;}
    log(std::string("renderer initialized; HDR output=")+(presenter->hdr()?"active":"SDR"));
    if(settings.hdr&&!presenter->hdr()&&!selfTest)MessageBoxW(window,L"HDR is unavailable on this display. The visualizer will use SDR. Enable Windows HDR and reopen it to try again.",L"HDR output",MB_OK|MB_ICONINFORMATION);
    if(settings.fullscreen)toggleFullscreen();
    log(std::string("startup_fullscreen=")+(fullscreen?"1":"0"));
    if(!background){SetForegroundWindow(window);SetFocus(window);}
    HANDLE parent=vlcPid?OpenProcess(SYNCHRONIZE,FALSE,vlcPid):nullptr;
    auto started=GetTickCount64();auto next=started;
    auto nextMetric=started;
    timeBeginPeriod(1);
    while(running){
        if(windowControl.closeRequested())break;
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;
        auto now=GetTickCount64();if(parent&&WaitForSingleObject(parent,0)==WAIT_OBJECT_0)break;if(quitAfter&&now-started>=quitAfter)break;
        if(selfTest&&now>=nextMetric){PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);DWORD handles=0;GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&counters,sizeof(counters));GetProcessHandleCount(GetCurrentProcess(),&handles);std::ofstream metrics(profile/L"metrics.jsonl",std::ios::app);metrics<<"{\"elapsed_ms\":"<<(now-started)<<",\"frames\":"<<frames<<",\"private_bytes\":"<<counters.PrivateUsage<<",\"handles\":"<<handles<<",\"gdi\":"<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<<",\"user\":"<<GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)<<"}\n";nextMetric=now+5000;}
        if(now<next){MsgWaitForMultipleObjectsEx(0,nullptr,DWORD(next-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);continue;}next=now+1000/settings.fps;
        if(demo){float pcm[2048];for(unsigned i=0;i<1024;++i){float t=float((now-started)*.001+i/44100.0);pcm[2*i]=0.55f*std::sin(t*6.2831853f*220.0f)*(0.65f+0.35f*std::sin(t*6.2831853f*2));pcm[2*i+1]=0.5f*std::sin(t*6.2831853f*330.0f);}suite::write(snapshot,pcm,1024,2,44100,now);}
        else{auto audioPid=capturePid?capturePid:vlcPid;if((!shared||!shared->ring)&&audioPid)shared=std::make_unique<suite::SharedAudio>(audioPid,false);if(shared&&shared->ring&&shared->lock()){memcpy(&snapshot,shared->ring,sizeof(snapshot));shared->unlock();}}
        suite::analyze(snapshot,now,module->waveformData,module->spectrumData);
        for(auto sample:module->waveformData[0])audioPeak=std::max(audioPeak,double(std::abs(int(static_cast<int8_t>(sample)))));
        if(module->Render(module))break;
        if(stress&&frames&&frames%180==0){if(chosen.adapter==L"geiss"){const char keys[]="NPWI";SendMessageW(window,WM_KEYDOWN,keys[(frames/180)%4],0);SendMessageW(window,WM_KEYUP,keys[(frames/180)%4],0);}else{SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);SendMessageW(window,WM_CHAR,' ',0);SendMessageW(window,WM_KEYUP,VK_SPACE,0);}}
        int w=0,h=0;auto pixels=getFrame(&w,&h);if(pixels&&w>0&&h>0&&w<=8192&&h<=8192){if(presenter->draw(pixels,w,h,fullscreen))++frames;else {presenter=std::make_unique<Presenter>(window,settings.hdr,settings.peak,settings.vsync);if(!presenter->valid()){log("presentation device unavailable; closing cleanly");break;}}
            if(!captured&&!capture.empty()&&frames>=60){captureFrame(pixels,w,h);captured=true;log("captured="+std::to_string(w)+"x"+std::to_string(h));}
        }
        if(selfTest&&!windowChecks&&frames>=15){
            RECT before,full,after;GetWindowRect(window,&before);bool original=fullscreen;
            SendMessageW(window,WM_LBUTTONDBLCLK,0,0);GetWindowRect(window,&full);
            SendMessageW(window,WM_LBUTTONDBLCLK,0,0);GetWindowRect(window,&after);
            bool restoredCorrectly=EqualRect(&before,&after)&&fullscreen==original;
            bool borderless=(GetWindowLongPtrW(window,GWL_STYLE)&(WS_CAPTION|WS_THICKFRAME))==0;
            log(std::string("double-click restore=")+(restoredCorrectly?"pass":"FAIL")+"; borderless="+(borderless?"pass":"FAIL"));
            if(!restoredCorrectly||!borderless){running=false;break;}
            if(keyState){int beforeKey=keyState();bool geiss=chosen.adapter==L"geiss";SendMessageW(window,WM_KEYDOWN,geiss?'L':VK_F1,0);int afterKey=keyState();SendMessageW(window,WM_KEYDOWN,geiss?'L':VK_F1,0);bool keyWorks=beforeKey!=afterKey&&keyState()==beforeKey;log(std::string("native key round-trip=")+(keyWorks?"pass":"FAIL"));if(!keyWorks){running=false;break;}}
            windowChecks=true;
        }
        // Invoke this process's normal source-selection callback only. Actual
        // desktop Ctrl-L/focus checks remain part of physical UI validation.
        if(captureServiceTest&&captureReselections<3&&frames>=(captureReselections+1)*45){selectInput();++captureReselections;}
    }
    timeEndPeriod(1);if(parent)CloseHandle(parent);
    if(captureServiceTest)log("capture_reselections="+std::to_string(captureReselections));
    log("frames="+std::to_string(frames)+"; audio_peak="+std::to_string(audioPeak));return frames&&(!selfTest||windowChecks)?0:8;
}
static int runHost(HINSTANCE instance){
    SetProcessDPIAware();InitCommonControls();
    wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);base=fs::path(exe).parent_path();wchar_t local[32768];DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);profile=(length&&length<32768?fs::path(local):base)/L"VLCVisualizers/profiles";assets=base/L"assets";
    int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::wstring id;bool edit=false,close=false,forceClassic=false;
    for(int i=1;i<argc;++i)originalArguments.emplace_back(argv[i]);
    for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--classic"){forceClassic=true;}else if(arg==L"--line-in"){lineInput=(i+1<argc&&std::wstring(argv[i+1]).rfind(L"--",0)!=0)?argv[++i]:L"default";}else if(arg==L"--close"&&i+1<argc){id=argv[++i];close=true;}else if(arg==L"--visualizer"&&i+1<argc)id=argv[++i];else if(arg==L"--settings"&&i+1<argc){id=argv[++i];edit=true;}else if(arg==L"--vlc-pid"&&i+1<argc)vlcPid=wcstoul(argv[++i],nullptr,10);else if(arg==L"--profile"&&i+1<argc)profile=argv[++i];else if(arg==L"--demo")demo=true;else if(arg==L"--background")background=true;else if(arg==L"--stress")stress=true;else if(arg==L"--self-test")selfTest=true;else if(arg==L"--capture-service-test")captureServiceTest=true;else if(arg==L"--capture-generated-test")captureGeneratedTest=true;else if(arg==L"--capture-cancel-test")captureCancelTest=true;else if(arg==L"--capture"&&i+1<argc)capture=argv[++i];else if(arg==L"--quit-after"&&i+1<argc)quitAfter=wcstoull(argv[++i],nullptr,10);}
    LocalFree(argv);if((captureServiceTest&&!selfTest)||((captureGeneratedTest||captureCancelTest)&&!captureServiceTest)||((captureServiceTest||captureGeneratedTest)&&vlcPid))throw std::runtime_error("Capture automation requires a standalone --self-test");if(close){suite::closeWindows(base,catalog::utf8(id),vlcPid);return 0;}fs::create_directories(profile);loadCatalog();
    if(id.empty()){
        HMENU menu=CreatePopupMenu();for(size_t i=0;i<entries.size();++i){AppendMenuW(menu,MF_STRING,100+UINT(i)*2,entries[i].name.c_str());AppendMenuW(menu,MF_STRING,101+UINT(i)*2,(entries[i].name+L" settings…").c_str());}
        WNDCLASSW c{};c.lpfnWndProc=DefWindowProcW;c.lpszClassName=L"SuiteChooser";c.hInstance=instance;RegisterClassW(&c);auto owner=CreateWindowW(c.lpszClassName,L"Visualizers",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,instance,nullptr);
        POINT point;GetCursorPos(&point);SetForegroundWindow(owner);UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,owner,nullptr);DestroyMenu(menu);DestroyWindow(owner);if(selected<100)return 0;size_t n=(selected-100)/2;if(n>=entries.size())return 1;id=entries[n].id;edit=(selected-100)%2!=0;
    }
    for(auto& entry:entries)if(entry.id==id)chosen=entry;
    if(chosen.id.empty()){if(!selfTest)MessageBoxW(nullptr,L"No installed x64 visualizer matches this ID. Scan visualizers after installing a package.",L"Visualizers",MB_OK);return 1;}
    assets=chosen.package/L"assets";settings=readSettings(chosen);
    if(edit&&fs::is_regular_file(suite::nativeRuntimePath(base)))return int(suite::nativeRuntime(base,{L"--settings",chosen.id,L"--profile",profile.wstring(),selfTest?L"--self-test":L"--background"},true));
    if(chosen.adapter==L"dawn"){
        if(!forceClassic&&!settings.classic){auto args=originalArguments;args.insert(args.end(),{L"--visualizer",chosen.id,L"--profile",profile.wstring()});return int(suite::nativeRuntime(base,args,true));}
        auto manifest=catalog::read(chosen.package/L"manifest.json");auto classic=manifest.at("classic");chosen.library=catalog::contained(chosen.package,catalog::wide(classic.at("entrypoint"))).wstring();chosen.adapter=catalog::wide(classic.at("adapter"));
        if((chosen.adapter!=L"geiss"&&chosen.adapter!=L"winamp")||!catalog::amd64(chosen.library))throw std::runtime_error("Classic renderer is missing or incompatible");chosen.name+=L" — Classic";
    }
    if(chosen.adapter==L"webgpu"){if(!forceClassic&&!settings.classic&&!edit)throw std::runtime_error("Install the native version of this visualizer to open it with this suite");if(!edit){auto manifest=catalog::read(chosen.package/L"manifest.json");auto classic=manifest.at("classic");chosen.library=catalog::contained(chosen.package,catalog::wide(classic.at("entrypoint"))).wstring();chosen.adapter=catalog::wide(classic.at("adapter"));if((chosen.adapter!=L"geiss"&&chosen.adapter!=L"winamp")||!catalog::amd64(chosen.library))throw std::runtime_error("Classic renderer is missing or incompatible");chosen.name+=L" — Classic";}}
    WNDCLASSW c{};c.style=CS_DBLCLKS;c.lpfnWndProc=edit?SettingsProc:HostProc;c.hInstance=instance;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);c.lpszClassName=edit?L"VLCVisualizersSettings":L"VLCVisualizersWindow";RegisterClassW(&c);
    if(edit){auto hwnd=CreateWindowExW(0,c.lpszClassName,(chosen.name+L" settings").c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,150,150,485,405,nullptr,nullptr,instance,nullptr);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(!IsDialogMessageW(hwnd,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}return 0;}
    std::ofstream(profile/L"last-run.log")<<"VLC Visualizers x64: starting\n";
    if(!demo&&!vlcPid)selectInput();
    int result=runVisualizer();log("exit="+std::to_string(result));return result;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){try{return runHost(instance);}catch(const std::exception& error){if(!profile.empty())log(std::string("error: ")+error.what());if(!selfTest)MessageBoxA(nullptr,error.what(),"Visualizer error",MB_OK|MB_ICONERROR);return 10;}}
