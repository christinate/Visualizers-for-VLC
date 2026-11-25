#include "catalog.hpp"
#include "install.hpp"
#include "install_v2.hpp"
#include <iostream>
#include <map>
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("Usage: VisualizersCtl scan|check --suite PATH [--package PATH]");
    std::map<std::string,std::string> options;if(argc%2)throw std::runtime_error("Expected option/value pairs");for(int n=2;n+1<argc;n+=2)if(!options.emplace(argv[n],argv[n+1]).second)throw std::runtime_error("Repeated command option");
    auto suite=native::fs::absolute(options.at("--suite"));native::json value;
    if(std::string(argv[1])=="scan")value=native::scan(suite);
    else if(std::string(argv[1])=="install")value=native::installer2::install(suite,native::fs::absolute(options.at("--source")),options.count("--skip")?options.at("--skip"):"",options.count("--mode")?options.at("--mode"):"");
    else if(std::string(argv[1])=="uninstall"){
        if(options.count("--source"))value=native::installer2::uninstall(suite,native::fs::absolute(options.at("--source")));
        else {auto journal=native::read(native::contained(suite,".install/"+options.at("--component")+"/journal.json"));if(journal.value("schema_version",0)==2)throw std::runtime_error("Use the matching package uninstaller/source for this component");native::removeComponent(suite,options.at("--component"));value=native::scan(suite);}
    }
    else if(std::string(argv[1])=="check"){auto package=native::contained(suite,native::fs::relative(native::fs::absolute(options.at("--package")),suite));value=native::validate(suite,package);}
    else throw std::runtime_error("Unknown command");std::cout<<value.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
