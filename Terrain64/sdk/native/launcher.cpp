// SPDX-License-Identifier: GPL-2.0-or-later
// Launch the installed VLC without changing its signed app or system files.
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
    fs::path executable;
#ifdef __APPLE__
    uint32_t size=0;_NSGetExecutablePath(nullptr,&size);std::vector<char> buffer(size);if(_NSGetExecutablePath(buffer.data(),&size))throw std::runtime_error("Cannot locate launcher");executable=fs::weakly_canonical(buffer.data());
#else
    executable=fs::read_symlink("/proc/self/exe");
#endif
    fs::path suite=executable.parent_path();
    if(suite.filename()=="MacOS")suite=suite.parent_path().parent_path().parent_path();
    if(const char* override=getenv("VLC_VISUALIZERS_SUITE"))suite=override;
    bool scan=false;std::string interfaces="visualizers_menu",player;
    auto addInterfaces=[&](const std::string& list){size_t begin=0;while(begin<=list.size()){auto end=list.find(':',begin);auto name=list.substr(begin,end==std::string::npos?end:end-begin);if(!name.empty()&&(":"+interfaces+":").find(":"+name+":")==std::string::npos)interfaces+=":"+name;if(end==std::string::npos)break;begin=end+1;}};
#ifdef __APPLE__
    player="/Applications/VLC.app/Contents/MacOS/VLC";
#else
    player="/usr/bin/vlc";
#endif
    if(const char* override=getenv("VLC_VISUALIZERS_VLC"))player=override;
    std::vector<std::string> args;
    for(int n=1;n<argc;++n){std::string value=argv[n];if(value=="--scan-visualizers")scan=true;
        else if(value=="--vlc"&&n+1<argc)player=argv[++n];
        else if(value.rfind("--extraintf=",0)==0)addInterfaces(value.substr(12));
        else if(value=="--extraintf"&&n+1<argc)addInterfaces(argv[++n]);
        else args.push_back(value);}
    if(scan){player=(suite/"VisualizersCtl").string();args={"scan","--suite",suite.string()};}
    else {
        if(!fs::is_regular_file(player)||!fs::is_regular_file(suite/"vlc-plugins"/
#ifdef __APPLE__
            "libvlc_visualizers_plugin.dylib"
#else
            "libvlc_visualizers_plugin.so"
#endif
        ))throw std::runtime_error("VLC or the addon module is missing. Install the addon or select VLC with --vlc PATH.");
        setenv("VLC_VISUALIZERS_SUITE",suite.c_str(),1);std::string modules=(suite/"vlc-plugins").string();if(const char* previous=getenv("VLC_PLUGIN_PATH"))modules+=":"+std::string(previous);setenv("VLC_PLUGIN_PATH",modules.c_str(),1);
        args.insert(args.begin(),"--extraintf="+interfaces);
#ifndef __APPLE__
        args.insert(args.begin(),"--no-one-instance");
#endif
    }
    args.insert(args.begin(),player);std::vector<char*> values;for(auto& value:args)values.push_back(value.data());values.push_back(nullptr);execv(player.c_str(),values.data());throw std::runtime_error("Could not start "+player);
}catch(const std::exception& error){std::cerr<<"VLC Visualizers: "<<error.what()<<'\n';return 1;}}
