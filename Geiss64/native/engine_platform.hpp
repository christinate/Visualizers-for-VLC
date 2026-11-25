#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <cstddef>
#define PLUGIN 1
#define GRFX 0
#define far
#define TRUE 1
#define FALSE 0
#define CALLBACK
#define WINAPI
#define MAX_PATH 260
using BOOL=int;using UINT=unsigned;using DWORD=uint32_t;using WORD=uint16_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
using HRESULT=int32_t;using std::ptrdiff_t;
using HWND=void*;using HINSTANCE=void*;using HFONT=void*;using HCURSOR=void*;using HDC=void*;using HBITMAP=void*;
using LPDIRECTDRAW=void*;using LPDIRECTDRAWSURFACE=void*;using LPDIRECTDRAWPALETTE=void*;
struct GUID {uint32_t a;uint16_t b,c;unsigned char d[8];};using LPGUID=GUID*;
struct RECT{int32_t left,top,right,bottom;};struct WINDOWPLACEMENT{int unused;};struct LARGE_INTEGER{int64_t QuadPart;};
struct PALETTEENTRY{unsigned char peRed,peGreen,peBlue,peFlags;};
template<class A,class B> inline auto min(A a,B b)->decltype(a+b){return a<b?a:b;}
template<class A,class B> inline auto max(A a,B b)->decltype(a+b){return a>b?a:b;}
inline uint32_t timeGetTime(){return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
inline void* SetCursor(void*){return nullptr;}
inline int DeleteObject(void*){return 1;}
inline void GetWindowsPath(){}
inline void dumpmsg(const char*,...){}
inline unsigned GetPrivateProfileInt(const char*,const char*,int fallback,const char*){return fallback;}
inline unsigned GetPrivateProfileString(const char*,const char*,const char* fallback,char* output,unsigned capacity,const char*){if(!capacity)return 0;std::snprintf(output,capacity,"%s",fallback);return std::strlen(output);}
inline int WritePrivateProfileString(const char*,const char*,const char*,const char*){return 1;}
struct winampVisModule{int sRate=44100,nCh=2;unsigned char spectrumData[2][576]{},waveformData[2][576]{};};
extern winampVisModule* g_this_mod;
enum visModeEnum {spectrum,wave};
inline visModeEnum visMode=wave;
void Process_Map(void*,void*);
void RenderDots(unsigned char*);
void RenderWave(unsigned char*);
