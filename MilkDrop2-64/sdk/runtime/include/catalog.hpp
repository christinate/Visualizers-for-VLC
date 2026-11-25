#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "native_manifest.hpp"
namespace catalog {
namespace fs=std::filesystem;
using json=nlohmann::json;
// Preserve long package/license paths without changing Windows system policy.
inline fs::path extended(const fs::path& path){
    auto value=fs::absolute(path).lexically_normal().make_preferred().wstring();
    if(value.rfind(L"\\\\?\\",0)==0)return fs::path(value);
    if(value.rfind(L"\\\\",0)==0)return fs::path(L"\\\\?\\UNC\\"+value.substr(2));
    return fs::path(L"\\\\?\\"+value);
}
inline std::wstring wide(const std::string& value){if(value.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n);return out;}
inline std::string utf8(const std::wstring& value){if(value.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid Unicode");std::string out(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
inline bool idValid(const std::string& value){return !value.empty()&&value.size()<=64&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos;}
inline fs::path contained(const fs::path& root,const fs::path& relative){
    if(relative.native().find(L'\0')!=std::wstring::npos)throw std::runtime_error("Package path contains NUL");
    if(relative.empty()||relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Expected a relative path");
    for(const auto& component:relative)if(component==L"..")throw std::runtime_error("Path escapes its package");
    auto joined=root/relative;joined.make_preferred();
    auto canonicalRoot=fs::weakly_canonical(root),target=fs::weakly_canonical(joined);
    auto r=canonicalRoot.begin(),t=target.begin();for(;r!=canonicalRoot.end();++r,++t)if(t==target.end()||_wcsicmp(r->c_str(),t->c_str()))throw std::runtime_error("Path escapes its package");
    // weakly_canonical normalizes away the Win32 extended-path prefix.
    // Preserve the caller's namespace after the containment check.
    return root.native().rfind(L"\\\\?\\",0)==0?extended(target):target;
}
inline bool amd64(const fs::path& file){std::ifstream in(file,std::ios::binary);IMAGE_DOS_HEADER dos{};in.read((char*)&dos,sizeof(dos));if(!in||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>16*1024*1024)return false;in.seekg(dos.e_lfanew);DWORD signature=0;IMAGE_FILE_HEADER pe{};in.read((char*)&signature,4);in.read((char*)&pe,sizeof(pe));return in&&signature==IMAGE_NT_SIGNATURE&&pe.Machine==IMAGE_FILE_MACHINE_AMD64;}
inline json read(const fs::path& file){if(fs::file_size(file)>1024*1024)throw std::runtime_error("JSON exceeds 1 MiB");std::ifstream in(file);return json::parse(in);}
inline void atomicWrite(const fs::path& file,const json& value){
    fs::create_directories(file.parent_path());auto temporary=file;temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId());
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<value.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write catalog");}
    if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){fs::remove(temporary);throw std::runtime_error("Cannot atomically replace catalog");}
}
inline json validate(const fs::path& suite,const fs::path& package){
    auto m=read(package/L"manifest.json");auto id=m.at("id").get<std::string>();
    const bool web=m.at("schema_version")==3&&m.at("api_version")==3&&m.at("adapter")=="webgpu";
    const bool dawn=m.at("adapter")=="dawn";
    if(dawn){if(m.at("platform")!="windows")throw std::runtime_error("Unsupported native package platform");suite::validateNativeManifest(m,package,[](const fs::path& p,const std::string& s){return contained(p,wide(s));});}
    if(web&&m.value("platform",std::string("windows"))!="windows")throw std::runtime_error("Unsupported package platform");
    if((!web&&!dawn&&(m.at("schema_version")!=1||m.at("api_version")!=1))||!idValid(id)||package.filename()!=wide(id)||m.at("architecture")!="x86_64")throw std::runtime_error("Unsupported manifest or package ID");
    auto name=m.at("name").get<std::string>(),adapter=m.at("adapter").get<std::string>();
    if(name.empty()||name.size()>128||(adapter!="geiss"&&adapter!="winamp"&&adapter!="webgpu"&&adapter!="dawn"))throw std::runtime_error("Invalid name or adapter");
    auto dll=contained(package,wide(m.at("entrypoint").get<std::string>()));
    if(web){if(dll.extension()!=L".html"||!fs::is_regular_file(dll)||fs::file_size(dll)>1048576)throw std::runtime_error("WebGPU entrypoint is missing or invalid");}
    else if(!amd64(dll))throw std::runtime_error("Renderer is absent or not AMD64");
    if(!m.contains("version")||!m["version"].is_string())throw std::runtime_error("Missing version");
    m["package"]=utf8(fs::relative(package,suite).generic_wstring());return m;
}
inline json scan(const fs::path& suite){
    fs::create_directories(suite);
    struct ScanLock {HANDLE file=INVALID_HANDLE_VALUE;OVERLAPPED overlap{};~ScanLock(){if(file!=INVALID_HANDLE_VALUE){UnlockFileEx(file,0,MAXDWORD,MAXDWORD,&overlap);CloseHandle(file);}}} lock;
    lock.file=CreateFileW((suite/L".scan.lock").c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock.file==INVALID_HANDLE_VALUE||!LockFileEx(lock.file,LOCKFILE_EXCLUSIVE_LOCK,0,MAXDWORD,MAXDWORD,&lock.overlap))throw std::runtime_error("Cannot lock the installed catalog for scanning");
    json out={{"schema_version",1},{"visualizers",json::array()},{"rejected",json::array()}};
    auto folder=suite/L"plugins";if(fs::exists(folder))for(const auto& entry:fs::directory_iterator(folder)){
        if(!entry.is_directory())continue;
        try{auto package=contained(suite,fs::path(L"plugins")/entry.path().filename());out["visualizers"].push_back(validate(suite,package));}catch(const std::exception& error){
            std::string name="<invalid Unicode package name>";try{name=utf8(entry.path().filename().wstring());}catch(const std::exception&){}
            out["rejected"].push_back({{"package",suite::catalogDiagnostic<json>(name)},{"reason",suite::catalogDiagnostic<json>(error.what())}});
        }
        if(out["visualizers"].size()+out["rejected"].size()>256)throw std::runtime_error("Too many visualizer packages");
    }
    std::sort(out["visualizers"].begin(),out["visualizers"].end(),[](const auto& a,const auto& b){return a.at("id")<b.at("id");});
    atomicWrite(suite/L"installed-visualizers.json",out);return out;
}
inline json installed(const fs::path& suite){
    if(!fs::exists(suite/L"installed-visualizers.json"))return {{"schema_version",1},{"visualizers",json::array()}};
    auto cache=read(suite/L"installed-visualizers.json");if(cache.at("schema_version")!=1||!cache.at("visualizers").is_array())throw std::runtime_error("Invalid installed catalog");return cache;
}
inline fs::path packageFor(const fs::path& suite,const json& entry){auto package=contained(suite,wide(entry.at("package").get<std::string>()));auto now=validate(suite,package);if(now.at("id")!=entry.at("id"))throw std::runtime_error("Package changed since scan");return package;}
}
