#pragma once
#ifdef _WIN32
#include <windows.h>
#include <string>
#include "audio.hpp"
namespace suite {
struct SharedAudio {
    HANDLE mapping=nullptr, mutex=nullptr; AudioRing* ring=nullptr;
    explicit SharedAudio(DWORD pid, bool create) {
        auto name=L"Local\\VLCVisualizers.Audio."+std::to_wstring(pid);
        mutex=CreateMutexW(nullptr,FALSE,(name+L".Lock").c_str());
        mapping=create ? CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(AudioRing),name.c_str())
                       : OpenFileMappingW(FILE_MAP_READ,FALSE,name.c_str());
        if(mapping)ring=static_cast<AudioRing*>(MapViewOfFile(mapping,create?FILE_MAP_WRITE:FILE_MAP_READ,0,0,sizeof(AudioRing)));
    }
    ~SharedAudio(){if(ring)UnmapViewOfFile(ring);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);}
    bool lock(){if(!mutex)return false;auto r=WaitForSingleObject(mutex,0);return r==WAIT_OBJECT_0||r==WAIT_ABANDONED;}
    void unlock(){ReleaseMutex(mutex);}
};
}
#else
#include "shared_audio_posix.hpp"
#endif
