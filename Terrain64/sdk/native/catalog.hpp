#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "../runtime/include/native_manifest.hpp"
namespace native {
namespace fs=std::filesystem;using json=nlohmann::json;
inline const char* platform(){
#ifdef __APPLE__
    return "macos";
#else
    return "linux";
#endif
}
inline const char* architecture(){
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#else
    return "x86_64";
#endif
}
inline fs::path contained(const fs::path& root,const fs::path& relative){
    if(relative.native().find('\0')!=std::string::npos)throw std::runtime_error("Package path contains NUL");
    if(relative.empty()||relative.is_absolute())throw std::runtime_error("Expected relative package path");
    for(const auto& part:relative)if(part=="..")throw std::runtime_error("Path escapes package");
    const auto base=fs::weakly_canonical(root),result=fs::weakly_canonical(root/relative);
    auto r=base.begin(),p=result.begin();for(;r!=base.end();++r,++p)if(p==result.end()||*r!=*p)throw std::runtime_error("Path escapes package");
    // Verify resolved containment, retaining framework symlinks for installers.
    return fs::absolute(root/relative).lexically_normal();
}
inline json read(const fs::path& file){if(fs::file_size(file)>1048576)throw std::runtime_error("JSON exceeds 1 MiB");std::ifstream stream(file);return json::parse(stream);}
inline void write(const fs::path& file,const json& data){
    fs::create_directories(file.parent_path());auto temporary=file;temporary+=".tmp."+std::to_string(getpid());
    {std::ofstream out(temporary,std::ios::trunc);out<<data.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write JSON");}
    fs::rename(temporary,file);
}
inline bool binaryMatches(const fs::path& file){
    unsigned char header[32]{};std::ifstream in(file,std::ios::binary);in.read((char*)header,sizeof(header));if(!in)return false;
#ifdef __APPLE__
    uint32_t magic=0,cpu=0;std::memcpy(&magic,header,4);std::memcpy(&cpu,header+4,4);
    return magic==0xfeedfacf && cpu==(std::string(architecture())=="arm64"?0x0100000cu:0x01000007u);
#else
    return header[0]==0x7f&&header[1]=='E'&&header[2]=='L'&&header[3]=='F'&&header[4]==2&&header[5]==1&&
        (uint16_t(header[18])|(uint16_t(header[19])<<8))==(std::string(architecture())=="arm64"?183:62);
#endif
}
inline json validate(const fs::path& suite,const fs::path& package){
    auto value=read(package/"manifest.json");const auto id=value.at("id").get<std::string>();
    if(id.empty()||id.size()>64||id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos||package.filename()!=id)throw std::runtime_error("Invalid package ID");
    const bool web=value.at("schema_version")==3&&value.at("api_version")==3&&value.at("adapter")=="webgpu";
    const bool dawn=value.at("adapter")=="dawn";
    if(dawn)suite::validateNativeManifest(value,package,[](const fs::path& p,const std::string& s){return contained(p,s);});
    if((!web&&!dawn&&(value.at("schema_version")!=2||value.at("api_version")!=2||value.at("adapter")!="opengl"))||value.at("platform")!=platform()||value.at("architecture")!=architecture())throw std::runtime_error("Incompatible package ABI/platform/architecture");
    auto name=value.at("name").get<std::string>();if(name.empty()||name.size()>128||!value.at("version").is_string())throw std::runtime_error("Invalid package name/version");
    auto library=contained(package,value.at("entrypoint").get<std::string>());
    if(web){if(library.extension()!=".html"||!fs::is_regular_file(library)||fs::file_size(library)>1048576)throw std::runtime_error("WebGPU entrypoint is missing or invalid");}
    else if(!binaryMatches(library))throw std::runtime_error("Native renderer is missing or incompatible");
    value["package"]=fs::relative(package,suite).generic_string();return value;
}
inline json scan(const fs::path& suite){
    fs::create_directories(suite);
    struct Lock{int fd;~Lock(){if(fd>=0){flock(fd,LOCK_UN);close(fd);}}} lock{open((suite/".scan.lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600)};
    if(lock.fd<0||flock(lock.fd,LOCK_EX))throw std::runtime_error("Cannot lock catalog");
    json result={{"schema_version",1},{"visualizers",json::array()},{"rejected",json::array()}};
    auto plugins=suite/"plugins";if(fs::exists(plugins))for(const auto& entry:fs::directory_iterator(plugins)){
        if(!entry.is_directory())continue;
        try{auto package=contained(suite,fs::path("plugins")/entry.path().filename());result["visualizers"].push_back(validate(suite,package));}
        catch(const std::exception& e){result["rejected"].push_back({{"package",suite::catalogDiagnostic<json>(entry.path().filename().string())},{"reason",suite::catalogDiagnostic<json>(e.what())}});}
        if(result["visualizers"].size()+result["rejected"].size()>256)throw std::runtime_error("Too many packages");
    }
    std::sort(result["visualizers"].begin(),result["visualizers"].end(),[](const auto& a,const auto& b){return a.at("id")<b.at("id");});
    write(suite/"installed-visualizers.json",result);return result;
}
inline fs::path defaultProfile(){const char* user=getenv("HOME");
#ifdef __APPLE__
    return fs::path(user?user:".")/"Library/Application Support/VLCVisualizers/profiles";
#else
    const char* config=getenv("XDG_CONFIG_HOME");return (config&&*config?fs::path(config):fs::path(user?user:".")/".config")/"vlc-visualizers/profiles";
#endif
}
}
