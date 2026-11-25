// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <string>
#include <vector>
int runNativeWindow(int,char**);
int main(int argc,char** argv){
    std::vector<std::string> arguments(argv,argv+argc);bool plugin=false,settings=false;std::string id="terrain";
    for(int n=1;n<argc;++n){const std::string a=argv[n];if(a=="--plugin")plugin=true;else if(a=="--settings")settings=true;else if(a=="--visualizer"&&n+1<argc)id=argv[++n];}
    if(!plugin&&!settings&&(id=="terrain"||id=="geiss")){
        auto folder=std::filesystem::absolute(argv[0]).parent_path();
#ifdef __APPLE__
        if(folder.filename()=="MacOS")folder=folder.parent_path().parent_path().parent_path();
#endif
        const auto module=folder/((id=="terrain"?"terrain_dawn":"prismflow_dawn")+
#ifdef _WIN32
            std::string(".dll")
#else
            std::string(".so")
#endif
        );
        const auto encoded=module.u8string();arguments.insert(arguments.end(),{"--plugin",std::string(encoded.begin(),encoded.end())});
    }
    std::vector<char*> values;for(auto& value:arguments)values.push_back(value.data());values.push_back(nullptr);
    return runNativeWindow(int(arguments.size()),values.data());
}
