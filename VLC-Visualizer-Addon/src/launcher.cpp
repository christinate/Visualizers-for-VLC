#define UNICODE
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include "process.hpp"
#include "vlc_original.hpp"
#include <iostream>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    bool verify=false;
    try{
        wchar_t file[32768];GetModuleFileNameW(nullptr,file,32768);auto root=std::filesystem::path(file).parent_path();int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);std::vector<std::wstring> forwarded;
        bool scan=false;for(int n=1;n<count;++n){if(std::wstring(args[n])==L"--scan-visualizers")scan=true;else if(std::wstring(args[n])==L"--verify-original")verify=true;else forwarded.emplace_back(args[n]);}LocalFree(args);
        if(scan&&verify)throw std::runtime_error("Run --scan-visualizers and --verify-original separately");
        if(scan)return int(process::launch(root/L"visualizers/VisualizersCtl.exe",{L"scan",L"--suite",(root/L"visualizers").wstring()},true));
        auto original=root/L"vlc.visualizers.original.exe";
        vlc_original::verify(original,root/L"libvlccore.dll",file);
        if(verify){std::cout<<"Original VLC executable/version check passed\n";return 0;}
        // VLC accepts one colon-separated extra-interface list. Merge an explicit
        // list instead of allowing a later command-line option to hide our menu.
        std::wstring interfaces=L"visualizers_menu";
        auto addInterfaces=[&](const std::wstring& list){size_t begin=0;while(begin<=list.size()){auto end=list.find(L':',begin);auto name=list.substr(begin,end==std::wstring::npos?end:end-begin);if(!name.empty()&&(L":"+interfaces+L":").find(L":"+name+L":")==std::wstring::npos)interfaces+=L":"+name;if(end==std::wstring::npos)break;begin=end+1;}};
        for(size_t n=0;n<forwarded.size();){
            const auto value=forwarded[n];
            if(value.rfind(L"--extraintf=",0)==0){addInterfaces(value.substr(12));forwarded.erase(forwarded.begin()+n);}
            else if(value==L"--extraintf"&&n+1<forwarded.size()){addInterfaces(forwarded[n+1]);forwarded.erase(forwarded.begin()+n,forwarded.begin()+n+2);}
            else ++n;
        }
        forwarded.insert(forwarded.begin(),L"--extraintf="+interfaces);
        return int(process::launch(original,forwarded,true));
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(!verify)MessageBoxA(nullptr,error.what(),"VLC Visualizer Addon",MB_OK|MB_ICONERROR);return 1;}
}
