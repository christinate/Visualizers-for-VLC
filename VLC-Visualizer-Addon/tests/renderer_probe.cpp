#define UNICODE
#include <windows.h>
#include <psapi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <vector>
#include "catalog.hpp"
struct Module {char* description;HWND hwndParent;HINSTANCE hDllInstance;int sRate,nCh,latencyMs,delayMs,spectrumNch,waveformNch;unsigned char spectrumData[2][576],waveformData[2][576];void(*Config)(Module*);int(*Init)(Module*);int(*Render)(Module*);void(*Quit)(Module*);void* userData;};
struct Header {int version;char* description;Module*(*getModule)(int);};
static std::wstring assets;
static LRESULT CALLBACK parentProc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==WM_USER){if(l==0)return 0x5050;if(l==104)return 1;if(l==1336)return (LRESULT)assets.c_str();return 0;}return DefWindowProcW(window,message,w,l);}
int wmain(int argc,wchar_t** argv){
    if(argc<4)return 2;std::filesystem::path file=std::filesystem::absolute(argv[1]);assets=std::filesystem::absolute(argv[2]).wstring();int cycles=_wtoi(argv[3]),frameCount=argc>4?_wtoi(argv[4]):600;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS|LOAD_LIBRARY_SEARCH_USER_DIRS);AddDllDirectory(file.parent_path().c_str());AddDllDirectory(std::filesystem::path(assets).parent_path().c_str());
    WNDCLASSW c{};c.lpfnWndProc=parentProc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"RendererProbeParent";RegisterClassW(&c);auto window=CreateWindowW(c.lpszClassName,L"Probe",0,0,0,0,0,nullptr,nullptr,c.hInstance,nullptr);
    for(int cycle=0;cycle<cycles;++cycle){
        auto dll=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DEFAULT_DIRS|LOAD_LIBRARY_SEARCH_USER_DIRS);if(!dll){std::cerr<<"load failed "<<GetLastError()<<'\n';return 3;}
        auto getHeader=(Header*(*)(HWND))GetProcAddress(dll,"winampVisGetHeader");auto frame=(const unsigned char*(*)(int*,int*))GetProcAddress(dll,"SuiteGetFrame");auto resize=(void(*)(int,int))GetProcAddress(dll,"SuiteSetSize");auto key=(void(*)(UINT,WPARAM,LPARAM))GetProcAddress(dll,"SuiteKey");auto getWindow=(HWND(*)())GetProcAddress(dll,"SuiteWindow");
        if(!getHeader||!frame)return 4;auto module=getHeader(window)->getModule(0);module->hwndParent=window;module->hDllInstance=dll;module->sRate=44100;module->nCh=2;if(module->Init(module))return 5;
        HWND input=getWindow?getWindow():nullptr;if(input)ShowWindow(input,SW_HIDE);
        for(int n=0;n<frameCount;++n){
            for(int ch=0;ch<2;++ch)for(int i=0;i<576;++i){float v=std::sin(float(i+n*7)*(.005f+.00003f*(n%1000))*(ch+1));module->waveformData[ch][i]=(unsigned char)(signed char)(v*127);module->spectrumData[ch][i]=(unsigned char)(std::abs(v)*255);}
            if(n%100==0){if(resize){int sizes[][2]={{320,240},{640,480},{800,448},{960,544}};resize(sizes[(n/100)%4][0],sizes[(n/100)%4][1]);}if(key){key(WM_KEYDOWN,'N',0);key(WM_KEYDOWN,'P',0);key(WM_KEYDOWN,'W',0);}else if(input){SendMessageW(input,WM_KEYDOWN,VK_SPACE,0);SendMessageW(input,WM_CHAR,' ',0);SendMessageW(input,WM_KEYUP,VK_SPACE,0);}}
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(module->Render(module))return 6;int w=0,h=0;auto pixels=frame(&w,&h);if(!pixels||w<=0||h<=0||w>8192||h>8192)return 7;volatile unsigned char sample=pixels[(size_t(w)*h-1)*4];(void)sample;
        }
        module->Quit(module);FreeLibrary(dll);MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){}
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&memory,sizeof(memory));DWORD handles;GetProcessHandleCount(GetCurrentProcess(),&handles);
        std::cout<<"{\"cycle\":"<<cycle<<",\"private_bytes\":"<<memory.PrivateUsage<<",\"handles\":"<<handles<<",\"gdi\":"<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<<",\"user\":"<<GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)<<"}"<<std::endl;
    }
    DestroyWindow(window);return 0;
}
