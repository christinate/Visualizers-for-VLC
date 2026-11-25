// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "catalog.hpp"
#include "file_digest.hpp"
#include "../runtime/include/native_runtime.hpp"
#include <sys/stat.h>
#include <map>
#include <functional>
namespace native {
namespace installer2 {
inline bool present(const fs::path& path){return fs::exists(path)||fs::is_symlink(path);}
inline bool identifier(const std::string& value){return !value.empty()&&value.size()<=64&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos;}
inline bool prefix(const fs::path& child,const fs::path& parent){auto c=child.begin(),p=parent.begin();for(;p!=parent.end();++p,++c)if(c==child.end()||*c!=*p)return false;return true;}
inline unsigned mode(const fs::path& path){return unsigned(fs::status(path).permissions())&0777u;}
inline bool matches(const fs::path& path,const json& spec){
    if(!present(path))return false;
    bool link=spec.at("kind")=="symlink";
    if(fs::is_symlink(path)!=link)return false;
    if(link){if(fs::read_symlink(path).generic_string()!=spec.at("target"))return false;}
    else if(!fs::is_regular_file(path)||fs::file_size(path)!=spec.at("bytes").get<uint64_t>()||mode(path)!=spec.at("mode").get<unsigned>())return false;
    return contentDigest(path)==spec.at("sha256");
}
inline json describe(const fs::path& path,const std::string& relative){
    bool link=fs::is_symlink(path);if(!link&&!fs::is_regular_file(path))throw std::runtime_error("Installer destination is not a regular file/link");
    json spec={{"path",relative},{"kind",link?"symlink":"file"},{"sha256",contentDigest(path)}};
    if(link){auto target=fs::read_symlink(path);if(target.is_absolute())throw std::runtime_error("Absolute installer symlinks are unsupported");spec["target"]=target.generic_string();}
    else {spec["bytes"]=fs::file_size(path);spec["mode"]=mode(path);}
    return spec;
}
struct Lock {
    int fd=-1;
    explicit Lock(const fs::path& suite){
        auto directory=contained(suite,".install");fs::create_directories(directory);
        auto file=contained(suite,".install/installer.lock");fd=open(file.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
        struct stat status{};
        if(fd<0||fstat(fd,&status)||!S_ISREG(status.st_mode)||status.st_uid!=geteuid()||status.st_nlink!=1||flock(fd,LOCK_EX|LOCK_NB)){
            if(fd>=0)close(fd);fd=-1;throw std::runtime_error("Another installer is active or the installer lock is invalid");
        }
    }
    Lock(const Lock&)=delete;~Lock(){if(fd>=0){flock(fd,LOCK_UN);close(fd);}}
};
inline fs::path root(const fs::path& suite){auto value=fs::weakly_canonical(fs::absolute(suite));if(value==value.root_path())throw std::runtime_error("Select a dedicated suite directory");return value;}
inline std::set<std::string> skipList(const std::string& value){
    std::set<std::string> result;if(value.empty())return result;std::istringstream stream(value);std::string item;
    while(std::getline(stream,item,',')){if(!identifier(item)||!result.insert(item).second)throw std::runtime_error("Invalid/repeated component opt-out");}
    return result;
}
struct Package {json metadata;fs::path source;std::string owner,version;std::vector<std::string> selected;};
inline Package package(const fs::path& source,const std::string& skip,const std::string& requestedMode){
    Package result;result.source=fs::weakly_canonical(fs::absolute(source));result.metadata=read(result.source/"native-payload.json");
    const auto& m=result.metadata;
    if(m.at("schema_version")!=2||m.at("platform")!=platform()||m.at("architecture")!=architecture())throw std::runtime_error("Native installer ABI/platform/architecture mismatch");
    result.owner=m.at("package_id").get<std::string>();result.version=m.at("version").get<std::string>();
    if(!identifier(result.owner)||result.version.empty()||!m.at("components").is_object()||m.at("components").empty()||m.at("components").size()>256)throw std::runtime_error("Invalid native package metadata");
    auto kind=m.at("kind").get<std::string>();auto selectedMode=requestedMode.empty()?(kind=="suite"?"suite":"standalone"):requestedMode;
    if((kind!="suite"&&kind!="renderer")||(kind=="suite"&&selectedMode!="suite")||(kind=="renderer"&&selectedMode!="standalone"&&selectedMode!="vlc"))throw std::runtime_error("Invalid native installation mode");
    auto omit=skipList(skip);for(const auto& id:omit)if(!m.at("components").contains(id)||id=="runtime"||id=="addon")throw std::runtime_error("Unknown/required component opt-out");
    if(kind=="renderer"&&(!m.at("components").contains(result.owner)||m.at("components").contains("addon")))throw std::runtime_error("Invalid renderer component ownership");
    if(kind=="suite"&&(!m.at("components").contains("runtime")||!m.at("components").contains("addon")))throw std::runtime_error("Combined package needs runtime and addon components");
    // Stable common-runtime-first ordering, followed by independent plugins.
    for(const auto& id:{std::string("runtime"),std::string("addon")})if(m.at("components").contains(id)&&!(id=="runtime"&&selectedMode=="vlc"))result.selected.push_back(id);
    for(const auto& [id,rows]:m.at("components").items())if(id!="runtime"&&id!="addon"&&!omit.count(id))result.selected.push_back(id);
    if(result.selected.empty())throw std::runtime_error("No installer components selected");
    std::set<fs::path> paths;size_t count=0;
    for(const auto& id:result.selected){
        if(!identifier(id))throw std::runtime_error("Invalid component ID");const auto& rows=m.at("components").at(id);
        if(!rows.is_array()||rows.empty())throw std::runtime_error("Empty/invalid installer file table");
        for(const auto& spec:rows){
            auto path=spec.at("path").get<std::string>();auto relative=fs::path(path);
            auto normalized=relative.lexically_normal();
            if(normalized.empty()||*normalized.begin()==".install"||!paths.insert(normalized).second)throw std::runtime_error("Reserved/duplicate installer destination");
            contained(result.source,relative);auto digest=spec.at("sha256").get<std::string>();
            if(digest.size()!=64||digest.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid installer SHA-256");
            auto type=spec.at("kind").get<std::string>();if(type!="file"&&type!="symlink")throw std::runtime_error("Unsupported installer file type");
            if(type=="symlink"&&fs::path(spec.at("target").get<std::string>()).is_absolute())throw std::runtime_error("Installer symlink must be relative");
            if(type=="file"&&(!spec.at("bytes").is_number_unsigned()&&!spec.at("bytes").is_number_integer()))throw std::runtime_error("Invalid installer file size");
            if(type=="file"&&(spec.at("bytes").get<int64_t>()<0||spec.at("mode").get<unsigned>()>0777u))throw std::runtime_error("Invalid installer file size/mode");
            if(++count>8192)throw std::runtime_error("Too many installer files");
        }
    }
    return result;
}
inline void copyNew(const fs::path& source,const fs::path& target){
    fs::create_directories(target.parent_path());
    if(fs::is_symlink(source)){fs::create_symlink(fs::read_symlink(source),target);return;}
    if(!fs::copy_file(source,target,fs::copy_options::none))throw std::runtime_error("Cannot create installer file");
}
inline int linkEntry(const fs::path& source,const fs::path& target){
    // link() follows the source symlink on macOS. Explicit flags preserve the
    // directory entry, including a library alias whose target is not yet copied.
    return ::linkat(AT_FDCWD,source.c_str(),AT_FDCWD,target.c_str(),0);
}
inline fs::path stagingDirectory(const fs::path& target){
    fs::create_directories(target.parent_path());auto pattern=(target.parent_path()/".install-publish-XXXXXX").string();std::vector<char> text(pattern.begin(),pattern.end());text.push_back(0);
    auto created=mkdtemp(text.data());if(!created)throw std::runtime_error("Cannot reserve private installer staging directory");return created;
}
inline void publish(const fs::path& source,const fs::path& target,bool replace,const json* expectedCurrent=nullptr){
    if(replace&&!expectedCurrent)throw std::runtime_error("Replacement needs its expected current identity");
    auto directory=stagingDirectory(target),temporary=directory/"file",displaced=directory/"displaced";
    bool moved=false,published=false;
    try{
        copyNew(source,temporary);
        if(contentDigest(source)!=contentDigest(temporary))throw std::runtime_error("Staged installer file hash differs");
        if(replace){
            // Capture the actual directory entry first, then verify it. A
            // non-cooperating writer cannot be silently replaced after our
            // earlier preflight hash check.
            fs::rename(target,displaced);moved=true;
            if(!matches(displaced,*expectedCurrent))throw std::runtime_error("Original destination changed during publication");
        }
        if(linkEntry(temporary,target))throw std::runtime_error(std::string("Cannot publish installer file without replacing an existing destination: ")+std::strerror(errno));
        published=true;
    }catch(...){
        auto failure=std::current_exception();bool restored=true;
        if(moved&&linkEntry(displaced,target))restored=false;
        if(!restored)throw std::runtime_error("Concurrent destination was preserved; displaced data retained in "+directory.string());
        std::error_code error;fs::remove_all(directory,error);std::rethrow_exception(failure);
    }
    // Once published, housekeeping must not report a failed publication before
    // its caller records ownership. Retain any unremovable temporary evidence.
    if(published){std::error_code error;fs::remove_all(directory,error);}
}
inline void removeOwned(const fs::path& target,const json& expected){
    if(!present(target))return;auto directory=stagingDirectory(target),displaced=directory/"displaced";
    fs::rename(target,displaced);
    try{
        if(!matches(displaced,expected))throw std::runtime_error("Installed file changed during removal");
    }catch(...){
        auto failure=std::current_exception();
        if(linkEntry(displaced,target))throw std::runtime_error("Concurrent destination was preserved; displaced data retained in "+directory.string());
        std::error_code error;fs::remove_all(directory,error);std::rethrow_exception(failure);
    }
    fs::remove_all(directory);
}
struct Component {std::string id;json journal;fs::path directory;bool ready=false;};
inline void checkRemoval(const fs::path& suite,const Component& component){
    const auto& j=component.journal;
    if(j.at("schema_version")!=2)throw std::runtime_error("Use the original archive to remove an older installer generation");
    for(const auto& file:j.at("files")){
        auto target=contained(suite,file.at("spec").at("path").get<std::string>());
        auto snapshot=contained(suite,file.at("snapshot").get<std::string>());
        if(!matches(snapshot,file.at("spec")))throw std::runtime_error("Installed snapshot is missing or changed");
        if(file.at("existed").get<bool>()){
            auto backup=contained(suite,file.at("backup").get<std::string>());
            if(!matches(backup,file.at("original")))throw std::runtime_error("An original backup is missing or changed");
        }
        if(!file.at("published").get<bool>())continue;
        if(present(target)&&!matches(target,file.at("spec"))&&!(file.at("existed").get<bool>()&&matches(target,file.at("original"))))throw std::runtime_error("Modified installed file preserved: "+target.string());
    }
}
inline void removeFiles(const fs::path& suite,Component& component){
    auto& j=component.journal;j["status"]="removing";write(component.directory/"journal.json",j);
    for(auto it=j["files"].rbegin();it!=j["files"].rend();++it){
        if(!it->at("published").get<bool>())continue;
        auto target=contained(suite,it->at("spec").at("path").get<std::string>());
        if(it->at("existed").get<bool>()){
            if(!present(target)||!matches(target,it->at("original")))publish(contained(suite,it->at("backup").get<std::string>()),target,present(target),&it->at("spec"));
        }else if(present(target))removeOwned(target,it->at("spec"));
    }
    for(auto it=j["files"].rbegin();it!=j["files"].rend();++it){auto parent=contained(suite,it->at("spec").at("path").get<std::string>()).parent_path();
        while(parent!=suite&&!parent.empty()&&parent!=parent.parent_path()){std::error_code error;if(!fs::is_directory(parent,error)||!fs::remove(parent,error))break;parent=parent.parent_path();}
    }
    j["status"]="removed";write(component.directory/"journal.json",j);
}
inline json install(const fs::path& requestedSuite,const fs::path& source,const std::string& skip,const std::string& modeValue,const std::function<void(size_t)>& beforePublish={}){
    auto pkg=package(source,skip,modeValue);auto suite=root(requestedSuite);
    if(prefix(pkg.source,suite)||prefix(suite,pkg.source))throw std::runtime_error("Keep installer source separate from the suite");
    fs::create_directories(suite);Lock lock(suite);
    if(pkg.metadata.at("kind")=="renderer"&&modeValue=="vlc"){
        if(!fs::is_regular_file(suite/"VlcVisualizerLauncher")||!fs::is_regular_file(suite::nativeRuntimePath(suite)))throw std::runtime_error("Install the native VLC addon before a plugin-only component");
    }
    std::map<fs::path,std::string> owners;
    for(const auto& item:fs::directory_iterator(suite/".install")){
        if(!item.is_directory()||!identifier(item.path().filename().string())||!fs::is_regular_file(item.path()/"journal.json"))continue;
        auto directory=contained(suite,fs::path(".install")/item.path().filename());auto journal=read(directory/"journal.json");if(journal.value("status","")=="removed")continue;
        for(const auto& file:journal.at("files")){
            auto path=file.contains("spec")?file.at("spec").at("path").get<std::string>():file.at("path").get<std::string>();
            owners[contained(suite,path)]=item.path().filename().string();
        }
    }
    std::map<fs::path,json> originals;
    for(const auto& id:pkg.selected){
        auto directory=contained(suite,".install/"+id);
        if(present(directory/"journal.json")&&read(directory/"journal.json").value("status","")!="removed")throw std::runtime_error("Uninstall the existing component before upgrading: "+id);
        for(const auto& spec:pkg.metadata.at("components").at(id)){
            auto relative=spec.at("path").get<std::string>();auto src=contained(pkg.source,relative);auto target=contained(suite,relative);
            if(owners.count(target))throw std::runtime_error("Destination is owned by another component: "+owners.at(target));
            if(!matches(src,spec))throw std::runtime_error("Installer source failed its hash/type/mode check: "+relative);
            originals[target]=present(target)?describe(target,relative):json();
        }
        if(id!="runtime"&&id!="addon")validate(pkg.source,pkg.source/"plugins"/id);
    }
    auto pattern=(suite/".install/.prepare-XXXXXX").string();std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back(0);
    auto prepared=mkdtemp(chars.data());if(!prepared)throw std::runtime_error("Cannot create private installer preparation directory");fs::path transaction=prepared;
    std::vector<Component> components;size_t publishedCount=0;
    try{
        // Snapshot all selected components and originals before publishing any.
        for(const auto& id:pkg.selected){
            Component component{id,{{"schema_version",2},{"package_id",pkg.owner},{"version",pkg.version},{"component",id},{"status","installing"},{"files",json::array()}},contained(suite,".install/"+id)};
            for(const auto& spec:pkg.metadata.at("components").at(id)){
                auto relative=spec.at("path").get<std::string>();auto src=contained(pkg.source,relative),target=contained(suite,relative);
                auto snapshot=transaction/id/"installed"/relative;copyNew(src,snapshot);
                if(!matches(snapshot,spec))throw std::runtime_error("Installer snapshot verification failed");
                const auto& original=originals.at(target);bool existed=!original.is_null();
                if((existed&&!matches(target,original))||(!existed&&present(target)))throw std::runtime_error("Destination changed during snapshot preparation");
                json file={{"spec",spec},{"snapshot",".install/"+id+"/installed/"+relative},{"existed",existed},{"published",false}};
                if(file.at("existed").get<bool>()){
                    file["original"]=original;file["backup"]=".install/"+id+"/original/"+relative;
                    auto backup=transaction/id/"original"/relative;copyNew(target,backup);if(!matches(backup,file["original"]))throw std::runtime_error("Original backup verification failed");
                }
                component.journal["files"].push_back(file);
            }
            write(transaction/id/"journal.json",component.journal);components.push_back(std::move(component));
        }
        for(auto& component:components){
            if(present(component.directory))fs::rename(component.directory,transaction/("previous-"+component.id));
            fs::rename(transaction/component.id,component.directory);
            component.ready=true;
        }
        for(auto& component:components)for(auto& file:component.journal["files"]){
            auto target=contained(suite,file.at("spec").at("path").get<std::string>()),snapshot=contained(suite,file.at("snapshot").get<std::string>());
            if(file.at("existed").get<bool>()){if(!matches(target,file.at("original")))throw std::runtime_error("Original destination changed after preflight");}
            else if(present(target))throw std::runtime_error("A new destination appeared after preflight and was preserved");
            if(beforePublish)beforePublish(publishedCount);
            publish(snapshot,target,file.at("existed").get<bool>(),file.at("existed").get<bool>()?&file.at("original"):nullptr);file["published"]=true;++publishedCount;write(component.directory/"journal.json",component.journal);
        }
        auto result=scan(suite);
        for(auto& component:components){component.journal["status"]="installed";write(component.directory/"journal.json",component.journal);}
        // Retain previous removed-generation snapshots for repair; no originals
        // or staged source archives are silently discarded.
        if(fs::is_empty(transaction))fs::remove(transaction);else fs::rename(transaction,suite/".install"/(".completed-"+transaction.filename().string()));return result;
    }catch(...){
        auto failure=std::current_exception();bool clean=true;
        for(auto it=components.rbegin();it!=components.rend();++it){
            if(!it->ready)continue;
            try{checkRemoval(suite,*it);removeFiles(suite,*it);}catch(...){clean=false;}
        }
        try{scan(suite);}catch(...){}
        if(!clean)throw std::runtime_error("Native installer rollback could not finish; modified files and transaction evidence were retained");
        std::rethrow_exception(failure);
    }
}
inline json uninstall(const fs::path& requestedSuite,const fs::path& source){
    auto pkg=package(source,"","");auto suite=root(requestedSuite);if(!fs::is_directory(suite/".install"))return scan(suite);Lock lock(suite);
    std::vector<Component> components;
    for(const auto& id:pkg.selected){
        auto directory=contained(suite,".install/"+id);if(!fs::is_regular_file(directory/"journal.json"))continue;auto journal=read(directory/"journal.json");
        if(journal.value("schema_version",0)!=2)throw std::runtime_error("Use the original archive to remove an older installer generation");
        if(journal.value("package_id","")!=pkg.owner||journal.value("status","")=="removed")continue;
        if(journal.at("version")!=pkg.version||journal.at("component")!=id||journal.at("files").size()!=pkg.metadata.at("components").at(id).size())throw std::runtime_error("Uninstaller version/ownership differs from journal");
        for(size_t i=0;i<journal.at("files").size();++i){
            const auto& file=journal.at("files").at(i);const auto& spec=pkg.metadata.at("components").at(id).at(i);auto relative=spec.at("path").get<std::string>();
            if(file.at("spec")!=spec||file.at("snapshot")!=".install/"+id+"/installed/"+relative||
               (file.at("existed").get<bool>()&&file.at("backup")!=".install/"+id+"/original/"+relative))throw std::runtime_error("Uninstaller ownership paths differ from trusted payload");
        }
        components.push_back({id,std::move(journal),directory});
    }
    // Preflight the entire package before changing any component, including
    // components that precede a modified file in reverse uninstall order.
    for(const auto& component:components)checkRemoval(suite,component);
    for(auto it=components.rbegin();it!=components.rend();++it)removeFiles(suite,*it);
    return scan(suite);
}
}
}
