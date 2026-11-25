// SPDX-License-Identifier: LGPL-2.1-or-later
// The menu is built into VLC's Qt interface. This service only enables PCM.
#include <vlc_common.h>
#include <vlc_interface.h>
#include <thread>
struct intf_sys_t {std::thread worker;};
using EnableFilter=void(*)(playlist_t*,const char*,bool);
extern "C" int VisualizersMenuOpen(vlc_object_t* object){
    auto intf=reinterpret_cast<intf_thread_t*>(object);
    auto enable=reinterpret_cast<EnableFilter>(GetProcAddress(GetModuleHandleW(L"libvlccore.dll"),"playlist_EnableAudioFilter"));
    if(!enable)return VLC_EGENERIC;
    auto sys=new(std::nothrow)intf_sys_t;if(!sys)return VLC_ENOMEM;
    try{sys->worker=std::thread([intf,enable]{enable(pl_Get(intf),"vlc_visualizers",true);});}catch(...){delete sys;return VLC_ENOMEM;}
    intf->p_sys=sys;return VLC_SUCCESS;
}
extern "C" void VisualizersMenuClose(vlc_object_t* object){
    auto intf=reinterpret_cast<intf_thread_t*>(object);auto sys=intf->p_sys;if(!sys)return;
    if(sys->worker.joinable())sys->worker.join();delete sys;intf->p_sys=nullptr;
}
