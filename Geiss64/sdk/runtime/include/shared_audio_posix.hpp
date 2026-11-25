#pragma once
#include "audio.hpp"
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <cstdlib>
#include <set>
#include <time.h>
namespace suite {
inline std::filesystem::path audioDirectory(){
    const char* root=std::getenv("XDG_RUNTIME_DIR");
    if(root&&*root)return std::filesystem::path(root)/"vlc-visualizers";
    const char* home=std::getenv("HOME");
    return std::filesystem::path(home?home:"/tmp")/".cache/vlc-visualizers/ipc";
}
inline uint64_t milliseconds(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
struct IpcLifetime {
    std::set<std::filesystem::path> files;
    ~IpcLifetime(){for(const auto& p:files)::unlink(p.c_str());}
};
inline IpcLifetime& ipcLifetime(){static IpcLifetime value;return value;}
struct SharedAudio {
    int mapping=-1;AudioRing* ring=nullptr;
    explicit SharedAudio(unsigned pid,bool create){
        auto dir=audioDirectory();std::error_code error;
        if(create){std::filesystem::create_directories(dir,error);if(error)return;}
        auto file=dir/(std::to_string(getuid())+"-"+std::to_string(pid)+".pcm");
        mapping=::open(file.c_str(),(create?(O_RDWR|O_CREAT):O_RDONLY)|O_CLOEXEC|O_NOFOLLOW,0600);
        if(mapping<0)return;
        struct stat info{};
        if(fstat(mapping,&info)||!S_ISREG(info.st_mode)||info.st_uid!=getuid()){::close(mapping);mapping=-1;return;}
        if(create){if(ftruncate(mapping,sizeof(AudioRing))){::close(mapping);mapping=-1;return;}ipcLifetime().files.insert(file);}
        else if(info.st_size!=sizeof(AudioRing)){::close(mapping);mapping=-1;return;}
        auto view=mmap(nullptr,sizeof(AudioRing),create?(PROT_READ|PROT_WRITE):PROT_READ,MAP_SHARED,mapping,0);
        if(view!=MAP_FAILED)ring=static_cast<AudioRing*>(view);
    }
    SharedAudio(const SharedAudio&)=delete;
    ~SharedAudio(){if(ring)munmap(ring,sizeof(AudioRing));if(mapping>=0)::close(mapping);}
    bool lock(){return mapping>=0&&flock(mapping,LOCK_EX|LOCK_NB)==0;}
    void unlock(){flock(mapping,LOCK_UN);}
};
}
