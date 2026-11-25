// SPDX-License-Identifier: LGPL-2.1-or-later
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include "shared_audio.hpp"
struct filter_sys_t {suite::SharedAudio shared{unsigned(getpid()),true};};
static block_t* filterAudio(filter_t* filter,block_t* block){if(!block)return nullptr;auto& shared=filter->p_sys->shared;auto channels=filter->fmt_in.audio.i_channels;if(shared.ring&&channels&&block->i_nb_samples<=block->i_buffer/(sizeof(float)*channels)&&shared.lock()){suite::write(*shared.ring,reinterpret_cast<float*>(block->p_buffer),block->i_nb_samples,channels,filter->fmt_in.audio.i_rate,suite::milliseconds());shared.unlock();}return block;}
static int openAudio(vlc_object_t* object){auto filter=reinterpret_cast<filter_t*>(object);filter->fmt_in.audio.i_format=VLC_CODEC_FL32;filter->fmt_out.audio=filter->fmt_in.audio;auto state=new(std::nothrow)filter_sys_t;if(!state)return VLC_ENOMEM;if(!state->shared.ring){delete state;return VLC_EGENERIC;}filter->p_sys=state;filter->pf_audio_filter=filterAudio;return VLC_SUCCESS;}
static void closeAudio(vlc_object_t* object){delete reinterpret_cast<filter_t*>(object)->p_sys;}
extern "C" int NativeMenuOpen(vlc_object_t*);
extern "C" void NativeMenuClose(vlc_object_t*);
vlc_module_begin()
 set_shortname("Visualizers audio bridge")
 set_description("Native PCM transport for VLC Visualizers")
 set_capability("audio filter",0)
 set_category(CAT_AUDIO)
 set_subcategory(SUBCAT_AUDIO_AFILTER)
 add_shortcut("vlc_visualizers")
 set_callbacks(openAudio,closeAudio)
 add_submodule()
 set_shortname("Visualizers menu")
 set_description("Visualizers in VLC's native menu")
 set_capability("interface",0)
 add_shortcut("visualizers_menu")
 set_callbacks(NativeMenuOpen,NativeMenuClose)
 cannot_unload_broken_library()
vlc_module_end()
