// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "audio_capture.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace visualizers {
// Own the Linux QApplication within main(), so it is destroyed before Qt's
// global cleanup. This object must outlive dialogs and the native window.
class NativeUiSession {
#if !defined(_WIN32) && !defined(__APPLE__)
    struct Impl;std::unique_ptr<Impl> impl_;
public:NativeUiSession();~NativeUiSession();
#else
public:NativeUiSession()=default;
#endif
public:
    NativeUiSession(const NativeUiSession&)=delete;
    NativeUiSession& operator=(const NativeUiSession&)=delete;
};
// Keep Cocoa/main-thread permission callbacks and native UI responsive in the
// background capture helper, which does not create an SDL/GPU window.
void pumpNativeUi();
// Internal test actions drive only this process's dialog, never desktop input.
struct PickerAutomation {bool cancel=false;std::optional<unsigned> row;unsigned pollsBeforeAction=2;};
std::optional<std::string> pickAudioInput(const std::vector<InputDevice>& devices,
    const std::string& current,const std::function<bool()>& keepOpen,
    const PickerAutomation* automation=nullptr,const std::string& notice="");
inline unsigned selectedInputRow(const std::vector<InputDevice>& devices,const std::string& current){
    for(unsigned n=0;n<devices.size();++n)if(devices[n].id==current)return n+1;
    return 0;
}
inline std::string inputRowId(const std::vector<InputDevice>& devices,unsigned row){
    if(row==0)return "default";
    return devices.at(row-1).id;
}
}
