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
extern char** environ;
#endif
namespace suite {
#ifdef _WIN32
inline DWORD browserRuntime(const std::filesystem::path& base,std::vector<std::wstring> arguments,bool wait=false){
    auto exe=base/L"browser/runtime/electron.exe";auto app=base/L"browser/app";
    if(!std::filesystem::is_regular_file(exe)||!std::filesystem::is_regular_file(app/L"main.cjs"))throw std::runtime_error("The visualizer browser runtime is missing; reinstall this package");
    arguments.insert(arguments.begin(),{app.wstring(),L"--suite",base.wstring()});
    return process::launch(exe,arguments,wait);
}
#else
inline int browserRuntime(const std::filesystem::path& base,std::vector<std::string> arguments,bool wait=false){
#ifdef __APPLE__
    auto exe=base/"browser/runtime/Electron.app/Contents/MacOS/Electron";
#else
    auto exe=base/"browser/runtime/electron";
#endif
    auto app=base/"browser/app";
    if(!std::filesystem::is_regular_file(exe)||!std::filesystem::is_regular_file(app/"main.cjs"))throw std::runtime_error("The visualizer browser runtime is missing; reinstall this package");
    arguments.insert(arguments.begin(),{exe.string(),app.string(),"--suite",base.string()});std::vector<char*> args;for(auto& arg:arguments)args.push_back(arg.data());args.push_back(nullptr);
    pid_t child=0;int result=posix_spawn(&child,exe.c_str(),nullptr,nullptr,args.data(),environ);if(result)throw std::runtime_error("Could not start the browser runtime");
    if(wait){int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}return WIFEXITED(status)?WEXITSTATUS(status):1;}
    std::thread([child]{int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}}).detach();
    return 0;
}
#endif
}
