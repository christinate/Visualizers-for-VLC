// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_cache.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

namespace visualizers::gpu {
namespace {
constexpr uint64_t seed=14695981039346656037ull;
constexpr size_t maxBlob=48*1024*1024;
uint64_t hash(std::span<const uint8_t> bytes,uint64_t value=seed){
    for(auto byte:bytes)value=(value^byte)*1099511628211ull;
    return value;
}
std::span<const uint8_t> bytes(std::string_view value){
    return {reinterpret_cast<const uint8_t*>(value.data()),value.size()};
}
uint64_t fileHash(const std::filesystem::path& path,uint64_t value){
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read Beach shader cache inputs");
    std::array<uint8_t,16384> buffer{};
    while(file.read(reinterpret_cast<char*>(buffer.data()),buffer.size())||file.gcount())
        value=hash({buffer.data(),size_t(file.gcount())},value);
    if(file.bad())throw std::runtime_error("Cannot hash Beach shader cache inputs");
    return value;
}
const auto marker=".complete";
}
ShaderCache::ShaderCache(const std::filesystem::path& profile,const std::filesystem::path& module,
                         const std::filesystem::path& shader):root_(profile/"beach-shaders"){
    buildKey_=std::to_string(fileHash(shader,fileHash(module,hash(bytes("Beach shader cache 1")))));
}
void ShaderCache::attach(WGPUDawnCacheDeviceDescriptor& descriptor,const std::string& adapter){
    directory_=root_/(buildKey_+"-"+std::to_string(hash(bytes(adapter))));
    std::error_code error;
    std::filesystem::create_directories(directory_,error);
    if(error){directory_.clear();return;}
    descriptor.isolationKey={"Beach native renderer 1",WGPU_STRLEN};
    descriptor.dawnLoadCacheDataCallbackInfo.userdata1=this;
    descriptor.dawnLoadCacheDataCallbackInfo.callback=[](size_t keySize,const uint8_t* key,size_t size,
                                                         uint8_t* value,void* user,void*)noexcept{
        return load(keySize,key,size,value,static_cast<ShaderCache*>(user));
    };
    descriptor.dawnStoreCacheDataCallbackInfo.userdata1=this;
    descriptor.dawnStoreCacheDataCallbackInfo.callback=[](size_t keySize,const uint8_t* key,size_t size,
                                                           const uint8_t* value,void* user,void*)noexcept{
        store(keySize,key,size,value,static_cast<ShaderCache*>(user));
    };
}
size_t ShaderCache::load(size_t keySize,const uint8_t* key,size_t length,uint8_t* value,ShaderCache* cache)noexcept{
    try{
        std::lock_guard lock(cache->mutex_);
        const auto path=cache->directory_/(std::to_string(hash({key,keySize}))+".blob");
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        const auto size=file.tellg();
        if(!file||size<=0||size>std::streamoff(maxBlob))return 0;
        const size_t count=size_t(size);
        if(!value)return count;
        if(length!=count)return 0;
        file.seekg(0);
        if(!file.read(reinterpret_cast<char*>(value),std::streamsize(count)))return 0;
        ++cache->loads_;
        return count;
    }catch(...){return 0;}
}
void ShaderCache::store(size_t keySize,const uint8_t* key,size_t length,const uint8_t* value,ShaderCache* cache)noexcept{
    if(!length||length>maxBlob)return;
    try{
        std::lock_guard lock(cache->mutex_);
        const auto path=cache->directory_/(std::to_string(hash({key,keySize}))+".blob");
        static std::atomic<uint64_t> sequence{0};
        auto temp=path;
        temp+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++);
        struct Cleanup {std::filesystem::path file;~Cleanup(){std::error_code ignored;std::filesystem::remove(file,ignored);}} cleanup{temp};
        std::ofstream file(temp,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(value),std::streamsize(length));
        file.close();
        if(!file)return;
        std::error_code error;
        std::filesystem::rename(temp,path,error);
        if(!error)++cache->stores_;
    }catch(...){}
}
bool ShaderCache::ready()const{
    if(directory_.empty())return false;
    std::error_code error;
    if(!std::filesystem::is_regular_file(directory_/marker,error))return false;
    for(std::filesystem::directory_iterator it(directory_,error),end;!error&&it!=end;it.increment(error)){
        if(it->path().extension()==".blob"&&it->file_size(error)>0&&!error)return true;
    }
    return false;
}
void ShaderCache::complete()noexcept{
    try{
        std::lock_guard lock(mutex_);
        if(!directory_.empty()&&(stores_||loads_))std::ofstream(directory_/marker)<<"Beach shaders ready\n";
    }catch(...){}
}
unsigned ShaderCache::hits()const{std::lock_guard lock(mutex_);return loads_;}
unsigned ShaderCache::writes()const{std::lock_guard lock(mutex_);return stores_;}
}
