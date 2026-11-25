// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "catalog.hpp"
#include <array>
namespace native {
// Exact installed snapshots make removal verifiable without a crypto library.
// All destinations and backups are confined to the user-selected suite.
inline bool identical(const fs::path& a,const fs::path& b){
    if(fs::is_symlink(a)||fs::is_symlink(b))return fs::is_symlink(a)&&fs::is_symlink(b)&&fs::read_symlink(a)==fs::read_symlink(b);
    if(!fs::is_regular_file(a)||!fs::is_regular_file(b)||fs::file_size(a)!=fs::file_size(b))return false;
    std::ifstream left(a,std::ios::binary),right(b,std::ios::binary);std::array<char,65536> x{},y{};
    while(left){left.read(x.data(),x.size());right.read(y.data(),y.size());if(left.gcount()!=right.gcount()||std::memcmp(x.data(),y.data(),size_t(left.gcount())))return false;}
    return left.eof()&&right.eof();
}
inline bool fileExists(const fs::path& file){return fs::exists(file)||fs::is_symlink(file);}
inline void copyAtomic(const fs::path& source,const fs::path& target){fs::create_directories(target.parent_path());auto temp=target;temp+=".install."+std::to_string(getpid());if(fs::is_symlink(source)){std::error_code error;fs::remove(temp,error);fs::create_symlink(fs::read_symlink(source),temp);}else fs::copy_file(source,temp,fs::copy_options::overwrite_existing);fs::rename(temp,target);}
inline void removeComponent(const fs::path& suite,const std::string& component){
    if(component!="addon"&&component!="geiss"&&component!="milkdrop"&&component!="terrain")throw std::runtime_error("Unknown component");
    auto journalFile=contained(suite,".install/"+component+"/journal.json");auto journal=read(journalFile);
    if(journal.at("status")=="removed")return;
    // Check every file before removing anything. Preserve modifications.
    for(const auto& file:journal.at("files")){
        auto target=contained(suite,file.at("path").get<std::string>()),snapshot=contained(suite,file.at("snapshot").get<std::string>());
        if(fileExists(target)&&!identical(target,snapshot)&&!(file.at("existed").get<bool>()&&identical(target,contained(suite,file.at("backup").get<std::string>()))))throw std::runtime_error("Modified installed file preserved: "+target.string());
        if(file.at("existed").get<bool>()&&!fileExists(contained(suite,file.at("backup").get<std::string>())))throw std::runtime_error("An original backup is missing");
    }
    for(auto it=journal["files"].rbegin();it!=journal["files"].rend();++it){auto target=contained(suite,it->at("path").get<std::string>());if(it->at("existed").get<bool>())copyAtomic(contained(suite,it->at("backup").get<std::string>()),target);else fs::remove(target);}
    for(auto it=journal["files"].rbegin();it!=journal["files"].rend();++it){auto parent=contained(suite,it->at("path").get<std::string>()).parent_path();while(parent!=fs::weakly_canonical(suite)){std::error_code error;if(!fs::remove(parent,error))break;parent=parent.parent_path();}}
    journal["status"]="removed";write(journalFile,journal);
}
inline json install(const fs::path& suite,const fs::path& source,const std::string& skip){
    if(suite==suite.root_path()||fs::weakly_canonical(source)==fs::weakly_canonical(suite))throw std::runtime_error("Select a separate suite directory");
    fs::create_directories(suite);auto payload=read(source/"native-payload.json");
    if(payload.at("platform")!=platform()||payload.at("architecture")!=architecture())throw std::runtime_error("Installer platform/architecture mismatch");
    if(!payload.at("components").contains("addon")&&!fs::is_regular_file(suite/"VlcVisualizerLauncher"))throw std::runtime_error("Install VLC Visualizer Addon before a standalone renderer package");
    std::vector<std::string> installed;
    try{for(const auto& component:{"addon","geiss","milkdrop","terrain"}){
        if(!payload.at("components").contains(component)||skip.find(component)!=std::string::npos)continue;
        auto base=std::string(".install/")+component;auto journalFile=contained(suite,base+"/journal.json");
        if(fs::exists(journalFile)&&read(journalFile).at("status")!="removed")throw std::runtime_error(std::string("Uninstall the existing component before upgrading: ")+component);
        json journal={{"schema_version",1},{"status","installing"},{"version",payload.at("version")},{"files",json::array()}};
        // Preflight all sources and create snapshots before changing targets.
        for(const auto& value:payload.at("components").at(component)){
            auto path=value.get<std::string>();if(path.rfind(".install/",0)==0)throw std::runtime_error("Reserved installer path");auto src=contained(source,path),target=contained(suite,path);if(!fs::is_regular_file(src)&&!fs::is_symlink(src))throw std::runtime_error("Installer source missing: "+path);
            json file={{"path",path},{"existed",fileExists(target)},{"snapshot",base+"/installed/"+path}};
            copyAtomic(src,contained(suite,file.at("snapshot").get<std::string>()));
            if(file["existed"].get<bool>()){if(!fs::is_regular_file(target)&&!fs::is_symlink(target))throw std::runtime_error("Destination is not a regular file/link");file["backup"]=base+"/original/"+path;copyAtomic(target,contained(suite,file.at("backup").get<std::string>()));}
            journal["files"].push_back(file);
        }
        write(journalFile,journal);installed.push_back(component);
        for(const auto& file:journal.at("files"))copyAtomic(contained(suite,file.at("snapshot").get<std::string>()),contained(suite,file.at("path").get<std::string>()));
        journal["status"]="installed";write(journalFile,journal);
    }}catch(...){for(auto it=installed.rbegin();it!=installed.rend();++it){try{removeComponent(suite,*it);}catch(...){}}throw;}
    return scan(suite);
}
}
