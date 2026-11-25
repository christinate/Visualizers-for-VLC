// SPDX-License-Identifier: LGPL-2.1-or-later
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include "shared_audio.hpp"
struct filter_sys_t { suite::SharedAudio shared{GetCurrentProcessId(),true}; };
static block_t* Filter(filter_t* filter, block_t* block) {
    if(!block)return nullptr;
    auto& shared=filter->p_sys->shared;
    const unsigned channels=filter->fmt_in.audio.i_channels;
    if(shared.ring && channels && block->i_nb_samples<=block->i_buffer/(sizeof(float)*channels) && shared.lock()) {
        suite::write(*shared.ring,reinterpret_cast<const float*>(block->p_buffer),block->i_nb_samples,channels,filter->fmt_in.audio.i_rate,GetTickCount64());
        shared.unlock();
    }
    return block; // VLC still owns the unchanged audio and sends it to its output.
}
static int Open(vlc_object_t* object) {
    auto filter=reinterpret_cast<filter_t*>(object);
    filter->fmt_in.audio.i_format=VLC_CODEC_FL32;
    filter->fmt_out.audio=filter->fmt_in.audio;
    auto sys=new(std::nothrow) filter_sys_t;
    if(!sys)return VLC_ENOMEM;
    if(!sys->shared.ring){delete sys;return VLC_EGENERIC;}
    filter->p_sys=sys;filter->pf_audio_filter=Filter;return VLC_SUCCESS;
}
static void Close(vlc_object_t* object){auto f=reinterpret_cast<filter_t*>(object);delete f->p_sys;}
static int OpenVisual(vlc_object_t* object){
    int result=Open(object);if(result!=VLC_SUCCESS)return result;
    HMODULE own=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&OpenVisual),&own);
    wchar_t path[32768];GetModuleFileNameW(own,path,32768);
    std::wstring directory=path;directory.resize(directory.find_last_of(L"\\/"));
    std::wstring root=directory.substr(0,directory.find_last_of(L"\\/"));root=root.substr(0,root.find_last_of(L"\\/"));
    std::wstring host=root+L"\\visualizers\\Visualizers.exe";
    if(GetFileAttributesW(host.c_str())==INVALID_FILE_ATTRIBUTES)host=root+L"\\Visualizers.exe";
    std::wstring command=L"\""+host+L"\" --vlc-pid "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)){Close(object);return VLC_EGENERIC;}
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return VLC_SUCCESS;
}
extern "C" int VisualizersMenuOpen(vlc_object_t*);
extern "C" void VisualizersMenuClose(vlc_object_t*);
vlc_module_begin()
    set_shortname("Visualizers audio bridge")
    set_description("PCM audio bridge for the modular Visualizers suite")
    set_capability("audio filter",0)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    add_shortcut("vlc_visualizers")
    set_callbacks(Open,Close)
    add_submodule()
    set_shortname("Visualizers")
    set_description("Visualizers")
    set_capability("visualization",0)
    add_shortcut("visualizers")
    set_callbacks(OpenVisual,Close)
    add_submodule()
    set_shortname("Visualizers menu")
    set_description("VLC Visualizer Addon native menu")
    set_capability("interface",0)
    add_shortcut("visualizers_menu")
    set_callbacks(VisualizersMenuOpen,VisualizersMenuClose)
    cannot_unload_broken_library()
vlc_module_end()
