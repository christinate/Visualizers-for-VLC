// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_picker.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace visualizers;
int main(){try{
    NativeUiSession ui;
    const std::vector<InputDevice> devices={{"backend:uid-α","Microphone — 音声",true},{"backend:uid-β","Microphone — 音声",false}};
    auto open=[](){return true;};PickerAutomation accept{},second{false,2},cancel{true,{}};
    if(pickAudioInput(devices,"",open,&accept)!=std::optional<std::string>("default"))throw std::runtime_error("Default input selection failed");
    if(pickAudioInput(devices,devices[0].id,open,&accept)!=std::optional<std::string>(devices[0].id))throw std::runtime_error("Existing selection was not retained");
    if(pickAudioInput(devices,"default",open,&second)!=std::optional<std::string>(devices[1].id))throw std::runtime_error("Duplicate/Unicode label identity was lost");
    if(pickAudioInput(devices,devices[0].id,open,&cancel))throw std::runtime_error("Cancelled selection was accepted");
    unsigned ticks=0;const auto start=std::chrono::steady_clock::now();
    if(pickAudioInput(devices,"default",[&](){return ++ticks<3;}))throw std::runtime_error("Owner-close request was ignored");
    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(3))throw std::runtime_error("Picker owner closure was unresponsive");
    std::cout<<"Native picker: default, existing ID, duplicate/Unicode labels, cancel, owner-close pass; no recording input opened\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
