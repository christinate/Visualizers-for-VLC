// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>

// Kept independent of WebGPU headers so scanners and VLC interfaces do not
// load or link a graphics implementation. The loader checks the binary ABI.
namespace suite {
// Parser/OS exceptions can contain the malformed input bytes. Keep diagnostic
// strings bounded and valid UTF-8 so one rejected package cannot break JSON
// serialization for the entire scan. Never use this to normalize a path.
template<class Json>std::string catalogDiagnostic(std::string text){
    if(text.size()>1024)text.resize(1024);
    return Json::parse(Json(text).dump(-1,' ',false,Json::error_handler_t::replace)).template get<std::string>();
}
inline constexpr const char* nativeDawnRevision="7daf3b0bee2c73621001ab15b256b5a84fb0b45f";
template<class Json,class Contained>
void validateNativeManifest(const Json& value,const std::filesystem::path& package,Contained contained){
    if(value.at("schema_version")!=4||value.at("api_version")!=4||value.at("adapter")!="dawn"||
       value.at("dawn_revision")!=nativeDawnRevision)
        throw std::runtime_error("Incompatible native Dawn manifest or runtime revision");
    const auto text=value.at("assets").template get<std::string>();
    // A portable manifest uses forward slashes. Reject alternate separators,
    // rooted Windows paths and NUL even when scanning it on another OS.
    if(text.empty()||text.find('\\')!=std::string::npos||text.find(':')!=std::string::npos||text.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid native assets path");
    if(!std::filesystem::is_directory(contained(package,text)))
        throw std::runtime_error("Native plugin assets directory is missing");
}
}
