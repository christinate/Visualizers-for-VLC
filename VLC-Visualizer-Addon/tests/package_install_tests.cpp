// SPDX-License-Identifier: GPL-2.0-or-later
#define UNICODE
#include "package_install.hpp"
#include <iostream>
namespace fs=std::filesystem;using catalog::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void write(const fs::path& file,const std::string& text){auto native=file;native.make_preferred();fs::create_directories(native.parent_path());std::ofstream out(native,std::ios::binary);out<<text;if(!out)throw std::runtime_error("Fixture write failed");}
static void pe(const fs::path& file,WORD machine){
    std::string bytes(512,'\0');IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=128;
    std::memcpy(bytes.data(),&dos,sizeof(dos));DWORD signature=IMAGE_NT_SIGNATURE;std::memcpy(bytes.data()+128,&signature,4);
    IMAGE_FILE_HEADER header{};header.Machine=machine;std::memcpy(bytes.data()+132,&header,sizeof(header));write(file,bytes);
}
static auto snapshot(const fs::path& root){
    std::map<std::wstring,std::string> result;if(!fs::exists(root))return result;
    for(const auto& item:fs::recursive_directory_iterator(root))if(item.is_regular_file())result[fs::relative(item.path(),root).wstring()]=filehash::sha256(item.path());
    return result;
}
template<class F>static void fails(F call){bool failed=false;try{call();}catch(const std::exception&){failed=true;}require(failed,"Expected rejection was accepted");}
static json row(const fs::path& payload,const std::string& source,const std::string& target){return {{"path",target},{"source",source},{"sha256",filehash::sha256(payload/catalog::wide(source))}};}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc!=2)throw std::runtime_error("Usage: package_install_tests NEW-OUTPUT-DIRECTORY");
        auto base=fs::absolute(argv[1]);require(!fs::exists(base),"Use a fresh test output directory");fs::create_directories(base);
        auto payload=base/L"payload";write(payload/L"runtime/Visualizers.exe","test runtime");write(payload/L"runtime/VisualizersCtl.exe","test scanner");
        write(payload/L"plugins/milkdrop/manifest.json",json({{"schema_version",1},{"api_version",1},{"id","milkdrop"},{"name","Transaction fixture"},{"version","test"},{"architecture","x86_64"},{"adapter","winamp"},{"entrypoint","vis_milkdrop64.dll"}}).dump());
        pe(payload/L"plugins/milkdrop/vis_milkdrop64.dll",IMAGE_FILE_MACHINE_AMD64);
        write(payload/L"shortcuts/Fixture.lnk","generated launch shortcut");write(payload/L"shortcuts/Fixture settings.lnk","generated settings shortcut");
        write(payload/L"bridges/x86/bridge.dll","x86 bridge fixture");write(payload/L"bridges/x64/bridge.dll","x64 bridge fixture");
        json manifest={{"schema_version",1},{"id","milkdrop"},{"version","test"},
          {"runtime",json::array({row(payload,"runtime/Visualizers.exe","Visualizers.exe"),row(payload,"runtime/VisualizersCtl.exe","VisualizersCtl.exe")})},
          {"plugin",json::array({row(payload,"plugins/milkdrop/manifest.json","plugins/milkdrop/manifest.json"),row(payload,"plugins/milkdrop/vis_milkdrop64.dll","plugins/milkdrop/vis_milkdrop64.dll")})},
          {"bridges",{{"x86",row(payload,"bridges/x86/bridge.dll","Plugins/vis_milkdrop_bridge.dll")},{"x64",row(payload,"bridges/x64/bridge.dll","Plugins/vis_milkdrop_bridge.dll")}}}};
        manifest["shortcuts"]=json::array({{{"path","Fixture.lnk"},{"source","shortcuts/Fixture.lnk"}},{{"path","Fixture settings.lnk"},{"source","shortcuts/Fixture settings.lnk"}}});
        write(payload/L"renderer-payload.json",manifest.dump());size_t count=0;
        auto options=[&](const std::wstring& name,const std::wstring& selected=L"standalone,vlc,winamp",WORD machine=IMAGE_FILE_MACHINE_I386){
            auto root=base/name;renderer_install::Options request{root/L"local",payload,root/L"vlc",root/L"winamp",selected,root/L"menu"};
            write(request.vlc/L"visualizers/native/VisualizerNative.exe","existing addon runtime");write(request.vlc/L"visualizers/VisualizersCtl.exe","existing addon scanner");
            pe(request.winamp/L"winamp.exe",machine);write(root/L"user-notes.txt","retain unrelated user files");return request;
        };
        for(unsigned mask=1;mask<8;++mask)for(auto machine:{IMAGE_FILE_MACHINE_I386,IMAGE_FILE_MACHINE_AMD64}){
            std::wstring selected;for(unsigned i=0;i<3;++i)if(mask&(1u<<i)){if(!selected.empty())selected+=L",";selected+=std::array<const wchar_t*,3>{L"standalone",L"vlc",L"winamp"}[i];}
            auto request=options(L"selection-"+std::to_wstring(mask)+L"-"+std::to_wstring(machine),selected,WORD(machine));
            auto plan=renderer_install::makePlan(request,true);renderer_install::install(request);renderer_install::remove(request,true);
            for(const auto& entry:plan.files)require(filehash::sha256(catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path)))==entry.sha,"Installed bytes differ");
            renderer_install::remove(request);for(const auto& entry:plan.files)require(!fs::exists(catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path))),"Owned file remains");
            require(fs::is_regular_file(request.winamp/L"winamp.exe"),"Original player was removed");++count;
        }
        {
            auto request=options(L"occupied");write(request.winamp/L"Plugins/vis_milkdrop_bridge.dll","existing user bridge");auto before=snapshot(base/L"occupied");
            fails([&]{renderer_install::install(request);});require(snapshot(base/L"occupied")==before,"Occupied destination refusal changed files");++count;
        }
        {
            auto request=options(L"bad-source");write(payload/L"runtime/Visualizers.exe","changed source");auto before=snapshot(base/L"bad-source");
            fails([&]{renderer_install::install(request);});require(snapshot(base/L"bad-source")==before,"Bad source refusal changed destinations");write(payload/L"runtime/Visualizers.exe","test runtime");++count;
        }
        {
            auto request=options(L"modified");renderer_install::install(request);auto file=request.winamp/L"Plugins/vis_milkdrop_bridge.dll";auto original=filehash::sha256(file);
            write(file,"modified user bridge");auto before=snapshot(base/L"modified");fails([&]{renderer_install::remove(request);});require(snapshot(base/L"modified")==before,"Modified-file refusal removed files");
            write(file,"x86 bridge fixture");require(filehash::sha256(file)==original,"Fixture restoration differs");renderer_install::remove(request);++count;
        }
        {
            auto request=options(L"locked");renderer_install::install(request);auto before=snapshot(base/L"locked");
            {filehash::Handle held(CreateFileW((request.winamp/L"Plugins/vis_milkdrop_bridge.dll").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));require(held.value!=INVALID_HANDLE_VALUE,"Cannot lock test bridge");
             fails([&]{renderer_install::remove(request);});require(snapshot(base/L"locked")==before,"Locked-file refusal changed files");}
            renderer_install::remove(request);++count;
        }
        {
            auto request=options(L"root-binding");renderer_install::install(request);auto before=snapshot(base/L"root-binding");auto changed=request;changed.vlc=base/L"wrong-vlc";
            fails([&]{renderer_install::remove(changed);});require(snapshot(base/L"root-binding")==before,"Wrong root changed files");renderer_install::remove(request);++count;
        }
        {
            auto request=options(L"journal-binding");renderer_install::install(request);auto journal=renderer_install::journalPath(request.root);auto saved=catalog::read(journal),altered=saved;altered["files"][0]["path"]="../user-notes.txt";catalog::atomicWrite(journal,altered);auto before=snapshot(base/L"journal-binding");
            fails([&]{renderer_install::remove(request);});require(snapshot(base/L"journal-binding")==before,"Malformed ownership changed files");catalog::atomicWrite(journal,saved);renderer_install::remove(request);++count;
        }
        {
            auto request=options(L"modified-shortcut");renderer_install::install(request);write(request.menu/L"Fixture.lnk","user-edited shortcut");auto before=snapshot(base/L"modified-shortcut");
            fails([&]{renderer_install::remove(request);});require(snapshot(base/L"modified-shortcut")==before,"Modified shortcut refusal changed files");
            write(request.menu/L"Fixture.lnk","generated launch shortcut");renderer_install::remove(request);++count;
        }
        {
            auto request=options(L"occupied-shortcut");write(request.menu/L"Fixture.lnk","pre-existing user shortcut");auto before=snapshot(base/L"occupied-shortcut");
            fails([&]{renderer_install::install(request);});require(snapshot(base/L"occupied-shortcut")==before,"Occupied shortcut refusal changed files");++count;
        }
        for(size_t fail=0;fail<9;++fail){
            auto request=options(L"failure-"+std::to_wstring(fail));auto plan=renderer_install::makePlan(request,true);
            fails([&]{renderer_install::install(request,[&](size_t index){if(index==fail)throw std::runtime_error("Controlled publication fault");});});
            for(const auto& entry:plan.files)require(!fs::exists(catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path))),"Rollback left an owned target");
            require(catalog::read(renderer_install::journalPath(request.root)).at("status")=="rolled-back","Rollback journal differs");
            for(const auto& item:fs::recursive_directory_iterator(base/(L"failure-"+std::to_wstring(fail))))require(item.path().filename().wstring().find(L".install.")==std::wstring::npos,"Rollback left a temporary file");++count;
        }
        {
            auto request=options(L"race");auto plan=renderer_install::makePlan(request,true);const auto foreign=catalog::contained(plan.roots.at(plan.files[2].anchor),catalog::wide(plan.files[2].path));
            fails([&]{renderer_install::install(request,[&](size_t index){if(index==2)fs::copy_file(payload/catalog::wide(plan.files[2].source),foreign);});});
            require(filehash::sha256(foreign)==plan.files[2].sha,"Racing identical foreign target was removed or overwritten");
            renderer_install::remove(request);require(fs::is_regular_file(foreign),"Rollback cleanup deleted an unpublished target");++count;
        }
        {
            auto sample=base/L"hash-slashes/source.bin";write(sample,"known streaming hash fixture");
            auto mixed=catalog::extended(sample).wstring();auto last=mixed.find_last_of(L'\\');require(last!=std::wstring::npos,"Missing path separator");mixed[last]=L'/';
            require(filehash::sha256(mixed)==filehash::sha256(sample),"Extended mixed-separator hash path differs");++count;
        }
        {
            auto request=options(L"unicode-\u65e5\u672c\u8a9e");request.root=catalog::extended(base/(L"long-"+std::wstring(140,L'x'))/L"\u97f3\u697d folder/local");
            auto plan=renderer_install::makePlan(request,true);renderer_install::install(request);renderer_install::remove(request,true);
            for(const auto& entry:plan.files)require(filehash::sha256(catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path)))==entry.sha,"Unicode/long path installed bytes differ");
            renderer_install::remove(request);for(const auto& entry:plan.files)require(!fs::exists(catalog::contained(plan.roots.at(entry.anchor),catalog::wide(entry.path))),"Unicode/long path owned file remains");++count;
        }
        json report={{"result","pass"},{"checks",count},{"scope","native multi-root filesystem transactions with minimal PE fixtures; not real Winamp playback or NSIS UI"}};
        write(base/L"result.json",report.dump(2)+"\n");std::cout<<report.dump()<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
