#define UNICODE
#include <windows.h>
#include <bcrypt.h>
#include <winver.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string_view>
#include "catalog.hpp"
#include "package_install.hpp"
#include "vlc_original.hpp"
#include "windows_install_support.hpp"
namespace fs=std::filesystem;using catalog::json;
static std::string hash(const fs::path& file){return filehash::sha256(file);}
static void replaceFile(const fs::path& staged,const fs::path& target,const std::string& relative){
    DWORD error=0;
    for(unsigned attempt=0;attempt<8;++attempt){
        if(MoveFileExW(staged.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return;
        error=GetLastError();
        if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION&&error!=ERROR_ACCESS_DENIED)break;
        Sleep(50*(attempt+1));
    }
    throw std::runtime_error("Cannot replace "+relative+" (Windows error "+std::to_string(error)+"). Close VLC and retry.");
}
static std::string version(const fs::path& file){DWORD ignored=0,size=GetFileVersionInfoSizeW(file.c_str(),&ignored);if(!size)throw std::runtime_error("VLC file version is missing");std::vector<char> bytes(size);if(!GetFileVersionInfoW(file.c_str(),0,size,bytes.data()))throw std::runtime_error("Cannot read VLC version");VS_FIXEDFILEINFO* info=nullptr;UINT length=0;if(!VerQueryValueW(bytes.data(),L"\\",(void**)&info,&length)||length<sizeof(*info))throw std::runtime_error("VLC version resource is invalid");return std::to_string(HIWORD(info->dwFileVersionMS))+"."+std::to_string(LOWORD(info->dwFileVersionMS))+"."+std::to_string(HIWORD(info->dwFileVersionLS));}
static fs::path journalFile(const fs::path& root){return root/L".visualizers-backup"/L"transaction.json";}
static constexpr std::string_view qtBackend="visualizers/qt/bin/libqt_backend.dll";
static constexpr std::string_view coreName="libvlccore.dll";
static bool dualSubsVlcFile(const std::string& path){
    return path==coreName||path=="libvlc.dll"||path=="plugins/codec/liblibass_plugin.dll"||
        path=="plugins/codec/libsubsdec_plugin.dll"||path=="plugins/codec/libsubstx3g_plugin.dll"||
        path=="plugins/codec/libttml_plugin.dll"||path=="plugins/codec/libwebvtt_plugin.dll";
}
static bool sameCore(const json& journal,const std::string& digest){
    if(digest==journal.at("core_sha256"))return true;
    const auto& files=journal.at("files");
    return std::any_of(files.begin(),files.end(),[&digest](const json& record){
        return record.at("path").get<std::string>()==coreName&&record.at("new_sha256")==digest;
    });
}
static bool stockVlc(const fs::path& file){
    if(!catalog::amd64(file))return false;
    try{const auto info=vlc_original::version(file);return info.vlcFilename&&info.type==VFT_APP&&version(file)=="3.0.24";}
    catch(const std::exception&){return false;}
}
static void restore(const fs::path& root,bool rollback=false,bool recoveredStock=false){
    auto journal=catalog::read(journalFile(root));if(journal.at("schema_version")!=1)throw std::runtime_error("Unsupported backup journal");
    const bool matchingCore=rollback||sameCore(journal,hash(root/L"libvlccore.dll"));
    if(!matchingCore&&!recoveredStock)throw std::runtime_error("VLC was updated after patching; refusing to mix the old executable with a new core");
    std::set<std::string> retained;
    // Preflight every file and backup before changing any installed file.
    for(const auto& record:journal.at("files")){
        const auto relative=record.at("path").get<std::string>();
        auto target=catalog::contained(root,catalog::wide(relative));
        if(recoveredStock&&relative=="vlc.exe"){retained.insert(relative);continue;}
        const bool hadOriginal=record.at("existed").get<bool>();
        if(fs::exists(target)){
            auto current=hash(target);
            const bool unchanged=current==record.at("new_sha256");
            if(recoveredStock&&hadOriginal&&relative=="plugins/gui/libqt_plugin.dll"&&!unchanged){
                retained.insert(relative);continue; // VLC already supplied this file again.
            }
            if(!unchanged&&current!=record.value("old_sha256",std::string{}))throw std::runtime_error("A patched file was modified after installation; originals and current files were preserved");
        }
        if(hadOriginal&&!matchingCore)throw std::runtime_error("A previous patch still owns a VLC file from a different core; reinstall stock VLC first");
        if(record.at("existed").get<bool>()){auto backup=catalog::contained(root/L".visualizers-backup",catalog::wide(record.at("backup")));if(!fs::is_regular_file(backup)||hash(backup)!=record.at("old_sha256"))throw std::runtime_error("Backup verification failed; no files were restored");}
    }
    for(auto it=journal["files"].rbegin();it!=journal["files"].rend();++it){const auto& record=*it;auto target=catalog::contained(root,catalog::wide(record.at("path")));
        if(retained.count(record.at("path").get<std::string>()))continue;
        if(record["existed"].get<bool>()){if(fs::exists(target)&&hash(target)==record.at("old_sha256"))continue;auto original=catalog::contained(root/L".visualizers-backup",catalog::wide(record.at("backup")));auto temporary=target;temporary+=L".restore.tmp";fs::copy_file(original,temporary,fs::copy_options::overwrite_existing);replaceFile(temporary,target,record.at("path").get<std::string>());}
        else if(fs::exists(target)&&!fs::remove(target))throw std::runtime_error("Could not remove a patched file; close VLC and retry uninstall");
    }
    journal["status"]="restored";catalog::atomicWrite(journalFile(root),journal);
}
static json selectedFiles(const json& package,const std::wstring& selection,const fs::path& root,
        const std::set<std::string>& replaced,bool keepExisting,bool& dualSubs){
    auto files=package.at("files");
    if(!files.is_array())throw std::runtime_error("Invalid core payload table");
    auto names=catalog::utf8(selection);
    if(names.empty())return files;
    if(names.back()==',')throw std::runtime_error("Invalid component selection");
    std::set<std::string> selected;std::istringstream input(names);std::string name;
    while(std::getline(input,name,',')){
        if((name!="geiss"&&name!="milkdrop"&&name!="terrain"&&name!="beach"&&name!="objects"&&name!="undersea"&&name!="coralcanyon"&&name!="dualsubs")
           ||!selected.insert(name).second)
            throw std::runtime_error("Unknown or repeated component selection");
        const auto& component=package.at("components").at(name);
        if(!component.is_array())throw std::runtime_error("Invalid component payload table");
        if(name=="dualsubs"){
            dualSubs=true;
            const auto isBackend=[](const json& item){return item.at("path").get<std::string>()==qtBackend;};
            auto backend=std::find_if(files.begin(),files.end(),isBackend);
            auto alternate=std::find_if(component.begin(),component.end(),isBackend);
            if(backend==files.end()||alternate==component.end())
                throw std::runtime_error("The DualSubs payload is missing a required Qt backend");
            *backend=*alternate;
            for(const auto& row:component){
                const auto path=row.at("path").get<std::string>();
                if(path==qtBackend)continue;
                if(!dualSubsVlcFile(path)&&path.rfind("visualizers/licenses/dualsubs/",0)!=0)
                    throw std::runtime_error("Unexpected destination in the DualSubs payload");
                files.push_back(row);
            }
            continue;
        }
        const bool conflict=std::any_of(component.begin(),component.end(),[&](const auto& record){
            const auto relative=record.at("path").template get<std::string>();
            return !replaced.count(relative)&&fs::exists(catalog::contained(root,catalog::wide(relative)));
        });
        if(conflict){
            if(!keepExisting)throw std::runtime_error("A selected renderer component is already installed ("+name+"); uninstall its owner or deselect it");
            std::cout<<"Keep separately installed renderer: "<<name<<'\n';
            continue;
        }
        for(const auto& record:component)files.push_back(record);
    }
    return files;
}
static void patch(const fs::path& root,const fs::path& payload,const std::wstring& selection,bool recover){
    windows_install::TransactionLock lock;
    if(!windows_install::runningPlayers(root).empty())throw std::runtime_error("VLC is running. Close it before patching.");
    const auto executable=root/L"vlc.exe",core=root/L"libvlccore.dll";
    const bool stock=stockVlc(executable);
    json previous;
    std::set<std::string> replaced;
    if(fs::exists(journalFile(root))){
        const auto existing=catalog::read(journalFile(root));
        if(existing.at("schema_version")!=1)throw std::runtime_error("Unsupported backup journal");
        const auto status=existing.at("status").get<std::string>();
        if(status!="restored"){
            if(!recover)throw std::runtime_error("An addon transaction already exists; uninstall or repair it before patching again");
            if(status!="installed"&&status!="installing")throw std::runtime_error("Unsupported previous addon transaction state");
            previous=existing;
            bool launcher=false;
            for(const auto& row:existing.at("files")){
                const auto name=row.at("path").get<std::string>();
                if(name!="vlc.exe"&&name!="vlc.visualizers.original.exe"&&name!="plugins/gui/libqt_plugin.dll"&&
                    name!="plugins/audio_filter/libvlc_visualizers_plugin.dll"&&name.rfind("visualizers/",0)!=0&&
                    !dualSubsVlcFile(name))
                    throw std::runtime_error("Unexpected path in the previous addon transaction");
                if(name=="vlc.exe"){
                    launcher=true;
                    if(!stock){
                        if(!row.at("existed").get<bool>()||!fs::is_regular_file(executable)||hash(executable)!=row.at("new_sha256"))
                            throw std::runtime_error("The previous launcher was modified; reinstall stock VLC before continuing");
                        const auto original=catalog::contained(root/L".visualizers-backup",catalog::wide(row.at("backup")));
                        if(!stockVlc(original)||hash(original)!=row.at("old_sha256"))
                            throw std::runtime_error("The previous launcher backup does not match stock VLC 3.0.24 AMD64");
                    }
                }
                if(!row.at("existed").get<bool>())replaced.insert(name);
            }
            if(!launcher)throw std::runtime_error("Previous addon transaction is missing its launcher record");
        }
    }
    if((!stock&&previous.is_null())||!catalog::amd64(core)||version(core)!="3.0.24")
        throw std::runtime_error("Select a VLC 3.0.24 AMD64 installation");
    auto package=catalog::read(payload/L"payload.json");
    if(package.at("schema_version")!=1)throw std::runtime_error("Unsupported VLC addon payload");
    bool dualSubs=false;
    auto files=selectedFiles(package,selection,root,replaced,recover,dualSubs);
    if(fs::exists(root/L"DualSubs"/L"install-manifest.json")||fs::exists(root/L"Uninstall DualSubs.exe"))
        throw std::runtime_error("Uninstall the standalone DualSubs overlay before installing the combined add-on");
    const auto originalCore=previous.is_null()?hash(core):previous.at("core_sha256").get<std::string>();
    if(dualSubs&&originalCore!=package.at("dualsubs").at("stock_core_sha256"))
        throw std::runtime_error("The bundled DualSubs overlay requires the original VLC 3.0.24 AMD64 core");
    // Bundled renderers are new package components. Never take ownership from
    // an independent renderer installer by backing up/replacing its files.
    for(const auto& item:files){
        auto relative=item.at("path").get<std::string>();auto target=catalog::contained(root,catalog::wide(relative));
        if(relative.rfind("visualizers/plugins/",0)==0&&!replaced.count(relative)&&fs::exists(target))
            throw std::runtime_error("A selected renderer component is already installed; uninstall its owner or deselect it");
    }
    std::set<std::string> targets;
    for(const auto& item:files){
        const auto relative=item.at("path").get<std::string>();
        if(!targets.insert(relative).second||relative=="vlc.visualizers.original.exe")
            throw std::runtime_error("Duplicate or reserved patch destination");
        const auto source=catalog::contained(payload,catalog::wide(item.at("source")));
        if(hash(source)!=item.at("sha256"))throw std::runtime_error("The installer payload did not pass its hash check");
    }
    if(!targets.count("vlc.exe"))throw std::runtime_error("The installer payload has no VLC launcher");
    if(!previous.is_null())restore(root,false,stock);
    if(!stockVlc(executable))throw std::runtime_error("The original VLC launcher could not be recovered");
    const auto currentCore=hash(core);
    if(dualSubs&&currentCore!=package.at("dualsubs").at("stock_core_sha256"))
        throw std::runtime_error("The original VLC 3.0.24 core could not be recovered");
    json journal={{"schema_version",1},{"status","preparing"},{"version",package.at("version")},
                  {"core_sha256",currentCore},{"files",json::array()}};
    const auto originalHash=hash(executable);
    const auto originalCopy=root/L"vlc.visualizers.original.exe";
    if(fs::exists(originalCopy)&&(!recover||!fs::is_regular_file(originalCopy)||hash(originalCopy)!=originalHash))
        throw std::runtime_error("The reserved original-executable path contains an unrecognized file");
    const fs::path generation=std::to_wstring(GetTickCount64());
    auto backupRoot=root/L".visualizers-backup";
    if(!fs::create_directories(backupRoot/generation))throw std::runtime_error("Cannot reserve a new addon backup directory");
    const auto prepared=backupRoot/generation/L"launcher.new";
    struct Cleanup {fs::path path;~Cleanup(){std::error_code ignored;fs::remove(path,ignored);}} cleanup{prepared};
    for(const auto& item:files){
        auto relative=item.at("path").get<std::string>();auto target=catalog::contained(root,catalog::wide(relative));auto source=catalog::contained(payload,catalog::wide(item.at("source")));
        auto digest=item.at("sha256").get<std::string>();
        if(relative=="vlc.exe"){
            fs::copy_file(source,prepared);
            windows_install::copyIcon(executable,prepared);
            digest=hash(prepared);
        }
        json record={{"path",relative},{"new_sha256",digest},{"existed",fs::exists(target)},{"source",item.at("source")}};
        if(record["existed"].get<bool>()){if(!fs::is_regular_file(target))throw std::runtime_error("Patch destination is not a regular file");auto backup=generation/L"files"/catalog::wide(relative);backup.make_preferred();fs::create_directories((backupRoot/backup).parent_path());fs::copy_file(target,backupRoot/backup,fs::copy_options::overwrite_existing);record["backup"]=catalog::utf8(backup.generic_wstring());record["old_sha256"]=hash(target);if(hash(backupRoot/backup)!=record["old_sha256"])throw std::runtime_error("Original backup failed verification");}
        journal["files"].push_back(record);
    }
    journal["files"].push_back({{"path","vlc.visualizers.original.exe"},{"new_sha256",originalHash},{"existed",false},{"original_copy",true}});
    journal["status"]="installing";catalog::atomicWrite(journalFile(root),journal);
    try {
        if(!fs::exists(originalCopy))fs::copy_file(executable,originalCopy);
        for(const auto& item:journal.at("files")){
            if(item.value("original_copy",false))continue;
            const auto name=item.at("path").get<std::string>();
            auto target=catalog::contained(root,catalog::wide(name));
            auto source=name=="vlc.exe"?prepared:catalog::contained(payload,catalog::wide(item.at("source")));
            fs::create_directories(target.parent_path());auto staged=target;staged+=L".patch.tmp";
            fs::copy_file(source,staged,fs::copy_options::overwrite_existing);
            if(hash(staged)!=item.at("new_sha256"))throw std::runtime_error("Cannot verify the staged VLC addon file");
            replaceFile(staged,target,name);
        }
        journal["status"]="installed";catalog::atomicWrite(journalFile(root),journal);
    }catch(...){
        try{restore(root,true);}catch(...){}
        // An in-use destination can leave our staged payload behind. Only remove
        // a staging file if its bytes still match this transaction's payload.
        for(const auto& item:journal.at("files")){try{if(item.value("original_copy",false))continue;auto staged=catalog::contained(root,catalog::wide(item.at("path")));staged+=L".patch.tmp";if(fs::is_regular_file(staged)&&hash(staged)==item.at("new_sha256"))fs::remove(staged);}catch(...){}}
        throw;
    }
}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc<2)throw std::runtime_error("Usage: VisualizersCtl scan|patch|restore|verify --suite PATH / --root PATH --payload PATH");
        std::wstring action=argv[1],selection;bool recover=false,force=false;
        std::map<std::wstring,fs::path> options;
        if(argc%2!=0)throw std::runtime_error("Expected option/value pairs");
        for(int n=2;n+1<argc;n+=2){const std::wstring option=argv[n],value=argv[n+1];
            if(option==L"--components")selection=value;
            else if(option==L"--recover")recover=value==L"yes";
            else if(option==L"--force-close")force=value==L"yes";
            else options[option]=value.empty()?fs::path{}:catalog::extended(value);
        }
        if(action==L"scan"){auto result=catalog::scan(options.at(L"--suite"));std::cout<<result.dump(2)<<'\n';return 0;}
        if(action==L"check"){auto suite=options.at(L"--suite");auto package=catalog::contained(suite,fs::relative(options.at(L"--package"),suite));std::cout<<catalog::validate(suite,package).dump()<<'\n';return 0;}
        if(action==L"architecture"){auto file=options.at(L"--file");std::ifstream in(file,std::ios::binary);IMAGE_DOS_HEADER dos{};in.read((char*)&dos,sizeof(dos));if(!in||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)return 2;in.seekg(dos.e_lfanew);DWORD signature;IMAGE_FILE_HEADER pe{};in.read((char*)&signature,4);in.read((char*)&pe,sizeof(pe));if(!in||signature!=IMAGE_NT_SIGNATURE)return 2;return pe.Machine==IMAGE_FILE_MACHINE_I386?0:pe.Machine==IMAGE_FILE_MACHINE_AMD64?1:2;}
        if(action==L"package-install"||action==L"package-remove"||action==L"package-verify"){
            renderer_install::Options request{options.at(L"--root"),options.at(L"--payload"),options[L"--vlc-root"],options[L"--winamp-root"],selection,options[L"--menu-root"]};
            if(action==L"package-install")renderer_install::install(request);else renderer_install::remove(request,action==L"package-verify");
            std::cout<<"OK\n";return 0;
        }
        auto root=catalog::extended(fs::weakly_canonical(options.at(L"--root")));if(root==root.root_path())throw std::runtime_error("A drive root is not a VLC installation");
        if(action==L"running")return windows_install::runningPlayers(root).empty()?0:10;
        if(action==L"close-vlc")windows_install::closePlayers(root,force);
        else if(action==L"patch")patch(root,options.at(L"--payload"),selection,recover);
        else if(action==L"restore"){
            windows_install::TransactionLock lock;
            if(!windows_install::runningPlayers(root).empty())throw std::runtime_error("Close VLC before restoring its original files");
            restore(root);
        }
        else if(action==L"verify"){auto value=catalog::read(journalFile(root));for(const auto& f:value.at("files"))if(hash(catalog::contained(root,catalog::wide(f.at("path"))))!=f.at("new_sha256"))throw std::runtime_error("An installed file differs from the patch journal");}else throw std::runtime_error("Unknown action");
        std::cout<<"OK\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
