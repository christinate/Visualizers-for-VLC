// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <winver.h>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
namespace vlc_original {
struct Version {DWORD high,low,type;bool vlcFilename=false;};
inline Version version(const std::filesystem::path& file){
    DWORD ignored=0,size=GetFileVersionInfoSizeW(file.c_str(),&ignored);
    if(!size||size>16*1024*1024)throw std::runtime_error("Original VLC version metadata is missing or invalid; repair the addon installation");
    std::vector<char> bytes(size);
    if(!GetFileVersionInfoW(file.c_str(),0,size,bytes.data()))throw std::runtime_error("Cannot read original VLC version metadata");
    VS_FIXEDFILEINFO* info=nullptr;UINT length=0;
    if(!VerQueryValueW(bytes.data(),L"\\",reinterpret_cast<void**>(&info),&length)||length<sizeof(*info)||info->dwSignature!=VS_FFI_SIGNATURE)
        throw std::runtime_error("Original VLC version metadata is invalid");
    Version result{info->dwFileVersionMS,info->dwFileVersionLS,info->dwFileType};
    struct Translation {WORD language,codepage;};Translation* languages=nullptr;
    if(VerQueryValueW(bytes.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&languages),&length)&&length<=64*sizeof(Translation)){
        for(UINT n=0;n<length/sizeof(Translation);++n){
            wchar_t key[96];swprintf_s(key,L"\\StringFileInfo\\%04x%04x\\OriginalFilename",languages[n].language,languages[n].codepage);
            wchar_t* name=nullptr;UINT count=0;
            if(VerQueryValueW(bytes.data(),key,reinterpret_cast<void**>(&name),&count)&&count==8&&name[7]==L'\0'&&
               _wcsicmp(name,L"vlc.exe")==0)result.vlcFilename=true;
        }
    }
    return result;
}
inline void verify(const std::filesystem::path& original,const std::filesystem::path& core,const std::filesystem::path& launcher){
    if(!std::filesystem::is_regular_file(original))throw std::runtime_error("Original VLC executable is missing; repair the addon installation");
    if(std::filesystem::equivalent(original,launcher))throw std::runtime_error("The VLC backup points to the addon launcher; repair the addon installation");
    const auto executable=version(original),library=version(core);
    if(!executable.vlcFilename||executable.type!=VFT_APP||HIWORD(executable.high)!=3||library.type!=VFT_DLL||
       executable.high!=library.high||executable.low!=library.low)
        throw std::runtime_error("The VLC backup is not a matching VLC 3 executable; repair the addon installation");
}
}
