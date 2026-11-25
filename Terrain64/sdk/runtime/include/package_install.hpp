// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "catalog.hpp"
#include "file_sha256.hpp"
#include <functional>
#include <cwctype>
#include <map>
namespace renderer_install {
namespace fs=std::filesystem;using json=catalog::json;
using Roots=std::map<std::string,fs::path>;
struct Options {fs::path root,payload,vlc,winamp;std::wstring selection;fs::path menu;};
struct Entry {std::string anchor,path,source,sha;bool generated=false;};
struct Plan {json manifest;Roots roots;std::set<std::string> selected;std::string bridgeArch;std::vector<Entry> files;};
inline fs::path rootPath(const fs::path& path){
    if(path.empty())throw std::runtime_error("An installation directory is required");
    auto absolute=fs::weakly_canonical(path);
    if(absolute==absolute.root_path())throw std::runtime_error("A drive root is not an installation directory");
    return catalog::extended(absolute);
}
inline bool samePath(const fs::path& a,const fs::path& b){return _wcsicmp(rootPath(a).c_str(),rootPath(b).c_str())==0;}
inline fs::path journalPath(const fs::path& root){return catalog::contained(root,L".package/transaction.json");}
inline bool present(const fs::path& file){return fs::exists(file)||fs::is_symlink(file);}
inline std::set<std::string> choices(const std::wstring& value){
    auto text=catalog::utf8(value);std::set<std::string> result;
    if(text.empty()||text.back()==',')throw std::runtime_error("Select at least one installation component");
    std::istringstream stream(text);std::string item;
    while(std::getline(stream,item,','))if((item!="standalone"&&item!="vlc"&&item!="winamp")||!result.insert(item).second)throw std::runtime_error("Unknown or repeated renderer component");
    return result;
}
inline std::string architecture(const fs::path& file){
    std::ifstream input(file,std::ios::binary);IMAGE_DOS_HEADER dos{};input.read(reinterpret_cast<char*>(&dos),sizeof(dos));
    if(!input||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>16*1024*1024)throw std::runtime_error("Invalid Winamp executable");
    input.seekg(dos.e_lfanew);DWORD signature=0;IMAGE_FILE_HEADER pe{};
    input.read(reinterpret_cast<char*>(&signature),sizeof(signature));input.read(reinterpret_cast<char*>(&pe),sizeof(pe));
    if(!input||signature!=IMAGE_NT_SIGNATURE)throw std::runtime_error("Invalid Winamp executable");
    if(pe.Machine==IMAGE_FILE_MACHINE_I386)return "x86";
    if(pe.Machine==IMAGE_FILE_MACHINE_AMD64)return "x64";
    throw std::runtime_error("Unsupported Winamp architecture");
}
inline Plan makePlan(const Options& options,bool installing,const json* saved=nullptr){
    Plan plan;plan.manifest=catalog::read(options.payload/L"renderer-payload.json");
    const auto id=plan.manifest.at("id").get<std::string>();
    if(plan.manifest.at("schema_version")!=1||!catalog::idValid(id)||!plan.manifest.at("version").is_string())throw std::runtime_error("Invalid renderer payload metadata");
    plan.roots["local"]=rootPath(options.root);
    if(saved){
        if(saved->at("schema_version")!=1||saved->at("id")!=id||saved->at("version")!=plan.manifest.at("version"))throw std::runtime_error("Renderer journal and uninstaller versions differ");
        for(const auto& choice:saved->at("selected"))plan.selected.insert(choice.get<std::string>());
        std::wstring value;for(const auto& choice:plan.selected){if(!value.empty())value+=L",";value+=catalog::wide(choice);}choices(value);
        plan.bridgeArch=saved->value("bridge_arch",std::string{});
    }else plan.selected=choices(options.selection);
    if(plan.selected.count("vlc")){
        auto vlc=rootPath(options.vlc);plan.roots["vlc"]=catalog::contained(vlc,L"visualizers");
        if(installing&&(!fs::is_regular_file(catalog::contained(plan.roots["vlc"],L"native/VisualizerNative.exe"))||!fs::is_regular_file(plan.roots["vlc"]/L"VisualizersCtl.exe")))throw std::runtime_error("Install the native VLC Visualizer Addon before its renderer component");
    }
    if(plan.selected.count("winamp")){
        plan.roots["winamp"]=rootPath(options.winamp);
        if(installing)plan.bridgeArch=architecture(plan.roots["winamp"]/L"winamp.exe");
        if(plan.bridgeArch!="x86"&&plan.bridgeArch!="x64")throw std::runtime_error("Invalid saved Winamp bridge architecture");
    }
    if(plan.selected.count("standalone")&&plan.manifest.contains("shortcuts"))plan.roots["menu"]=rootPath(options.menu);
    if(saved){
        if(saved->at("roots").size()!=plan.roots.size())throw std::runtime_error("Renderer root binding differs from journal");
        for(const auto& [name,path]:plan.roots)if(!samePath(path,catalog::wide(saved->at("roots").at(name))))throw std::runtime_error("Renderer root binding differs from journal");
    }
    auto append=[&](const std::string& anchor,const json& rows,bool generated=false){
        if(!rows.is_array())throw std::runtime_error("Invalid renderer file table");
        for(const auto& row:rows){
            Entry entry{anchor,row.at("path"),row.at("source"),generated?std::string{}:row.at("sha256").get<std::string>(),generated};
            if(generated){
                if(anchor!="menu"||fs::path(catalog::wide(entry.path)).filename()!=catalog::wide(entry.path)||fs::path(catalog::wide(entry.path)).extension()!=L".lnk"||entry.source!="shortcuts/"+entry.path)throw std::runtime_error("Invalid generated shortcut path");
                if(installing)entry.sha=filehash::sha256(catalog::contained(options.payload,catalog::wide(entry.source)));
                else {
                    const auto& savedRow=saved->at("files").at(plan.files.size());
                    entry.sha=savedRow.at("sha256").get<std::string>();
                }
            }
            if(entry.sha.size()!=64||entry.sha.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid renderer file hash");
            auto relative=catalog::wide(entry.path);
            if(relative.empty()||_wcsicmp(fs::path(relative).begin()->c_str(),L".package")==0)throw std::runtime_error("Reserved renderer destination");
            catalog::contained(plan.roots.at(anchor),relative);catalog::contained(options.payload,catalog::wide(entry.source));
            plan.files.push_back(std::move(entry));
        }
    };
    if(plan.selected.count("standalone")||plan.selected.count("winamp")){append("local",plan.manifest.at("runtime"));append("local",plan.manifest.at("plugin"));}
    if(plan.selected.count("vlc"))append("vlc",plan.manifest.at("plugin"));
    if(plan.selected.count("winamp"))append("winamp",json::array({plan.manifest.at("bridges").at(plan.bridgeArch)}));
    if(plan.roots.count("menu"))append("menu",plan.manifest.at("shortcuts"),true);
    if(plan.files.empty()||plan.files.size()>4096)throw std::runtime_error("Invalid renderer file count");
    std::set<std::wstring> destinations;
    for(const auto& entry:plan.files){
        auto target=catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path));auto key=target.wstring();
        CharLowerBuffW(key.data(),DWORD(key.size()));
        if(!destinations.insert(key).second)throw std::runtime_error("Duplicate renderer destination");
        // Detect an existing file where a parent directory is needed before
        // any file/root/journal is created.
        for(auto parent=target.parent_path();_wcsicmp(parent.c_str(),plan.roots.at(entry.anchor).c_str())!=0;parent=parent.parent_path()){
            if(parent.empty()||parent==parent.parent_path())throw std::runtime_error("Renderer parent escaped its root");
            if(present(parent)&&!fs::is_directory(parent))throw std::runtime_error("Renderer destination parent is not a directory");
        }
        if(installing){
            if(present(target))throw std::runtime_error("A renderer destination is occupied; uninstall its owner before upgrading");
            auto source=catalog::contained(options.payload,catalog::wide(entry.source));
            if(!fs::is_regular_file(source)||filehash::sha256(source)!=entry.sha)throw std::runtime_error("Renderer payload failed its source hash check");
        }
    }
    return plan;
}
inline json encode(const Entry& entry,bool published=false){return {{"anchor",entry.anchor},{"path",entry.path},{"source",entry.source},{"sha256",entry.sha},{"generated",entry.generated},{"published",published}};}
inline void bindFiles(const Plan& plan,const json& journal){
    if(journal.at("files").size()!=plan.files.size())throw std::runtime_error("Renderer journal file count differs from payload");
    for(size_t i=0;i<plan.files.size();++i){auto expected=encode(plan.files[i],journal.at("files").at(i).at("published"));if(journal.at("files").at(i)!=expected)throw std::runtime_error("Renderer journal ownership differs from trusted payload");}
}
inline void cleanDirectories(const Plan& plan){
    for(auto it=plan.files.rbegin();it!=plan.files.rend();++it){
        const auto& root=plan.roots.at(it->anchor);auto parent=catalog::contained(root,catalog::wide(it->path)).parent_path();
        while(!parent.empty()&&_wcsicmp(parent.c_str(),root.c_str())!=0&&parent!=parent.parent_path()){std::error_code error;if(!fs::is_directory(parent,error)||!fs::remove(parent,error))break;parent=parent.parent_path();}
    }
}
inline void refreshCatalogs(const Plan& plan,bool removing){
    const auto id=plan.manifest.at("id").get<std::string>();
    for(const auto& anchor:{"local","vlc"}){
        if((std::string(anchor)=="local"&&!plan.selected.count("standalone")&&!plan.selected.count("winamp"))||!plan.roots.count(anchor))continue;
        const auto& root=plan.roots.at(anchor);
        if(!removing)catalog::validate(root,catalog::contained(root,L"plugins"/fs::path(catalog::wide(id))));
        catalog::scan(root);
    }
}
inline void install(const Options& options,const std::function<void(size_t)>& beforePublish={}){
    auto plan=makePlan(options,true);const auto journalFile=journalPath(plan.roots.at("local"));
    if(present(journalFile)){auto old=catalog::read(journalFile);if(old.value("status","")!="removed"&&old.value("status","")!="rolled-back")throw std::runtime_error("Uninstall or repair the existing renderer transaction first");}
    // Validate the source manifest using its real package directory name.
    catalog::validate(options.payload,options.payload/L"plugins"/catalog::wide(plan.manifest.at("id")));
    json journal={{"schema_version",1},{"id",plan.manifest.at("id")},{"version",plan.manifest.at("version")},{"status","installing"},
                  {"selected",plan.selected},{"bridge_arch",plan.bridgeArch},{"roots",json::object()},{"files",json::array()}};
    for(const auto& [name,path]:plan.roots)journal["roots"][name]=catalog::utf8(path.wstring());
    for(const auto& entry:plan.files)journal["files"].push_back(encode(entry));
    catalog::atomicWrite(journalFile,journal);
    std::vector<size_t> published;fs::path pending;bool pendingOwned=false;
    try{
        for(size_t i=0;i<plan.files.size();++i){
            const auto& entry=plan.files[i];auto target=catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path));
            auto source=catalog::contained(options.payload,catalog::wide(entry.source));fs::create_directories(target.parent_path());
            pending=target;pending+=L".install."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
            {
                filehash::Handle output(CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
                if(output.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot reserve renderer staging file");
                pendingOwned=true;
                filehash::Handle input(CreateFileW(source.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
                if(input.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot read renderer source file");
                std::array<unsigned char,65536> buffer{};DWORD count=0,written=0;
                for(;;){
                    if(!ReadFile(input.value,buffer.data(),DWORD(buffer.size()),&count,nullptr))throw std::runtime_error("Cannot read renderer source file");
                    if(!count)break;
                    if(!WriteFile(output.value,buffer.data(),count,&written,nullptr)||written!=count)throw std::runtime_error("Cannot write renderer staging file");
                }
                if(!FlushFileBuffers(output.value))throw std::runtime_error("Cannot flush renderer staging file");
            }
            if(filehash::sha256(pending)!=entry.sha)throw std::runtime_error("Renderer staging hash differs from trusted payload");
            if(beforePublish)beforePublish(i);
            // No REPLACE_EXISTING: another writer cannot be overwritten after
            // preflight. Rollback tracks only files actually published here.
            if(!MoveFileExW(pending.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish renderer file; an occupied destination was preserved");
            pending.clear();pendingOwned=false;published.push_back(i);journal["files"][i]["published"]=true;catalog::atomicWrite(journalFile,journal);
        }
        refreshCatalogs(plan,false);journal["status"]="installed";catalog::atomicWrite(journalFile,journal);
    }catch(...){
        auto failure=std::current_exception();bool clean=true;
        if(pendingOwned&&!pending.empty()){std::error_code error;fs::remove(pending,error);if(error)clean=false;}
        for(auto it=published.rbegin();it!=published.rend();++it){const auto& entry=plan.files[*it];auto target=catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path));
            try{if(present(target)){if(filehash::sha256(target)!=entry.sha||!fs::remove(target))clean=false;}}catch(...){clean=false;}
        }
        cleanDirectories(plan);try{refreshCatalogs(plan,true);}catch(...){}
        journal["status"]=clean?"rolled-back":"rollback-incomplete";try{catalog::atomicWrite(journalFile,journal);}catch(...){clean=false;}
        if(!clean)throw std::runtime_error("Renderer rollback could not complete; transaction evidence and modified files were retained");
        std::rethrow_exception(failure);
    }
}
inline void remove(const Options& options,bool verifyOnly=false){
    const auto root=rootPath(options.root),journalFile=journalPath(root);auto journal=catalog::read(journalFile);
    auto plan=makePlan(options,false,&journal);bindFiles(plan,journal);
    if(journal.at("status")=="removed")return;
    if(journal.at("status")!="installed"&&journal.at("status")!="installing"&&journal.at("status")!="removing"&&journal.at("status")!="rollback-incomplete"&&journal.at("status")!="rolled-back")throw std::runtime_error("Unsupported renderer transaction state");
    std::vector<filehash::Handle> leases;
    for(size_t i=0;i<plan.files.size();++i){
        if(!journal.at("files").at(i).at("published").get<bool>())continue;
        const auto& entry=plan.files[i];auto target=catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path));
        if(!present(target))continue;
        // Acquire deletion access to every file and deny writers before any
        // deletion; locked/modified files leave the entire transaction intact.
        filehash::Handle file(CreateFileW(target.c_str(),GENERIC_READ|(verifyOnly?0:DELETE),FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("An installed renderer file is in use; close its player/windows and retry");
        if(filehash::sha256(file.value)!=entry.sha)throw std::runtime_error("A modified renderer file was preserved; no package files were removed");
        leases.push_back(std::move(file));
    }
    if(verifyOnly)return;
    journal["status"]="removing";catalog::atomicWrite(journalFile,journal);
    for(size_t i=0;i<plan.files.size();++i){
        if(!journal.at("files").at(i).at("published").get<bool>())continue;
        const auto& entry=plan.files[i];auto target=catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path));
        if(present(target)&&!DeleteFileW(target.c_str()))throw std::runtime_error("Could not remove an owned renderer file; close its player/windows and retry");
    }
    leases.clear();cleanDirectories(plan);refreshCatalogs(plan,true);
    journal["status"]="removed";catalog::atomicWrite(journalFile,journal);
}
}
