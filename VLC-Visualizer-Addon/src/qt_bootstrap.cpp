// SPDX-License-Identifier: GPL-2.0-or-later
// VLC searches System32 for plugin dependencies. This dependency-free module
// loads our private Qt interface with DLL_LOAD_DIR, outside the DLL loader lock.
// The backend registers VLC's original Qt interface and cannot-unload flag.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
using SetCallback=int(__cdecl *)(void*,void*,int,...);
using Entry=int(__cdecl *)(SetCallback,void*);
static HMODULE backend=nullptr;
extern "C" __declspec(dllexport) int __cdecl vlc_entry__3_0_0f(SetCallback callback,void* opaque){
    if(!backend){
        HMODULE self=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&vlc_entry__3_0_0f),&self))return -1;
        wchar_t file[32768];DWORD length=GetModuleFileNameW(self,file,32768);if(!length||length>=32768)return -1;
        std::wstring path(file);for(int n=0;n<3;++n){auto end=path.find_last_of(L"\\/");if(end==std::wstring::npos)return -1;path.resize(end);}
        path+=L"\\visualizers\\qt\\bin\\libqt_backend.dll";
        backend=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!backend)return -1;
    }
    auto entry=reinterpret_cast<Entry>(GetProcAddress(backend,"vlc_entry__3_0_0f"));return entry?entry(callback,opaque):-1;
}
extern "C" __declspec(dllexport) const char* __cdecl vlc_entry_license__3_0_0f(){return "GPL-2.0-or-later";}
extern "C" __declspec(dllexport) const char* __cdecl vlc_entry_copyright__3_0_0f(){return "Copyright (C) VideoLAN and VLC Visualizers contributors";}
