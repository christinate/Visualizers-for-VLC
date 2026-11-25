// SPDX-License-Identifier: GPL-2.0-or-later
#include "install_v2.hpp"
#include <iostream>
namespace fs=std::filesystem;using native::json;
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void writeFile(const fs::path& path,const std::string& bytes){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<bytes;if(!out)throw std::runtime_error("Fixture write failed");}
static std::string readFile(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
template<class F>static void rejected(F callback){bool caught=false;try{callback();}catch(const std::exception&){caught=true;}require(caught,"Expected rejection was accepted");}
static auto snapshot(const fs::path& root){std::map<std::string,std::string> result;for(const auto& item:fs::recursive_directory_iterator(root))if(item.is_symlink()||item.is_regular_file())result[fs::relative(item.path().parent_path(),root).string()+"/"+item.path().filename().string()]=native::contentDigest(item.path());return result;}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Use a fresh output directory");auto base=fs::absolute(argv[1]);require(!fs::exists(base),"Output exists");fs::create_directories(base);auto source=base/"source";
    const auto runtime=suite::nativeRuntimePath(fs::path{}).generic_string();
    writeFile(source/"runtime.dat","runtime content");writeFile(source/runtime,"runtime executable fixture");writeFile(source/"native/libexample.so.1.0","shared library fixture");fs::create_symlink("libexample.so.1.0",source/"native/libexample.so.1");
    writeFile(source/"README.md","package readme");writeFile(source/"VlcVisualizerLauncher","launcher fixture");
    json manifest={{"schema_version",2},{"api_version",2},{"id","geiss"},{"name","Installer fixture"},{"version","test"},{"platform",native::platform()},{"architecture",native::architecture()},{"adapter","opengl"},{"entrypoint","engine.so"}};
    writeFile(source/"plugins/geiss/manifest.json",manifest.dump());std::string elf(512,'\0');
#ifdef __APPLE__
    const uint32_t magic=0xfeedfacf,cpu=std::string(native::architecture())=="arm64"?0x0100000c:0x01000007;
    std::memcpy(elf.data(),&magic,sizeof(magic));std::memcpy(elf.data()+4,&cpu,sizeof(cpu));
#else
    elf[0]=0x7f;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=2;elf[5]=1;elf[18]=std::string(native::architecture())=="arm64"?183:62;
#endif
    writeFile(source/"plugins/geiss/engine.so",elf);
    auto rows=[&](std::initializer_list<const char*> paths){json out=json::array();for(auto path:paths)out.push_back(native::installer2::describe(source/path,path));return out;};
    json payload={{"schema_version",2},{"version","test"},{"package_id","addon"},{"kind","suite"},{"platform",native::platform()},{"architecture",native::architecture()},
      {"components",{{"runtime",rows({"runtime.dat",runtime.c_str(),"native/libexample.so.1.0","native/libexample.so.1"})},{"addon",rows({"README.md","VlcVisualizerLauncher"})},{"geiss",rows({"plugins/geiss/manifest.json","plugins/geiss/engine.so"})}}}};
    writeFile(source/"native-payload.json",payload.dump());size_t checks=0;
    {native::FileDigest hash;hash.append("abc",3);require(hash.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 vector differs");++checks;}
    {
        auto suite=base/"normal";writeFile(suite/"README.md","original user readme");native::installer2::install(suite,source,"","");auto before=snapshot(suite);
        auto file=suite/"plugins/geiss/manifest.json";auto saved=readFile(file);writeFile(file,saved+" ");before=snapshot(suite);
        rejected([&]{native::installer2::uninstall(suite,source);});require(snapshot(suite)==before,"Whole-package modified-file refusal changed files");writeFile(file,saved);
        auto link=suite/"native/libexample.so.1";fs::remove(link);fs::create_symlink("different.so",link);before=snapshot(suite);
        rejected([&]{native::installer2::uninstall(suite,source);});require(snapshot(suite)==before,"Symlink refusal changed files");fs::remove(link);fs::create_symlink("libexample.so.1.0",link);
        native::installer2::uninstall(suite,source);require(readFile(suite/"README.md")=="original user readme","Original readme not restored");require(!fs::exists(suite/"VlcVisualizerLauncher"),"Owned file remains");checks+=3;
    }
    for(size_t fail=0;fail<8;++fail){
        auto suite=base/("failure-"+std::to_string(fail));writeFile(suite/"README.md","original user readme");
        rejected([&]{native::installer2::install(suite,source,"","",[&](size_t index){if(index==fail)throw std::runtime_error("Controlled publication fault");});});
        require(readFile(suite/"README.md")=="original user readme","Failure rollback lost original");require(!fs::exists(suite/"runtime.dat")&&!fs::exists(suite/"VlcVisualizerLauncher")&&!fs::exists(suite/"plugins/geiss/manifest.json"),"Failure rollback left owned files");++checks;
    }
    {
        auto suite=base/"foreign-new";fs::create_directories(suite);
        rejected([&]{native::installer2::install(suite,source,"","",[&](size_t index){if(index==0)writeFile(suite/"runtime.dat","runtime content");});});
        require(readFile(suite/"runtime.dat")=="runtime content","Competing identical destination was removed");++checks;
    }
    {
        auto suite=base/"changed-original";writeFile(suite/"README.md","original user readme");
        rejected([&]{native::installer2::install(suite,source,"","",[&](size_t index){if(index==4)writeFile(suite/"README.md","concurrent user edit");});});
        require(readFile(suite/"README.md")=="concurrent user edit","Concurrent original-file edit was overwritten");++checks;
    }
    json report={{"result","pass"},{"checks",checks},{"scope","native transaction fault/ownership fixtures; no renderer/GPU/player test"}};writeFile(base/"result.json",report.dump(2)+"\n");std::cout<<report.dump()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
