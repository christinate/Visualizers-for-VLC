// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <cstdint>
namespace visualizers {
struct CaptureServiceOptions {
    std::filesystem::path suite,profile,output;
    std::string id,lineInput;
    uint32_t owner=0;
    bool test=false,automatePicker=false,generated=false,cancelPicker=false;
    double seconds=0;
};
int runCaptureService(const CaptureServiceOptions&);
}
