// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings.hpp"
#include "decimal_parse.hpp"
#include "scene_ini.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace visualizers {
namespace fs=std::filesystem;
namespace {
constexpr size_t limit=1024*1024;
bool validId(const std::string& id){return !id.empty()&&id.size()<=64&&id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos;}
std::string trim(std::string text){const auto start=text.find_first_not_of(" \t\r\n");if(start==std::string::npos)return {};return text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);}
std::string lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c+=32;return text;}
fs::path nativePath(const fs::path& value){
#ifdef _WIN32
    auto text=fs::absolute(value).lexically_normal().make_preferred().wstring();
    if(text.rfind(L"\\\\?\\",0)==0)return fs::path(text);
    return fs::path(text.rfind(L"\\\\",0)==0?L"\\\\?\\UNC\\"+text.substr(2):L"\\\\?\\"+text);
#else
    return fs::absolute(value).lexically_normal();
#endif
}
struct File {
#ifdef _WIN32
    HANDLE value=INVALID_HANDLE_VALUE;
    ~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    void close(){if(value!=INVALID_HANDLE_VALUE){CloseHandle(value);value=INVALID_HANDLE_VALUE;}}
#else
    int value=-1;
    ~File(){if(value>=0)::close(value);}
    void close(){if(value>=0){::close(value);value=-1;}}
#endif
};
std::optional<std::string> readFile(const fs::path& path){
    File file;
#ifdef _WIN32
    file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file.value==INVALID_HANDLE_VALUE){const auto code=GetLastError();if(code==ERROR_FILE_NOT_FOUND||code==ERROR_PATH_NOT_FOUND)return {};throw std::runtime_error("Cannot open settings file");}
#else
    file.value=open(path.c_str(),O_RDONLY|O_CLOEXEC);if(file.value<0){if(errno==ENOENT)return {};throw std::runtime_error("Cannot open settings file");}
    struct stat info{};if(fstat(file.value,&info)||!S_ISREG(info.st_mode))throw std::runtime_error("Settings must be a regular file");
#endif
    std::string out;char block[4096];
    for(;;){
#ifdef _WIN32
        DWORD count=0;if(!ReadFile(file.value,block,sizeof(block),&count,nullptr))throw std::runtime_error("Cannot read settings file");
#else
        const auto count=read(file.value,block,sizeof(block));if(count<0){if(errno==EINTR)continue;throw std::runtime_error("Cannot read settings file");}
#endif
        if(!count)break;if(out.size()+size_t(count)>limit)throw std::runtime_error("Settings file exceeds 1 MiB");out.append(block,size_t(count));
    }
    return out;
}
SettingsJson parse(const std::string& text){
    auto value=SettingsJson::parse(text,[](int depth,SettingsJson::parse_event_t,SettingsJson&){if(depth>32)throw std::runtime_error("Settings nesting exceeds its limit");return true;});
    if(!value.is_object())throw std::runtime_error("Settings must contain a JSON object");return value;
}
std::string iniText(const std::string& bytes){
    if(bytes.size()>=3&&bytes.compare(0,3,"\xef\xbb\xbf")==0)return bytes.substr(3);
    if(bytes.size()<2||!((uint8_t(bytes[0])==255&&uint8_t(bytes[1])==254)||(uint8_t(bytes[0])==254&&uint8_t(bytes[1])==255)))return bytes;
    if(bytes.size()%2)throw std::runtime_error("Legacy settings contain incomplete UTF-16");
    const bool little=uint8_t(bytes[0])==255;std::string out;
    auto unit=[&](size_t n){return little?unsigned(uint8_t(bytes[n]))|(unsigned(uint8_t(bytes[n+1]))<<8):(unsigned(uint8_t(bytes[n]))<<8)|unsigned(uint8_t(bytes[n+1]));};
    for(size_t n=2;n<bytes.size();n+=2){unsigned c=unit(n);
        if(c>=0xd800&&c<=0xdbff){if(n+3>=bytes.size())throw std::runtime_error("Invalid legacy UTF-16");const auto next=unit(n+2);if(next<0xdc00||next>0xdfff)throw std::runtime_error("Invalid legacy UTF-16");c=0x10000+((c-0xd800)<<10)+(next-0xdc00);n+=2;}
        else if(c>=0xdc00&&c<=0xdfff)throw std::runtime_error("Invalid legacy UTF-16");
        if(c<128)out+=char(c);else if(c<2048){out+=char(192|(c>>6));out+=char(128|(c&63));}
        else if(c<65536){out+=char(224|(c>>12));out+=char(128|((c>>6)&63));out+=char(128|(c&63));}
        else{out+=char(240|(c>>18));out+=char(128|((c>>12)&63));out+=char(128|((c>>6)&63));out+=char(128|(c&63));}}
    return out;
}
SettingsJson importIni(const std::string& bytes,const std::string& id){
    const auto text=iniText(bytes);SettingsJson out=SettingsJson::object();std::string section;
    size_t start=0;while(start<=text.size()){const auto end=text.find('\n',start);auto line=trim(text.substr(start,end==std::string::npos?end:end-start));
        if(!line.empty()&&line.front()!=';'&&line.front()!='#'){
            if(line.front()=='['&&line.back()==']')section=lower(trim(line.substr(1,line.size()-2)));
            else if(section==id){const auto equals=line.find('=');if(equals!=std::string::npos){const auto key=lower(trim(line.substr(0,equals)));
                if(key=="width"||key=="height"||key=="fps"||key=="peak"||key=="fullscreen"||key=="hdr")try{const auto number=settingNumber(line.substr(equals+1));if(key=="fullscreen"||key=="hdr")out[key]=number!=0;else out[key]=number;}catch(const std::exception&){} }}
        }
        if(end==std::string::npos)break;start=end+1;
    }
    return out;
}
double number(const SettingsJson& value,double fallback){try{if(value.is_number()){const auto n=value.get<double>();return std::isfinite(n)?n:fallback;}if(value.is_string())return settingNumber(value.get<std::string>());}catch(const std::exception&){}return fallback;}
bool boolean(const SettingsJson& value,bool fallback){if(value.is_boolean())return value.get<bool>();if(value.is_number())return number(value,0)!=0;if(value.is_string()){const auto text=lower(trim(value.get<std::string>()));if(text=="true"||text=="1")return true;if(text=="false"||text=="0")return false;}return fallback;}
void privateDirectory(const fs::path& path){const bool created=fs::create_directories(path);
#ifndef _WIN32
    if(created&&chmod(path.c_str(),0700))throw std::runtime_error("Cannot protect settings directory");
#else
    (void)created;
#endif
}
struct Lock {
    File file;
    explicit Lock(const fs::path& path){
#ifdef _WIN32
        file.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open settings lock");
#else
        file.value=open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0600);if(file.value<0)throw std::runtime_error("Cannot open settings lock");
#endif
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        for(;;){
#ifdef _WIN32
            OVERLAPPED range{};if(LockFileEx(file.value,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&range))break;
            if(GetLastError()!=ERROR_LOCK_VIOLATION)throw std::runtime_error("Cannot lock settings");
#else
            if(flock(file.value,LOCK_EX|LOCK_NB)==0)break;if(errno!=EWOULDBLOCK&&errno!=EAGAIN&&errno!=EINTR)throw std::runtime_error("Cannot lock settings");
#endif
            if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Settings are busy; retry saving");std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};
void writeText(const fs::path& path,const std::string& text){
    if(text.size()>limit)throw std::runtime_error("Saved settings exceed 1 MiB");
    static std::atomic<uint64_t> sequence{0};auto temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++);
    struct Cleanup {fs::path path;~Cleanup(){std::error_code ignored;fs::remove(path,ignored);}} cleanup{temporary};File file;
#ifdef _WIN32
    file.value=CreateFileW(temporary.c_str(),GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create temporary settings file");
    DWORD written=0;if(!WriteFile(file.value,text.data(),DWORD(text.size()),&written,nullptr)||written!=text.size()||!FlushFileBuffers(file.value))throw std::runtime_error("Cannot write settings");
    // MoveFileEx cannot replace an open destination even with SHARE_DELETE.
    // POSIX rename semantics retain readers' old snapshots while atomically
    // making the complete replacement available to subsequent readers.
    const auto name=path.wstring();std::vector<std::byte> storage(sizeof(FILE_RENAME_INFO)+name.size()*sizeof(wchar_t));
    auto* rename=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());rename->Flags=FILE_RENAME_FLAG_REPLACE_IF_EXISTS|FILE_RENAME_FLAG_POSIX_SEMANTICS;
    rename->FileNameLength=DWORD(name.size()*sizeof(wchar_t));std::memcpy(rename->FileName,name.data(),rename->FileNameLength);
    if(!SetFileInformationByHandle(file.value,FileRenameInfoEx,rename,DWORD(storage.size()))){
        const auto code=GetLastError();file.close();
        if(code!=ERROR_INVALID_PARAMETER&&code!=ERROR_NOT_SUPPORTED&&code!=ERROR_INVALID_FUNCTION)
            throw std::runtime_error("Cannot atomically replace settings (Windows error "+std::to_string(code)+")");
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot atomically replace settings (Windows error "+std::to_string(GetLastError())+")");
    }else file.close();
#else
    file.value=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(file.value<0)throw std::runtime_error("Cannot create temporary settings file");
    size_t written=0;while(written<text.size()){const auto count=write(file.value,text.data()+written,text.size()-written);if(count<0&&errno==EINTR)continue;if(count<=0)throw std::runtime_error("Cannot write settings");written+=size_t(count);}
    if(fsync(file.value))throw std::runtime_error("Cannot flush settings");file.close();if(rename(temporary.c_str(),path.c_str()))throw std::runtime_error("Cannot atomically replace settings");
    File directory;directory.value=open(path.parent_path().c_str(),O_RDONLY|O_CLOEXEC);if(directory.value>=0)fsync(directory.value);
#endif
}
void writeFile(const fs::path& path,const SettingsJson& data){writeText(path,data.dump(2)+'\n');}
constexpr size_t sceneLimit=32768;
void checkSceneText(const std::string& text){
    if(text.size()>sceneLimit||text.find('\0')!=std::string::npos)
        throw std::runtime_error("Scene INI must be a text file of at most 32 KiB");
    suite::SceneIni check;
    check.apply(text);
}
}
double settingNumber(const std::string& text){const auto value=trim(text);if(value.empty())throw std::runtime_error("Enter a number");double result=0;const char* first=value.data();if(*first=='+'){++first;if(first==value.data()+value.size()||*first=='-'||*first=='+')throw std::runtime_error("Enter a number");}
    if(!suite::finiteDecimal(std::string_view(first,size_t(value.data()+value.size()-first)),result))throw std::runtime_error("Enter a finite number");
    return result;}
SettingsJson normalizeSettings(const SettingsJson& raw){
    if(!raw.is_object())throw std::runtime_error("Settings must contain a JSON object");auto out=raw;
    for(const auto& item:std::vector<std::tuple<const char*,double,double,double>>{{"width",320,3840,640},{"height",240,2160,480},{"fps",15,144,60},{"peak",200,2000,600},{"cameraSeconds",30,180,60},{"colorBeats",1,16,4}}){
        const auto [key,min,max,fallback]=item;const double value=std::clamp(number(raw.value(key,SettingsJson()),fallback),min,max);if(std::string(key)=="cameraSeconds")out[key]=value;else out[key]=int(std::lround(value));}
    out["fullscreen"]=boolean(raw.value("fullscreen",SettingsJson()),false);out["hdr"]=boolean(raw.value("hdr",SettingsJson()),false);
    out["vsync"]=boolean(raw.value("vsync",SettingsJson()),false);
    out["supersampling"]=number(raw.value("supersampling",SettingsJson()),1)==2?2:1;
    out["engine"]=raw.value("engine",SettingsJson())=="classic"?"classic":"modern";
    const auto input=raw.value("inputDevice",SettingsJson());out["inputDevice"]=input.is_string()&&input.get_ref<const std::string&>().size()<=4096?input:SettingsJson("");return out;
}
bool hasSceneIni(const std::string& id){return id=="terrain"||id=="beach"||id=="objects"||id=="undersea"||id=="coralcanyon";}
std::vector<SettingField> settingsFields(const std::string& id,bool sceneEditor){
    if(!validId(id))throw std::runtime_error("Invalid visualizer settings ID");
    std::vector<SettingField> fields={{"fullscreen","Always open visualizer in full screen",SettingType::Boolean},{"width","Window width",SettingType::Integer,320,3840},
        {"height","Window height",SettingType::Integer,240,2160},{"fps","Frame limit",SettingType::Integer,15,144},
        {"vsync","VSync in fullscreen",SettingType::Boolean},{"hdr","HDR output — Experimental",SettingType::Boolean},{"peak","Peak brightness reference (nits)",SettingType::Integer,200,2000}};
    if(id=="geiss"){fields.push_back({"engine","Renderer",SettingType::Choice,0,0,{{"Prism Flow","modern"},{"Geiss Classic — compatibility","classic"}}});fields.push_back({"supersampling","Supersampling",SettingType::Choice,0,0,{{"1×",1},{"2×",2}}});}
    if(id=="terrain"){fields.push_back({"cameraSeconds","Camera mode duration (seconds)",SettingType::Number,30,180});fields.push_back({"colorBeats","Beats between color changes",SettingType::Integer,1,16});}
    if(sceneEditor&&hasSceneIni(id))fields.push_back({"sceneIni","Scene settings (INI)",SettingType::Ini});
    return fields;
}
void validateSettingsPatch(const std::vector<SettingField>& fields,const SettingsJson& patch){
    if(!patch.is_object())throw std::runtime_error("Settings changes must be an object");
    for(const auto& field:fields)if(patch.contains(field.key)){const auto& value=patch.at(field.key);bool valid=false;
        if(field.type==SettingType::Boolean)valid=value.is_boolean();
        else if(field.type==SettingType::Choice)valid=std::any_of(field.choices.begin(),field.choices.end(),[&](const auto& choice){return choice.value==value;});
        else if(field.type==SettingType::Ini&&value.is_string()){checkSceneText(value.get_ref<const std::string&>());valid=true;}
        else if(value.is_number()){const auto n=value.get<double>();valid=std::isfinite(n)&&n>=field.minimum&&n<=field.maximum&&(field.type!=SettingType::Integer||n==std::floor(n));}
        if(!valid)throw std::runtime_error("Check "+field.label+"; the value is outside its supported range");
    }
}
fs::path defaultSettingsProfile(){
#ifdef _WIN32
    const auto base=_wgetenv(L"LOCALAPPDATA");if(!base||!*base)throw std::runtime_error("LOCALAPPDATA is unavailable");return fs::path(base)/L"VLCVisualizers/profiles";
#else
    const auto home=getenv("HOME");if(!home||!*home)throw std::runtime_error("HOME is unavailable");
#ifdef __APPLE__
    return fs::path(home)/"Library/Application Support/VLCVisualizers/profiles";
#else
    const auto configured=getenv("XDG_CONFIG_HOME");const fs::path base=configured&&*configured&&fs::path(configured).is_absolute()?fs::path(configured):fs::path(home)/".config";return base/"vlc-visualizers/profiles";
#endif
#endif
}
SettingsStore::SettingsStore(fs::path profile,std::string id):profile_(nativePath(profile)),id_(std::move(id)){if(!validId(id_))throw std::runtime_error("Invalid visualizer settings ID");}
SettingsSnapshot SettingsStore::load()const{if(const auto text=readFile(profile_/(id_+".json")))return {normalizeSettings(parse(*text)),false};
    if(const auto text=readFile(profile_/"settings.ini"))return {normalizeSettings(importIni(*text,id_)),true};return {normalizeSettings(SettingsJson::object()),false};}
std::string SettingsStore::sceneOverrides()const{
    if(!hasSceneIni(id_))return {};
    auto text=readFile(profile_/(id_+".ini")).value_or("");
    checkSceneText(text);
    return text;
}
std::string SettingsStore::sceneEditor(const fs::path& assets)const{
    if(!hasSceneIni(id_)||assets.empty())return {};
    const auto defaults=readFile(nativePath(assets/(id_+".ini"))).value_or("");
    const auto overrides=readFile(profile_/(id_+".ini")).value_or("");
    checkSceneText(defaults);
    if(overrides.empty())return defaults;
    // A hand-edited invalid document can still be opened and repaired here.
    suite::SceneIni combined;
    combined.apply(defaults);
    try{
        checkSceneText(overrides);
        suite::SceneIni edited;
        edited.apply(overrides);
        if(std::all_of(combined.values().begin(),combined.values().end(),
                [&](const auto& entry){return edited.values().count(entry.first)!=0;}))return overrides;
        combined.apply(overrides);
    }
    catch(const std::exception&){return overrides;}
    return combined.values().empty()?defaults:combined.document();
}
unsigned SettingsStore::advanceObjectsStart()const{
    privateDirectory(profile_);
    Lock lock(profile_/".objects.launch.lock");
    const auto path=profile_/".objects.launch.json";
    const auto current=readFile(path);
    const auto index=current?parse(*current).value("next",0u):0u;
    // 60 is divisible by each supported count of enabled objects (1–5).
    const auto slot=index%60;
    writeFile(path,{{"next",(slot+1)%60}});
    return slot;
}
SettingsSnapshot SettingsStore::savePatch(const SettingsJson& changed)const{
    if(!changed.is_object())throw std::runtime_error("Settings changes must be an object");
    const bool scene=changed.contains("sceneIni");
    if(scene&&!hasSceneIni(id_))throw std::runtime_error("This visualizer has no scene INI");
    validateSettingsPatch(settingsFields(id_,scene),changed);
    privateDirectory(profile_);Lock lock(profile_/("."+id_+".settings.lock"));auto current=load();
    for(auto it=changed.begin();it!=changed.end();++it)if(it.key()!="sceneIni")current.values[it.key()]=it.value();
    current.values=normalizeSettings(current.values);
    // Parse the serialized result to enforce nesting and Unicode before writing.
    parse(current.values.dump());
    if(scene)writeText(profile_/(id_+".ini"),changed.at("sceneIni").get_ref<const std::string&>());
    if(!scene||changed.size()>1){writeFile(profile_/(id_+".json"),current.values);current.importedIni=false;}
    return current;
}
}
