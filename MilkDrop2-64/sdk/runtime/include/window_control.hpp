// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <libproc.h>
#endif
#endif
namespace suite {
namespace controlfs=std::filesystem;
inline controlfs::path controlPathFromUtf8(const std::string& value){
#if defined(__cpp_char8_t)
    return controlfs::path(std::u8string(value.begin(),value.end()));
#else
    return controlfs::u8path(value);
#endif
}
inline controlfs::path controlDirectory(){
#ifdef _WIN32
    const wchar_t* home=_wgetenv(L"LOCALAPPDATA");if(!home)throw std::runtime_error("LOCALAPPDATA is unavailable");
    auto directory=controlfs::path(home)/L"VLCVisualizers"/L"control";
#elif defined(__APPLE__)
    const char* home=getenv("HOME");if(!home)throw std::runtime_error("HOME is unavailable");
    auto directory=controlfs::path(home)/"Library/Application Support/VLCVisualizers/control";
#else
    const char* runtime=getenv("XDG_RUNTIME_DIR");const char* home=getenv("HOME");
    auto directory=runtime&&*runtime?controlfs::path(runtime)/"vlc-visualizers/control":controlfs::path(home?home:".")/".cache/vlc-visualizers/control";
#endif
    controlfs::create_directories(directory);
#ifndef _WIN32
    chmod(directory.c_str(),0700);
#endif
    return directory;
}
inline unsigned controlPid(){
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return unsigned(getpid());
#endif
}
inline bool sameSuite(const controlfs::path& a,const controlfs::path& b){
    auto left=controlfs::weakly_canonical(a),right=controlfs::weakly_canonical(b);
#ifdef _WIN32
    return !_wcsicmp(left.c_str(),right.c_str());
#else
    return left==right;
#endif
}
struct ControlProcess {
    bool exited=false;
    std::string start;
};
inline ControlProcess controlProcess(uint32_t pid){
#ifdef _WIN32
    HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return {GetLastError()==ERROR_INVALID_PARAMETER,{}};
    const bool exited=WaitForSingleObject(process,0)==WAIT_OBJECT_0;
    FILETIME created{},exitTime{},kernel{},user{};
    const bool hasTime=!exited&&GetProcessTimes(process,&created,&exitTime,&kernel,&user);
    CloseHandle(process);
    if(hasTime)return {false,std::to_string((uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime)};
    return {exited,{}};
#else
    if(kill(static_cast<pid_t>(pid),0)<0)return {errno==ESRCH,{}};
#if defined(__APPLE__)
    proc_bsdinfo info{};
    if(proc_pidinfo(static_cast<int>(pid),PROC_PIDTBSDINFO,0,&info,sizeof(info))==sizeof(info))
        return {false,std::to_string(info.pbi_start_tvsec)+":"+std::to_string(info.pbi_start_tvusec)};
    return {};
#else
    std::ifstream stat("/proc/"+std::to_string(pid)+"/stat");
    std::string line;
    if(!std::getline(stat,line))return {};
    // The command name is parenthesized and can itself contain parentheses.
    const auto end=line.rfind(") ");
    if(end==std::string::npos)return {};
    std::istringstream fields(line.substr(end+2));
    char state{};
    fields>>state;
    if(state=='Z'||state=='X')return {true,{}};
    // Fields 4–21 precede the start time in clock ticks (field 22).
    std::string start;
    for(int field=4;field<=22;++field)if(!(fields>>start))return {};
    static const std::string boot=[]{
        std::ifstream id("/proc/sys/kernel/random/boot_id");
        std::string value;
        return std::getline(id,value)?value:std::string{};
    }();
    return {false,boot+":"+start};
#endif
#endif
}
class WindowControl {
    controlfs::path record_,request_,input_;std::string token_;
public:
    WindowControl(const controlfs::path& suite,const std::string& id,unsigned owner,bool inputSelection=false){
        std::random_device random;std::ostringstream token;for(int n=0;n<4;++n)token<<std::hex<<std::setw(8)<<std::setfill('0')<<random();token_=token.str();
        record_=controlDirectory()/(std::to_string(controlPid())+"-"+token_+".json");request_=record_;request_+=".close";
        input_=record_;input_+=".input";
        const auto encodedSuite=controlfs::weakly_canonical(suite).u8string();
        const std::string suiteUtf8(encodedSuite.begin(),encodedSuite.end());
        nlohmann::json value={{"protocol",1},{"pid",controlPid()},{"owner_pid",owner},{"id",id},{"token",token_},{"suite",suiteUtf8}};
        const auto process=controlProcess(controlPid());
        if(!process.start.empty())value["process_start"]=process.start;
        if(inputSelection)value["input_selection"]=true;
        {std::ofstream out(record_,std::ios::binary|std::ios::trunc);out<<value.dump();if(!out)throw std::runtime_error("Cannot register visualizer window");}
#ifndef _WIN32
        chmod(record_.c_str(),0600);
#endif
    }
    WindowControl(const WindowControl&)=delete;
    ~WindowControl(){std::error_code ignored;controlfs::remove(input_,ignored);controlfs::remove(request_,ignored);controlfs::remove(record_,ignored);}
    bool closeRequested()const{
        std::error_code error;auto size=controlfs::file_size(request_,error);if(error||size>128)return false;
        std::ifstream stream(request_);std::string token;stream>>token;return token==token_;
    }
    bool inputRequested(){
        std::error_code error;auto size=controlfs::file_size(input_,error);if(error||size>128)return false;
        std::ifstream stream(input_);std::string token;stream>>token;stream.close();controlfs::remove(input_,error);return token==token_;
    }
};
inline unsigned requestWindows(const controlfs::path& suite,const std::string& id,unsigned owner,bool inputSelection){
    unsigned count=0,seen=0;for(const auto& file:controlfs::directory_iterator(controlDirectory())){
        if(++seen>2048)break;
        try{if(file.path().extension()!=".json"||!controlfs::is_regular_file(file.symlink_status())||file.file_size()>4096)continue;
            std::ifstream stream(file.path());auto value=nlohmann::json::parse(stream);
            stream.close();
            if(value.at("protocol")!=1||!sameSuite(controlPathFromUtf8(value.at("suite").get<std::string>()),suite))continue;
            const auto& idValue=value.at("pid");
            if(!idValue.is_number_unsigned())continue;
            const auto widePid=idValue.get<uint64_t>();
            if(!widePid||widePid>(std::numeric_limits<int32_t>::max)())continue;
            const auto pid=static_cast<uint32_t>(widePid);
            auto token=value.at("token").get<std::string>();if(token.size()!=32||token.find_first_not_of("0123456789abcdef")!=std::string::npos)continue;
            if(file.path().filename()!=std::to_string(pid)+"-"+token+".json")continue;
            const auto process=controlProcess(pid);
            const auto start=value.value("process_start",std::string{});
            if(process.exited||(!start.empty()&&!process.start.empty()&&start!=process.start)){
                std::error_code ignored;
                controlfs::remove(file.path(),ignored);
                for(const auto* suffix:{".close",".input"}){
                    auto request=file.path();request+=suffix;
                    controlfs::remove(request,ignored);
                }
                continue;
            }
            if(value.at("owner_pid")!=owner||(id!="*"&&value.at("id")!=id))continue;
            if(inputSelection&&!value.value("input_selection",false))continue;
#ifdef _WIN32
            if(inputSelection){
                // The newly launched selector inherits foreground eligibility
                // from the focused host. Pass it to the existing helper before
                // requesting its dialog; do not grant it to every process.
                AllowSetForegroundWindow(pid);
            }
#endif
            auto request=file.path();request+=inputSelection?".input":".close";
            std::ofstream out(request,std::ios::trunc);out<<token;out.close();
            std::error_code error;
            if(!controlfs::exists(file.path(),error)){
                if(!error)controlfs::remove(request,error);
            }else if(out)++count;
        }catch(const std::exception&){ }
    }return count;
}
inline unsigned closeWindows(const controlfs::path& suite,const std::string& id,unsigned owner){return requestWindows(suite,id,owner,false);}
inline unsigned selectCaptureInput(const controlfs::path& suite,const std::string& id,unsigned owner){return requestWindows(suite,id,owner,true);}
}
