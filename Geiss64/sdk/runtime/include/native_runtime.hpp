// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <vector>
#include <string>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include "process.hpp"
#else
#include <spawn.h>
#include <sys/wait.h>
#include <cerrno>
extern char** environ;
#endif
namespace suite {
inline std::filesystem::path nativeRuntimePath(const std::filesystem::path& base){
#ifdef _WIN32
    return base/L"native/VisualizerNative.exe";
#elif defined(__APPLE__)
    return base/"native/VisualizerNative.app/Contents/MacOS/VisualizerNative";
#else
    return base/"native/VisualizerNative";
#endif
}
#ifdef _WIN32
inline DWORD nativeRuntime(const std::filesystem::path& base,std::vector<std::wstring> arguments,bool wait=false){
    const auto exe=nativeRuntimePath(base);
    if(!std::filesystem::is_regular_file(exe))throw std::runtime_error("The native visualizer runtime is missing; reinstall this package");
    arguments.insert(arguments.end(),{L"--suite",base.wstring()});
    return process::launch(exe,arguments,wait);
}
#else
inline int nativeRuntime(const std::filesystem::path& base,std::vector<std::string> arguments,bool wait=false){
    auto exe=nativeRuntimePath(base);
    if(!std::filesystem::is_regular_file(exe))throw std::runtime_error("The native visualizer runtime is missing; reinstall this package");
    arguments.insert(arguments.end(),{"--suite",base.string()});
#ifdef __APPLE__
    // Launch Services associates microphone approval with our app bundle.
    // /usr/bin/open's exit status reports launch success, not app exit status.
    const auto bundle=exe.parent_path().parent_path().parent_path();exe="/usr/bin/open";
    arguments.insert(arguments.begin(),{"-W","-n",bundle.string(),"--args"});
#endif
    arguments.insert(arguments.begin(),exe.string());std::vector<char*> args;
    for(auto& arg:arguments)args.push_back(arg.data());args.push_back(nullptr);
    pid_t child=0;const int result=posix_spawn(&child,exe.c_str(),nullptr,nullptr,args.data(),environ);
    if(result)throw std::runtime_error("Could not start the native visualizer runtime");
    if(wait){int status=0;pid_t ended;do{ended=waitpid(child,&status,0);}while(ended<0&&errno==EINTR);
        return ended==child&&WIFEXITED(status)?WEXITSTATUS(status):1;}
    std::thread([child]{int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}}).detach();return 0;
}
#endif
}
