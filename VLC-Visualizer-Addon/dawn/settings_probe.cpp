// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings.hpp"
#include <atomic>
#include <chrono>
#include <clocale>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

using namespace visualizers;namespace fs=std::filesystem;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void put(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;if(!out)throw std::runtime_error("Cannot write test fixture");}
static std::string get(const fs::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});}
template<class F>static void rejects(F&& function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,message);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Provide an output directory for settings fixtures");
    const auto root=fs::absolute(argv[1])/("run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    rejects([&]{SettingsStore invalid(root,"../escape");},"Traversing settings ID was accepted");
    SettingsStore store(root/"json","terrain");const auto defaults=store.load();
    require(defaults.values["width"]==640&&defaults.values["height"]==480&&defaults.values["fps"]==60&&!defaults.values["fullscreen"].get<bool>()&&!defaults.values["hdr"].get<bool>(),"Default display preferences changed");
    require(!fs::exists(root/"json"),"Reading defaults created a profile");
    const SettingsJson fixture={{"width",960},{"height",540},{"fps",75},{"fullscreen",true},{"hdr",true},{"peak",1200},{"supersampling",2},{"engine","classic"},{"cameraSeconds",45.125},{"colorBeats",8},{"inputDevice","native:音声"},{"custom",{{"keep","plugin option"},{"array",{1,2,3}}}}};
    put(root/"json/terrain.json",fixture.dump());auto loaded=store.load();
    for(auto it=fixture.begin();it!=fixture.end();++it)require(loaded.values.at(it.key())==it.value(),"Existing valid JSON preference changed");
    store.savePatch({{"inputDevice","native:new"},{"concurrentOption",42}});
    const auto saved=store.savePatch({{"width",800}});
    require(saved.values["width"]==800&&saved.values["inputDevice"]=="native:new"&&saved.values["custom"]==fixture["custom"]&&saved.values["concurrentOption"]==42,"Changed-field save discarded concurrent or plugin settings");
    SettingsStore unicodePath(root/fs::path(u8"音声-profile"),"terrain");unicodePath.savePatch({{"width",960},{"inputDevice","native:音声"}});
    require(unicodePath.load().values["width"]==960&&unicodePath.load().values["inputDevice"]=="native:音声","Unicode profile path or device preference did not round-trip");
    validateSettingsPatch(settingsFields("terrain"),{{"cameraSeconds",45.125},{"colorBeats",8}});
    rejects([&]{store.savePatch({{"width",5000}});},"Out-of-range GUI value was saved");
    rejects([&]{validateSettingsPatch(settingsFields("geiss"),{{"engine","unknown"}});},"Unknown engine was accepted");
    const auto normalized=normalizeSettings({{"width","99999"},{"height",-1},{"fps","invalid"},{"hdr","false"},{"cameraSeconds",45.125}});
    require(normalized["width"]==3840&&normalized["height"]==240&&normalized["fps"]==60&&!normalized["hdr"].get<bool>()&&normalized["cameraSeconds"]==45.125,"Settings normalization failed");
    for(const auto& invalid:std::vector<std::string>{"", "NaN", "Infinity", "12abc", "+-1", "1e9999"})rejects([&]{settingNumber(invalid);},"Invalid numeric edit was accepted");
    for(const auto& invalid:std::vector<std::string>{"0x1p2","1,5","+ 1","1 2","--1","1e+","1e-9999",std::string("1\0tail",6)})rejects([&]{settingNumber(invalid);},"Nondecimal/partial/out-of-range numeric edit was accepted");
    for(const auto& [text,expected]:std::vector<std::pair<std::string,double>>{{" +45.125 ",45.125},{".5",.5},{"-.5",-.5},{"1.",1},{"1.5e2",150},{"1e-310",1e-310}})
        require(settingNumber(text)==expected,"Finite decimal/exponent/subnormal edit changed");
    require(std::signbit(settingNumber("-0")),"Negative zero sign was lost");
#ifdef __APPLE__
    const std::string oldLocale=std::setlocale(LC_NUMERIC,nullptr);
    require(std::setlocale(LC_NUMERIC,"fr_FR.UTF-8")!=nullptr,"French numeric locale fixture unavailable");
    require(settingNumber("45.125")==45.125,"Decimal parsing depends on process numeric locale");
    rejects([&]{settingNumber("45,125");},"Process locale changed the persisted-number grammar");
    std::setlocale(LC_NUMERIC,oldLocale.c_str());
#endif
    const std::string ini="[other]\nwidth=2000\n[terrain]\nwidth=800\nheight=600\nfps=75\nfullscreen=1\nhdr=1\npeak=1200\n";
    put(root/"ini/settings.ini",ini);SettingsStore legacy(root/"ini","terrain");
    require(legacy.load().importedIni&&legacy.load().values["fullscreen"]==true&&legacy.load().values["peak"]==1200,"Legacy INI import failed");
    legacy.savePatch({{"cameraSeconds",50}});require(get(root/"ini/settings.ini")==ini,"Import modified the original INI");
    put(root/"ini/settings.ini","[terrain]\nwidth=1200\n");require(!legacy.load().importedIni&&legacy.load().values["width"]==800,"Legacy INI overrode existing JSON");
    std::string utf16="\xff\xfe";for(char c:ini){utf16+=c;utf16+='\0';}put(root/"utf16/settings.ini",utf16);SettingsStore unicode(root/"utf16","terrain");require(unicode.load().values["width"]==800,"UTF-16 Windows INI import failed");
    const std::string broken="{\"width\":";put(root/"broken/terrain.json",broken);SettingsStore bad(root/"broken","terrain");
    rejects([&]{bad.load();},"Corrupt JSON was silently accepted");rejects([&]{bad.savePatch({{"width",800}});},"Corrupt JSON was overwritten");require(get(root/"broken/terrain.json")==broken,"Failed save changed corrupt source data");
    put(root/"large/terrain.json",std::string(1024*1024+1,' '));rejects([&]{SettingsStore(root/"large","terrain").load();},"Oversized settings were accepted");
    const std::string nested="{\"unknown\":"+std::string(40,'[')+"0"+std::string(40,']')+"}";put(root/"deep/terrain.json",nested);rejects([&]{SettingsStore(root/"deep","terrain").load();},"Excessive JSON nesting was accepted");
    SettingsStore concurrent(root/"concurrent","terrain");concurrent.savePatch({{"baseline",1}});
    std::atomic<bool> stop=false;std::exception_ptr failure;std::mutex errorMutex;
    auto remember=[&](){std::lock_guard lock(errorMutex);if(!failure)failure=std::current_exception();};
    std::thread reader([&]{try{while(!stop.load(std::memory_order_acquire))require(concurrent.load().values["baseline"]==1,"Concurrent read saw partial settings");}catch(...){remember();}});
    std::vector<std::thread> writers;for(unsigned worker=0;worker<4;++worker)writers.emplace_back([&,worker]{try{for(unsigned n=0;n<20;++n)concurrent.savePatch({{"worker"+std::to_string(worker),n}});}catch(...){remember();}});
    for(auto& writer:writers)writer.join();stop.store(true,std::memory_order_release);reader.join();if(failure)std::rethrow_exception(failure);
    const auto final=concurrent.load();for(unsigned worker=0;worker<4;++worker)require(final.values.at("worker"+std::to_string(worker))==19,"Concurrent save lost another writer's key");
    for(const auto& item:fs::recursive_directory_iterator(root))require(item.path().filename().u8string().find(u8".tmp-")==std::u8string::npos,"Temporary settings file was not cleaned up");
    std::cout<<"Native settings: JSON/unknown fields, INI/UTF-16 migration, bounds, corruption protection and 80 concurrent atomic saves pass\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
