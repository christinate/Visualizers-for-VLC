#define UNICODE
#include <windows.h>
#include <filesystem>
#include "shared_audio.hpp"
#include "process.hpp"
#ifndef VIS_ID
#define VIS_ID L"geiss"
#endif
#ifndef VIS_LABEL
#define VIS_LABEL "Geiss 64-bit bridge"
#endif
struct Module {char* description;HWND hwndParent;HINSTANCE hDllInstance;int sRate,nCh,latencyMs,delayMs,spectrumNch,waveformNch;unsigned char spectrumData[2][576],waveformData[2][576];void(*Config)(Module*);int(*Init)(Module*);int(*Render)(Module*);void(*Quit)(Module*);void* userData;};
struct Header {int version;char* description;Module*(*getModule)(int);};
struct Session {suite::SharedAudio audio{GetCurrentProcessId(),true};};
static std::filesystem::path host(){wchar_t path[32768];DWORD count=GetEnvironmentVariableW(L"VLC_VISUALIZERS_BRIDGE_HOST",path,32768);if(count&&count<32768)return path;DWORD bytes=sizeof(path);auto key=std::wstring(L"Software\\VLCVisualizers\\Renderers\\")+VIS_ID;LSTATUS result=RegGetValueW(HKEY_LOCAL_MACHINE,key.c_str(),L"Host",RRF_RT_REG_SZ|RRF_SUBKEY_WOW6464KEY,nullptr,path,&bytes);if(result!=ERROR_SUCCESS)throw std::runtime_error("Reinstall this visualizer to register its x64 host");return path;}
static void config(Module*){try{process::launch(host(),{L"--settings",VIS_ID});}catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Visualizer settings",MB_OK|MB_ICONERROR);}}
static int init(Module* module){try{auto state=new Session;if(!state->audio.ring){delete state;return 1;}module->userData=state;std::vector<std::wstring> args={L"--visualizer",VIS_ID,L"--vlc-pid",std::to_wstring(GetCurrentProcessId())};wchar_t profile[32768];DWORD count=GetEnvironmentVariableW(L"VLC_VISUALIZERS_BRIDGE_TEST_PROFILE",profile,32768);if(count&&count<32768)args.insert(args.end(),{L"--profile",profile,L"--self-test",L"--background",L"--quit-after",L"1200"});process::launch(host(),args);return 0;}catch(const std::exception& e){delete static_cast<Session*>(module->userData);module->userData=nullptr;MessageBoxA(module->hwndParent,e.what(),"Visualizer",MB_OK|MB_ICONERROR);return 1;}}
static int render(Module* module){auto state=static_cast<Session*>(module->userData);if(!state)return 1;float pcm[1152];for(int n=0;n<576;++n)for(int ch=0;ch<2;++ch)pcm[n*2+ch]=float(static_cast<signed char>(module->waveformData[ch][n]))/128.0f;if(state->audio.lock()){suite::write(*state->audio.ring,pcm,576,2,module->sRate>0?module->sRate:44100,GetTickCount64());state->audio.unlock();}return 0;}
static void quit(Module* module){delete static_cast<Session*>(module->userData);module->userData=nullptr;}
static Module module={const_cast<char*>(VIS_LABEL),nullptr,nullptr,44100,2,0,16,2,2,{}, {},config,init,render,quit,nullptr};
static Module* get(int index){return index==0?&module:nullptr;}
static Header header={0x101,const_cast<char*>(VIS_LABEL),get};
extern "C" __declspec(dllexport) Header* winampVisGetHeader(HWND){return &header;}
