// SPDX-License-Identifier: GPL-2.0-or-later
#include "capture_service.hpp"
#include "audio_capture.hpp"
#include "player_audio.hpp"
#include "input_picker.hpp"
#include "settings.hpp"
#include "diagnostic_text.hpp"
#include "shared_audio.hpp"
#include "window_control.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#ifdef __APPLE__
#include <libproc.h>
#endif

namespace visualizers {
namespace {
namespace fs=std::filesystem;
using json=nlohmann::json;
class CaptureLease {
    fs::path file_;
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
public:
    explicit CaptureLease(uint32_t owner){
        file_=suite::controlDirectory()/("capture-"+std::to_string(owner)+".lock");
#ifdef _WIN32
        handle_=CreateFileW(file_.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle_==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_SHARING_VIOLATION)throw std::runtime_error("Cannot lock native capture source");
#else
        handle_=open(file_.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
        if(handle_<0)throw std::runtime_error("Cannot open native capture lock");
        if(flock(handle_,LOCK_EX|LOCK_NB)){const auto code=errno;close(handle_);handle_=-1;if(code!=EWOULDBLOCK&&code!=EAGAIN)throw std::runtime_error("Cannot lock native capture source");}
        else{
            // A contender may have opened an inode just before its previous
            // owner removed it. Never claim an unlinked/replaced lock inode.
            struct stat held{},current{};
            if(fstat(handle_,&held)||stat(file_.c_str(),&current)||held.st_nlink!=1||held.st_dev!=current.st_dev||held.st_ino!=current.st_ino){close(handle_);handle_=-1;}
        }
#endif
    }
    CaptureLease(const CaptureLease&)=delete;
    bool owned()const noexcept{
#ifdef _WIN32
        return handle_!=INVALID_HANDLE_VALUE;
#else
        return handle_>=0;
#endif
    }
    ~CaptureLease(){
        if(!owned())return;
#ifdef _WIN32
        CloseHandle(handle_);DeleteFileW(file_.c_str());
#else
        unlink(file_.c_str());close(handle_);
#endif
    }
};
fs::path processExecutable(uint32_t pid){
#ifdef _WIN32
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)throw std::runtime_error("Cannot identify capture owner");
    std::wstring path(32768,L'\0');DWORD size=DWORD(path.size());const bool ok=QueryFullProcessImageNameW(process,0,path.data(),&size);CloseHandle(process);
    if(!ok)throw std::runtime_error("Cannot identify capture owner");path.resize(size);return path;
#elif defined(__APPLE__)
    std::array<char,PROC_PIDPATHINFO_MAXSIZE> path{};const auto size=proc_pidpath(int(pid),path.data(),uint32_t(path.size()));
    if(size<=0)throw std::runtime_error("Cannot identify capture owner");return fs::u8path(path.data());
#else
    return fs::read_symlink(fs::path("/proc")/std::to_string(pid)/"exe");
#endif
}
struct CaptureRing {
    uint32_t owner;bool clearPending=true;
#ifndef _WIN32
    fs::path ipcFile;
#endif
    std::unique_ptr<suite::SharedAudio> mapping;
    explicit CaptureRing(uint32_t pid):owner(pid),
#ifndef _WIN32
        ipcFile(suite::audioDirectory()/(std::to_string(getuid())+"-"+std::to_string(pid)+".pcm")),
#endif
        mapping(std::make_unique<suite::SharedAudio>(pid,true)){
        if(!mapping->ring){cleanup();throw std::runtime_error("Cannot create recording PCM transport");}
    }
    void cleanup()noexcept{
        mapping.reset();
#ifndef _WIN32
        // Finish owned IPC cleanup before releasing CaptureLease. Leaving this
        // to the global lifetime destructor could unlink a replacement helper's
        // transport after its predecessor released the source lock.
        if(suite::ipcLifetime().files.erase(ipcFile))unlink(ipcFile.c_str());
#endif
    }
    ~CaptureRing(){cleanup();}
    void reset(){clearPending=true;if(mapping->lock()){*mapping->ring={};clearPending=false;mapping->unlock();}}
    bool write(std::span<const float> samples){
        if(!mapping->lock())return false;
        if(clearPending){*mapping->ring={};clearPending=false;}
        suite::write(*mapping->ring,samples.data(),samples.size()/2,2,44100,playerMilliseconds());mapping->unlock();return true;
    }
};
void report(const fs::path& directory,const char* name,const json& value){
    fs::create_directories(directory);const auto file=directory/name;auto temporary=file;temporary+=".tmp."+std::to_string(suite::controlPid());
    try{
        {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<value.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write native capture report");}
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish native capture report");
#else
        fs::rename(temporary,file);
#endif
    }catch(...){std::error_code ignored;fs::remove(temporary,ignored);throw;}
}
}
int runCaptureService(const CaptureServiceOptions& options){
    if(!options.owner||options.owner>uint32_t(std::numeric_limits<int32_t>::max()))throw std::runtime_error("Invalid capture owner process ID");
    if(((options.generated||options.automatePicker||options.cancelPicker)&&!options.test)||(options.cancelPicker&&!options.automatePicker))throw std::runtime_error("Capture automation requires --self-test and the native picker fixture");
    if(!std::isfinite(options.seconds)||options.seconds<0||options.seconds>86400)throw std::runtime_error("Invalid capture duration");
    SettingsStore preferences(options.profile,options.id);const auto saved=preferences.load().values;
    PlayerLifetime owner(options.owner);if(owner.exited())throw std::runtime_error("Capture owner has already exited");
    if(!options.generated){
#ifdef _WIN32
        const auto expected=options.suite/L"Visualizers.exe";
#else
        const auto expected=options.suite/"Visualizers";
#endif
        if(!suite::sameSuite(processExecutable(options.owner),expected))throw std::runtime_error("Recording capture is only available to this suite's standalone host");
    }
    CaptureLease lease(options.owner);const auto controlId="capture_"+options.id;
    if(!lease.owned()){
        // Another instance already owns capture. Its native dialog gets the
        // request; this process never opens a device or shared-audio writer.
        for(unsigned attempt=0;attempt<50&&!owner.exited();++attempt){
            if(suite::selectCaptureInput(options.suite,controlId,options.owner))return 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Native input selector is still starting; try Ctrl-L again");
    }
    const auto output=options.output.empty()?options.profile/("capture-"+options.id):options.output;
    json observed={{"result","running"},{"helper_pid",suite::controlPid()},{"owner_pid",options.owner},{"visualizer",options.id},
                   {"generated_pcm",options.generated},{"native_capture_service",true},{"audio_saved",false}};
    try{
        NativeUiSession ui;suite::WindowControl control(options.suite,controlId,options.owner,true);
        CaptureRing transport(options.owner);transport.reset();
        std::unique_ptr<AudioCapture> capture,pending;std::string activeInput,pendingInput;
        auto generated=std::make_unique<AudioFifo>();std::vector<float> pcm(16384*2);std::array<float,4096> synthesis{};
        uint64_t generatedFrames=0,generatedAt=audioMilliseconds(),published=0,transportDrops=0,transportRetries=0,streamFrames=0,streamDrops=0,nextReport=0;
        unsigned pendingFrames=0;
        unsigned selections=0,activations=0,maximumDrain=0;bool running=true,ready=false,inPicker=false;std::string reason="owner-exit";
        const auto started=audioMilliseconds();
        auto startedInput=[&](std::string identifier){
            if(capture){const auto previous=capture->status();streamFrames+=previous.frames;streamDrops+=previous.dropped;}
            if(options.generated)streamDrops+=generated->dropped();
            transportDrops+=pendingFrames;pendingFrames=0;
            activeInput=identifier;pendingInput.clear();++activations;transport.reset();
            try{preferences.savePatch({{"inputDevice",identifier}});}catch(const std::exception& e){observed["settings_warning"]=boundedDiagnostic(e.what());}
        };
        auto prepare=[&](const std::string& token){
            if(options.generated){
                const std::vector<InputDevice> fixtures={{"fixture:a","Generated fixture",true}};const auto id=resolveAudioInput(fixtures,token);
                startedInput(id);generated->reset();generatedAt=audioMilliseconds();return;
            }
            auto replacement=std::make_unique<AudioCapture>();const auto id=replacement->resolveInput(token);requestCapturePermission();
            const auto permission=capturePermission();
            if(permission==CapturePermission::Granted){replacement->start(id);startedInput(id);capture=std::move(replacement);pending.reset();}
            else if(permission==CapturePermission::Pending){pending=std::move(replacement);pendingInput=id;std::cerr<<capturePermissionMessage(permission)<<'\n';}
            else throw std::runtime_error(capturePermissionMessage(permission));
        };
        auto snapshot=[&]{
            auto frames=streamFrames,drops=streamDrops;CaptureStatus status;
            if(capture){status=capture->status();frames+=status.frames;drops+=status.dropped;}
            if(options.generated){frames=generatedFrames;drops=streamDrops+generated->dropped();}
            observed["capture_frames"]=frames;observed["fifo_dropped_frames"]=drops;observed["published_frames"]=published;observed["transport_dropped_frames"]=transportDrops;
            observed["input_picker_opens"]=selections;observed["capture_activations"]=activations;observed["active_input"]=activeInput;
            observed["transport_retries"]=transportRetries;observed["pending_transport_frames"]=pendingFrames;
            observed["maximum_drain_frames"]=maximumDrain;
            observed["permission_pending"]=bool(pending);observed["elapsed_ms"]=audioMilliseconds()-started;
            if(capture){observed["backend"]=status.backend;observed["device"]=status.device;}
        };
        auto pump=[&](){
            if(owner.exited()){reason="owner-exit";running=false;return false;}
            if(control.closeRequested()){reason="menu";running=false;return false;}
            if(inPicker)control.inputRequested(); // Coalesce requests while this dialog is already open.
            const auto now=audioMilliseconds();if(options.seconds&&now-started>=options.seconds*1000){reason="duration";running=false;return false;}
            if(pending){const auto permission=capturePermission();
                if(permission==CapturePermission::Granted){pending->start(pendingInput);startedInput(pendingInput);capture=std::move(pending);}
                else if(permission!=CapturePermission::Pending)throw std::runtime_error(capturePermissionMessage(permission));
            }
            if(options.generated&&activations){
                auto frames=(now-generatedAt)*44100/1000;frames=std::min<uint64_t>(frames,16384);generatedAt=now;
                while(frames){const auto count=unsigned(std::min<uint64_t>(frames,2048));
                    for(unsigned n=0;n<count;++n){const float value=float(.55*std::sin((generatedFrames+n)*.025));synthesis[n*2]=value;synthesis[n*2+1]=-value;}
                    generated->push(synthesis.data(),count,now);generatedFrames+=count;frames-=count;
                }
            }
            if(pendingFrames&&transport.write(std::span(pcm).first(pendingFrames*2))){published+=pendingFrames;pendingFrames=0;}
            if(!pendingFrames){const auto count=options.generated?generated->drain(pcm,now):capture?capture->drain(pcm):0;
                maximumDrain=std::max(maximumDrain,count);
                if(count){if(transport.write(std::span(pcm).first(count*2)))published+=count;else{pendingFrames=count;++transportRetries;}}
            }
            if(capture&&!capture->status().running)throw std::runtime_error("Recording device stopped or disconnected");
            if(now>=nextReport){snapshot();report(output,"status.json",observed);nextReport=now+(options.test?250:10000);}
            if(published>=1024&&!ready){snapshot();report(output,"ready.json",observed);ready=true;}
            return running;
        };
        auto choose=[&](){
            inPicker=true;std::string notice;
            for(;;){
                std::vector<InputDevice> devices;
                if(options.generated)devices={{"fixture:a","Generated fixture",true}};
                else{AudioCapture listing;devices=listing.devices();}
                PickerAutomation automation;automation.row=0;automation.cancel=options.cancelPicker;++selections;
                const auto current=pending?pendingInput:activeInput.empty()?saved.at("inputDevice").get<std::string>():activeInput;
                const auto selected=pickAudioInput(devices,current,pump,options.automatePicker?&automation:nullptr,notice);
                if(!selected)break;
                try{prepare(*selected);break;}catch(const std::exception& e){if(options.automatePicker)throw;notice=boundedDiagnostic(e.what());}
            }
            inPicker=false;
        };
        if(options.lineInput.empty())choose();else prepare(options.lineInput);
        if(!capture&&!pending&&!activations){running=false;reason="input-cancelled";}
        while(running&&pump()){
            pumpNativeUi();
            if(!inPicker&&control.inputRequested())choose();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        snapshot();capture.reset();pending.reset();observed["result"]="pass";observed["closed_by"]=reason;
        // Capture, owned transport and control records release before the lease.
    }catch(const std::exception& error){observed["result"]="fail";observed["error"]=boundedDiagnostic(error.what());report(output,"result.json",observed);throw;}
    report(output,"result.json",observed);return 0;
}
}
