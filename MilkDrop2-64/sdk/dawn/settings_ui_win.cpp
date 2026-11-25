// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_ui.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <exception>
#include <cmath>
#include <stdexcept>

namespace visualizers {namespace {
std::wstring wide(const std::string& value){const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(size<=0)return L"";std::wstring out(size,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),size);return out;}
std::wstring controlText(HWND control,int maximum=4096){const int size=GetWindowTextLengthW(control);if(size>maximum)throw std::runtime_error("Settings edit is too long");std::wstring out(size+1,0);out.resize(GetWindowTextW(control,out.data(),int(out.size())));return out;}
std::string sceneText(const std::wstring& text){
    if(text.empty())return {};
    auto s=text;s.erase(std::remove(s.begin(),s.end(),L'\r'),s.end());
    if(s.empty())return {};
    const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    if(count<=0)throw std::runtime_error("Scene settings are not valid Unicode");
    std::string out(count,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),count,nullptr,nullptr);
    return out;
}
std::wstring multiline(const std::string& value){
    std::wstring result;
    for(auto c:wide(value)){if(c==L'\r')continue;if(c==L'\n')result+=L'\r';result+=c;}
    return result;
}
std::string ascii(const std::wstring& text){std::string out;for(auto c:text){if(c>127)throw std::runtime_error("Enter a number using digits and a decimal point");out+=char(c);}return out;}
struct Control {SettingField field;HWND window=nullptr;std::wstring text;LRESULT selected=0;};
struct State {
    const SettingsJson& values;const std::string& title;const std::function<bool()>& keepOpen;const SettingsAutomation* automation;const std::string& notice;
    std::vector<SettingField> fields;std::vector<Control> controls;std::optional<SettingsJson> result;std::exception_ptr failure;HWND note=nullptr;unsigned ticks=0;
};
void save(HWND dialog,State& state){
    auto patch=SettingsJson::object();
    for(const auto& control:state.controls){const auto& field=control.field;
        if(field.type==SettingType::Boolean){const bool checked=SendMessageW(control.window,BM_GETCHECK,0,0)==BST_CHECKED;if(checked!=state.values.at(field.key).get<bool>())patch[field.key]=checked;}
        else if(field.type==SettingType::Choice){const auto row=SendMessageW(control.window,CB_GETCURSEL,0,0);if(row!=control.selected){if(row<0||size_t(row)>=field.choices.size())throw std::runtime_error("Choose a valid "+field.label);patch[field.key]=field.choices[size_t(row)].value;}}
        else if(field.type==SettingType::Ini){const auto text=controlText(control.window,65536);if(text!=control.text)patch[field.key]=sceneText(text);}
        else {const auto text=controlText(control.window);if(text!=control.text){try{const auto number=settingNumber(ascii(text));if(field.type==SettingType::Integer&&number==std::floor(number)&&number>=field.minimum&&number<=field.maximum)patch[field.key]=int(number);else patch[field.key]=number;}catch(const std::exception&){throw std::runtime_error("Check "+field.label+"; enter a finite number");}}}
    }
    validateSettingsPatch(state.fields,patch);state.result=std::move(patch);EndDialog(dialog,IDOK);
}
INT_PTR CALLBACK procedure(HWND dialog,UINT message,WPARAM w,LPARAM l){
    auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
    try{
        if(message==WM_INITDIALOG){state=reinterpret_cast<State*>(l);SetWindowLongPtrW(dialog,DWLP_USER,LONG_PTR(state));SetWindowTextW(dialog,wide(state->title+" settings").c_str());
            auto create=[&](const wchar_t* cls,const std::wstring& text,DWORD style,int x,int y,int width,int height,int id){RECT bounds{x,y,x+width,y+height};MapDialogRect(dialog,&bounds);
                auto control=CreateWindowExW(std::wstring(cls)==L"EDIT"?WS_EX_CLIENTEDGE:0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|style,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,dialog,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
                if(!control)throw std::runtime_error("Cannot create settings control");SendMessageW(control,WM_SETFONT,WPARAM(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return control;};
            int y=8;for(const auto& field:state->fields){Control item{field};const auto& value=state->values.at(field.key);const int id=100+int(state->controls.size());
                if(field.type==SettingType::Boolean){item.window=create(L"BUTTON",wide(field.label),WS_TABSTOP|BS_AUTOCHECKBOX,10,y,350,14,id);SendMessageW(item.window,BM_SETCHECK,value.get<bool>()?BST_CHECKED:BST_UNCHECKED,0);}
                else if(field.type==SettingType::Ini){
                    create(L"STATIC",wide(field.label),SS_NOPREFIX,10,y+1,340,12,0);
                    item.window=create(L"EDIT",L"",WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL,10,y+15,350,101,id);
                    SendMessageW(item.window,EM_SETLIMITTEXT,65536,0);
                    SetWindowTextW(item.window,multiline(value.get<std::string>()).c_str());
                    item.text=controlText(item.window,65536);y+=104;
                }else{create(L"STATIC",wide(field.label),SS_NOPREFIX,10,y+2,230,14,0);
                    if(field.type==SettingType::Choice){item.window=create(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,245,y,115,120,id);
                        for(size_t n=0;n<field.choices.size();++n){if(SendMessageW(item.window,CB_ADDSTRING,0,LPARAM(wide(field.choices[n].label).c_str()))<0)throw std::runtime_error("Cannot populate settings choices");if(field.choices[n].value==value)item.selected=LRESULT(n);}
                        SendMessageW(item.window,CB_SETCURSEL,item.selected,0);
                    }else{item.window=create(L"EDIT",wide(value.dump()),WS_TABSTOP|ES_AUTOHSCROLL,245,y,115,15,id);item.text=controlText(item.window);}}
                state->controls.push_back(std::move(item));y+=18;
            }
            state->note=create(L"STATIC",wide(state->notice.empty()?settingsNote():state->notice),SS_NOPREFIX,10,y+4,350,35,0);
            create(L"BUTTON",L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,210,y+43,70,17,IDOK);create(L"BUTTON",L"Cancel",WS_TABSTOP|BS_PUSHBUTTON,290,y+43,70,17,IDCANCEL);
            if(!SetTimer(dialog,1,50,nullptr))throw std::runtime_error("Cannot start settings dialog timer");return TRUE;
        }
        if(!state)return FALSE;
        if(message==WM_TIMER){if(!state->keepOpen()){EndDialog(dialog,IDCANCEL);return TRUE;}
            if(state->automation&&++state->ticks==2){for(auto& control:state->controls){const auto& field=control.field;if(!state->automation->edits.contains(field.key))continue;const auto& value=state->automation->edits.at(field.key);
                    if(field.type==SettingType::Boolean)SendMessageW(control.window,BM_SETCHECK,value.get<bool>()?BST_CHECKED:BST_UNCHECKED,0);
                    else if(field.type==SettingType::Choice){for(size_t n=0;n<field.choices.size();++n)if(field.choices[n].value==value)SendMessageW(control.window,CB_SETCURSEL,n,0);}
                    else SetWindowTextW(control.window,field.type==SettingType::Ini?multiline(value.get<std::string>()).c_str():wide(value.dump()).c_str());}
                PostMessageW(dialog,WM_COMMAND,state->automation->cancel?IDCANCEL:IDOK,0);}return TRUE;
        }
        if(message==WM_COMMAND&&LOWORD(w)==IDOK){try{save(dialog,*state);}catch(const std::exception& error){SetWindowTextW(state->note,wide(error.what()).c_str());}return TRUE;}
        if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}
    }catch(...){if(state)state->failure=std::current_exception();EndDialog(dialog,IDCANCEL);return TRUE;}return FALSE;
}
}
std::optional<SettingsJson> editNativeSettings(const SettingsJson& values,const std::string& id,const std::string& title,
        const std::function<bool()>& keepOpen,const SettingsAutomation* automation,const std::string& notice){
    const bool ini=values.contains("sceneIni");const auto fields=settingsFields(id,ini);if(automation)validateSettingsPatch(fields,automation->edits);
    struct Template {DLGTEMPLATE dialog;WORD menu=0,windowClass=0,title=0;} layout{};layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;layout.dialog.cx=370;layout.dialog.cy=SHORT(fields.size()*18+76+(ini?104:0));
    State state{values,title,keepOpen,automation,notice,fields};const auto result=DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&layout.dialog,nullptr,procedure,LPARAM(&state));
    if(state.failure)std::rethrow_exception(state.failure);if(result==-1)throw std::runtime_error("Cannot open settings dialog");return state.result;
}
}
