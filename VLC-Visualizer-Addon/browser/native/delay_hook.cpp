// SPDX-License-Identifier: MIT
#include <windows.h>
#include <delayimp.h>
#include <cstring>
static FARPROC WINAPI nodeDelay(unsigned event,PDelayLoadInfo info){
    if(event==dliNotePreLoadLibrary&&!_stricmp(info->szDll,"node.exe"))return reinterpret_cast<FARPROC>(GetModuleHandleW(nullptr));
    return nullptr;
}
extern "C" const PfnDliHook __pfnDliNotifyHook2=nodeDelay;
