// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace windows_install {
namespace fs=std::filesystem;
using Handle=std::unique_ptr<void,decltype(&CloseHandle)>;

class TransactionLock {
    Handle mutex_{CreateMutexW(nullptr,FALSE,L"Global\\VLCVisualizersSetup"),CloseHandle};
public:
    TransactionLock(){
        if(!mutex_)throw std::runtime_error("Cannot lock VLC Visualizer Addon setup");
        const auto status=WaitForSingleObject(mutex_.get(),0);
        if(status!=WAIT_OBJECT_0&&status!=WAIT_ABANDONED)
            throw std::runtime_error("Another VLC Visualizer Addon install or removal is in progress");
    }
    ~TransactionLock(){ReleaseMutex(mutex_.get());}
};

struct IconCopy {
    HANDLE update;
    bool failed=false;
    static BOOL CALLBACK language(HMODULE from,LPCWSTR type,LPCWSTR name,WORD lang,LONG_PTR value){
        auto& context=*reinterpret_cast<IconCopy*>(value);
        const auto resource=FindResourceExW(from,type,name,lang);
        const auto data=resource?LoadResource(from,resource):nullptr;
        const auto size=resource?SizeofResource(from,resource):0;
        const auto block=data?LockResource(data):nullptr;
        if(!block||!size||!UpdateResourceW(context.update,type,name,lang,block,size)){
            context.failed=true;
            return FALSE;
        }
        return TRUE;
    }
    static BOOL CALLBACK name(HMODULE from,LPCWSTR type,LPWSTR name,LONG_PTR value){
        return EnumResourceLanguagesW(from,type,name,language,value);
    }
};

inline void copyIcon(const fs::path& original,const fs::path& destination){
    using Module=std::unique_ptr<std::remove_pointer_t<HMODULE>,decltype(&FreeLibrary)>;
    Module from(LoadLibraryExW(original.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE),FreeLibrary);
    if(!from)throw std::runtime_error("Cannot read the original VLC icon");
    IconCopy icons{BeginUpdateResourceW(destination.c_str(),FALSE)};
    if(!icons.update)throw std::runtime_error("Cannot stage the VLC launcher icon");
    for(auto type:{MAKEINTRESOURCEW(3),MAKEINTRESOURCEW(14)}){
        if(!EnumResourceNamesW(from.get(),type,IconCopy::name,reinterpret_cast<LONG_PTR>(&icons))&&
            GetLastError()!=ERROR_RESOURCE_TYPE_NOT_FOUND)icons.failed=true;
    }
    if(!EndUpdateResourceW(icons.update,icons.failed)||icons.failed)
        throw std::runtime_error("Cannot preserve the original VLC launcher icon");
}

struct Player {DWORD id;Handle handle;};
inline std::vector<Player> runningPlayers(const fs::path& root,bool force=false){
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot check whether VLC is running");
    Handle processes(snapshot,CloseHandle);
    PROCESSENTRY32W row{};row.dwSize=sizeof(row);
    std::vector<Player> players;
    if(!Process32FirstW(snapshot,&row))throw std::runtime_error("Cannot read the process list");
    do{
        if(_wcsicmp(row.szExeFile,L"vlc.exe")&&_wcsicmp(row.szExeFile,L"vlc.visualizers.original.exe"))continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,row.th32ProcessID),CloseHandle);
        if(!process)continue;
        std::vector<wchar_t> name(32768);
        DWORD length=static_cast<DWORD>(name.size());
        if(!QueryFullProcessImageNameW(process.get(),0,name.data(),&length))continue;
        const auto executable=fs::path(name.data()).lexically_normal();
        const auto parent=executable.parent_path();
        std::error_code error;
        if(!fs::equivalent(parent,root,error))continue;
        if(force){
            Handle writable(OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,row.th32ProcessID),CloseHandle);
            if(!writable)throw std::runtime_error("Cannot close VLC. Exit it and retry setup.");
            process=std::move(writable);
        }
        players.push_back({row.th32ProcessID,std::move(process)});
    }while(Process32NextW(snapshot,&row));
    return players;
}

inline BOOL CALLBACK requestClose(HWND window,LPARAM value){
    const auto& players=*reinterpret_cast<const std::vector<Player>*>(value);
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    for(const auto& player:players)if(player.id==pid){PostMessageW(window,WM_CLOSE,0,0);break;}
    return TRUE;
}

inline void closePlayers(const fs::path& root,bool force){
    const auto players=runningPlayers(root,force);
    if(players.empty())return;
    EnumWindows(requestClose,reinterpret_cast<LPARAM>(&players));
    const auto deadline=GetTickCount64()+8000;
    for(const auto& player:players){
        const auto now=GetTickCount64();
        if(WaitForSingleObject(player.handle.get(),DWORD(now<deadline?deadline-now:0))==WAIT_OBJECT_0)continue;
        if(!force||!TerminateProcess(player.handle.get(),0)||
           WaitForSingleObject(player.handle.get(),3000)!=WAIT_OBJECT_0)
            throw std::runtime_error("VLC is still running. Close it and retry setup.");
    }
}
} // namespace windows_install
