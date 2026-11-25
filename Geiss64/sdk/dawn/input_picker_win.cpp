// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_picker.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <exception>
#include <stdexcept>

namespace visualizers {
void pumpNativeUi(){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
namespace {
std::wstring wide(const std::string& value){
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(count<=0)throw std::runtime_error("Audio input text is not valid UTF-8");
    std::wstring out(count,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),count);return out;
}
struct State {
    const std::vector<InputDevice>& devices;const std::function<bool()>& keepOpen;
    const PickerAutomation* automation;unsigned initial;const std::string& notice;std::optional<std::string> result;
    std::exception_ptr failure;HWND list=nullptr;unsigned ticks=0;
};
void accept(HWND dialog,State& state){
    const auto row=SendMessageW(state.list,LB_GETCURSEL,0,0);
    if(row!=LB_ERR){state.result=inputRowId(state.devices,unsigned(row));EndDialog(dialog,IDOK);}
}
INT_PTR CALLBACK procedure(HWND dialog,UINT message,WPARAM w,LPARAM l){
    auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
    try{
        if(message==WM_INITDIALOG){
            state=reinterpret_cast<State*>(l);SetWindowLongPtrW(dialog,DWLP_USER,LONG_PTR(state));
            SetWindowTextW(dialog,L"Audio input");
            auto create=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){
                RECT bounds{x,y,x+width,y+height};MapDialogRect(dialog,&bounds);
                auto child=CreateWindowExW(cls==std::wstring(L"LISTBOX")?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,
                    bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,dialog,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
                if(!child)throw std::runtime_error("Cannot create audio input control");
                SendMessageW(child,WM_SETFONT,WPARAM(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;
            };
            const auto prompt=wide(state->notice.empty()?"Choose a recording input. Ctrl-L opens this list again.":state->notice);
            create(L"STATIC",prompt.c_str(),0,10,9,340,36,100);
            state->list=create(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,10,50,340,130,101);
            SendMessageW(state->list,LB_ADDSTRING,0,LPARAM(L"Default recording input"));
            for(const auto& device:state->devices){const auto label=wide(device.label+(device.isDefault?" (default)":"")+" — "+device.id);
                if(SendMessageW(state->list,LB_ADDSTRING,0,LPARAM(label.c_str()))<0)throw std::runtime_error("Cannot populate audio input list");}
            SendMessageW(state->list,LB_SETHORIZONTALEXTENT,2000,0);SendMessageW(state->list,LB_SETCURSEL,state->initial,0);
            create(L"BUTTON",L"Use input",WS_TABSTOP|BS_DEFPUSHBUTTON,205,190,70,17,IDOK);
            create(L"BUTTON",L"Cancel",WS_TABSTOP|BS_PUSHBUTTON,280,190,70,17,IDCANCEL);
            if(!SetTimer(dialog,1,50,nullptr))throw std::runtime_error("Cannot start audio input dialog timer");
            SetForegroundWindow(dialog);SetFocus(state->list);return FALSE;
        }
        if(!state)return FALSE;
        if(message==WM_TIMER){
            if(!state->keepOpen()){EndDialog(dialog,IDCANCEL);return TRUE;}
            if(state->automation&&++state->ticks==state->automation->pollsBeforeAction){
                if(state->automation->cancel)PostMessageW(dialog,WM_COMMAND,IDCANCEL,0);
                else {if(state->automation->row)SendMessageW(state->list,LB_SETCURSEL,*state->automation->row,0);PostMessageW(dialog,WM_COMMAND,IDOK,0);}
            }
            return TRUE;
        }
        if(message==WM_COMMAND){
            if(LOWORD(w)==IDOK||(LOWORD(w)==101&&HIWORD(w)==LBN_DBLCLK)){accept(dialog,*state);return TRUE;}
            if(LOWORD(w)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
        }
        if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
    }catch(...){if(state)state->failure=std::current_exception();EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}
}
std::optional<std::string> pickAudioInput(const std::vector<InputDevice>& devices,const std::string& current,
        const std::function<bool()>& keepOpen,const PickerAutomation* automation,const std::string& notice){
    if(devices.size()>1024)throw std::runtime_error("Audio input list exceeds its limit");
    struct Template {DLGTEMPLATE dialog;WORD menu=0,windowClass=0,title=0;} layout{};
    layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    layout.dialog.cx=360;layout.dialog.cy=220;
    State state{devices,keepOpen,automation,selectedInputRow(devices,current),notice};
    const auto result=DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&layout.dialog,nullptr,procedure,LPARAM(&state));
    if(state.failure)std::rethrow_exception(state.failure);
    if(result==-1)throw std::runtime_error("Cannot open audio input dialog");
    return state.result;
}
}
