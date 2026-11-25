#define UNICODE
#include <windows.h>
#include <iostream>
#include <cmath>
#include <string_view>
#include "shared_audio.hpp"
struct Module {char* description;HWND parent;HINSTANCE dll;int rate,channels,latency,delay,spectrumChannels,waveChannels;unsigned char spectrum[2][576],wave[2][576];void(*config)(Module*);int(*init)(Module*);int(*render)(Module*);void(*quit)(Module*);void* data;};
struct Header {int version;char* description;Module*(*get)(int);};
int wmain(int argc,wchar_t** argv){
    const bool configure=argc==3&&std::wstring_view(argv[2])==L"--configure";
    if(argc!=2&&!configure)return 2;
    auto dll=LoadLibraryW(argv[1]);if(!dll)return 3;
    auto fn=(Header*(*)(HWND))GetProcAddress(dll,"winampVisGetHeader");if(!fn)return 4;
    auto header=fn(nullptr);if(header->version!=0x101)return 5;
    auto module=header->get(0);module->dll=dll;
    if(configure){
        if(!module->config)return 9;
        module->config(module);FreeLibrary(dll);return 0;
    }
    module->rate=48000;module->channels=2;if(module->init(module))return 6;
    for(int n=0;n<350;++n){
        for(int ch=0;ch<2;++ch)for(int i=0;i<576;++i)module->wave[ch][i]=(unsigned char)(signed char)(90*std::sin((i+n*4)*.07));
        if(module->render(module))return 7;
        Sleep(5);
    }
    suite::SharedAudio audio(GetCurrentProcessId(),false);
    if(!audio.ring||audio.ring->magic!=suite::audioMagic||audio.ring->rate!=48000||audio.ring->written<576)return 8;
    module->quit(module);FreeLibrary(dll);
    std::cout<<"PASS: Winamp ABI callbacks published 48 kHz audio from a "<<(sizeof(void*)*8)<<"-bit bridge\n";
    return 0;
}
