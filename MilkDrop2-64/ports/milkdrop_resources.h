#pragma once
#include <windows.h>
// The original English resources are included in the renderer DLL.
inline char* SuiteString(HINSTANCE h, UINT id, char* dest = nullptr, size_t size = 0) {
    static thread_local char pool[32][4096]; static thread_local unsigned n;
    if (!dest) { dest = pool[(n++) % 32]; size = 4096; }
    if (size) { dest[0] = 0; LoadStringA(h, id, dest, static_cast<int>(size)); }
    return dest;
}
inline wchar_t* SuiteStringW(HINSTANCE h, UINT id, wchar_t* dest = nullptr, size_t size = 0) {
    static thread_local wchar_t pool[32][4096]; static thread_local unsigned n;
    if (!dest) { dest = pool[(n++) % 32]; size = 4096; }
    if (size) { dest[0] = 0; LoadStringW(h, id, dest, static_cast<int>(size)); }
    return dest;
}
#undef WASABI_API_LNGSTR
#undef WASABI_API_LNGSTRW
#define WASABI_API_LNGSTR(local,orig,id,...) SuiteString(orig,id,##__VA_ARGS__)
#define WASABI_API_LNGSTRW(local,orig,id,...) SuiteStringW(orig,id,##__VA_ARGS__)
#undef WASABI_API_CREATEDIALOGPARAM
#undef WASABI_API_CREATEDIALOGPARAMW
#undef WASABI_API_CREATEDIALOG
#undef WASABI_API_CREATEDIALOGW
#undef WASABI_API_DIALOGBOXPARAM
#undef WASABI_API_DIALOGBOXPARAMW
#undef WASABI_API_DIALOGBOX
#undef WASABI_API_DIALOGBOXW
#undef WASABI_API_LOADMENU
#undef WASABI_API_LOADMENUW
#define WASABI_API_CREATEDIALOGPARAM(id,parent,proc,param) CreateDialogParamA(WASABI_API_ORIG_HINST,MAKEINTRESOURCEA(id),parent,(DLGPROC)proc,param)
#define WASABI_API_CREATEDIALOGPARAMW(id,parent,proc,param) CreateDialogParamW(WASABI_API_ORIG_HINST,MAKEINTRESOURCEW(id),parent,(DLGPROC)proc,param)
#define WASABI_API_CREATEDIALOG(id,parent,proc) WASABI_API_CREATEDIALOGPARAM(id,parent,proc,0)
#define WASABI_API_CREATEDIALOGW(id,parent,proc) WASABI_API_CREATEDIALOGPARAMW(id,parent,proc,0)
#define WASABI_API_DIALOGBOXPARAM(id,parent,proc,param) DialogBoxParamA(WASABI_API_ORIG_HINST,MAKEINTRESOURCEA(id),parent,(DLGPROC)proc,param)
#define WASABI_API_DIALOGBOXPARAMW(id,parent,proc,param) DialogBoxParamW(WASABI_API_ORIG_HINST,MAKEINTRESOURCEW(id),parent,(DLGPROC)proc,param)
#define WASABI_API_DIALOGBOX(id,parent,proc) WASABI_API_DIALOGBOXPARAM(id,parent,proc,0)
#define WASABI_API_DIALOGBOXW(id,parent,proc) WASABI_API_DIALOGBOXPARAMW(id,parent,proc,0)
#define WASABI_API_LOADMENU(id) LoadMenuA(WASABI_API_ORIG_HINST,MAKEINTRESOURCEA(id))
#define WASABI_API_LOADMENUW(id) LoadMenuW(WASABI_API_ORIG_HINST,MAKEINTRESOURCEW(id))
