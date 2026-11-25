// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#include "scheduler.hpp"
#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace prismflow {
namespace {
void validTime(double time){if(!std::isfinite(time))throw std::invalid_argument("Preset time must be finite");}
double draw(const RandomFunction& random){
    if(!random)throw std::invalid_argument("A controller random source is required");const double value=random();
    if(!std::isfinite(value)||value<0||value>=1)throw std::invalid_argument("Controller random source must return [0,1)");return value;
}
}
Preset* PresetHistory::find(uint64_t number){for(auto& preset:presets_)if(preset.number==number)return &preset;return nullptr;}
void PresetHistory::append(Preset preset){
    validTime(preset.t1);validTime(preset.t2);
    if(preset.t2<preset.t1||(!presets_.empty()&&(preset.number<=presets_.back().number||preset.t1<presets_.back().t2)))throw std::invalid_argument("Preset history must be chronological with increasing identities");
    presets_.push_back(std::move(preset));if(presets_.size()>limit){presets_.pop_front();if(focus_)--focus_;}keepNear(focus_);
}
ActivePresets PresetHistory::active(double time)const{
    validTime(time);if(presets_.empty())throw std::logic_error("Preset history is empty");ActivePresets result;
    auto single=[&](size_t index){result.indices[0]=index;result.weights[0]=1;result.count=1;result.mostActive=index;};
    if(time<presets_.front().t2){single(0);return result;}
    for(size_t i=1;i<presets_.size();++i){const auto& preset=presets_[i];if(time<preset.t1){single(i-1);return result;}
        if(time<preset.t2){double t=(time-preset.t1)/(preset.t2-preset.t1);t=t*t*(3-2*t);
            result.indices={i-1,i};result.weights={1-t,t};result.count=2;result.mostActive=t>=.5?i:i-1;result.blend=t;return result;}}
    single(presets_.size()-1);return result;
}
ActivePresets PresetHistory::activate(double time,std::stop_token stop){
    const auto result=active(time);keepNear(result.mostActive);for(size_t i=0;i<result.count;++i)at(result.indices[i]).rebuildMaps(false,stop);return result;
}
std::optional<uint64_t> PresetHistory::keepNear(size_t index){
    if(index>=presets_.size())throw std::out_of_range("Invalid preset history focus");focus_=index;
    const size_t begin=index>radius?index-radius:0,end=std::min(presets_.size(),index+radius+1);std::optional<uint64_t> needed;
    for(size_t i=0;i<presets_.size();++i){if(i<begin||i>=end)presets_[i].releaseMaps();else if(!needed&&!presets_[i].mapsReady)needed=presets_[i].number;}
    return needed;
}
double PresetHistory::next(double time,bool hardCut,std::stop_token stop){
    validTime(time);
    if(hardCut){for(size_t i=0;i<presets_.size();++i)if(time<presets_[i].t1*.1+.9*presets_[i].t2){keepNear(i);presets_[i].rebuildMaps(false,stop);return presets_[i].t2+.0001;}}
    else{for(size_t i=0;i+1<presets_.size();++i){if(time<presets_[i].t2)return time;
        if(time<presets_[i+1].t1){keepNear(i+1);presets_[i+1].rebuildMaps(false,stop);return presets_[i+1].t1+.0001;}}}return time;
}
double PresetHistory::previous(double time,std::stop_token stop){
    validTime(time);for(size_t n=presets_.size();n>0;--n){const size_t i=n-1;if(time>presets_[i].t1*.9+.1*presets_[i].t2){
        const size_t selected=i?i-1:0;keepNear(selected);auto& preset=presets_[selected];preset.rebuildMaps(false,stop);return preset.t2+.0001;}}return time;
}
bool PresetHistory::applyRebuild(int64_t ticket,Preset&& rebuilt){
    auto* current=find(rebuilt.number);if(!current||current->mapsReady||ticket<0||current->rebuildId!=ticket||rebuilt.rebuildId!=ticket||!rebuilt.mapsReady)return false;
    for(size_t i=0;i<current->warps.size();++i)if(current->warps[i].mode!=rebuilt.warps[i].mode||current->warps[i].map.seed!=rebuilt.warps[i].map.seed)return false;
    // Preserve any waveform/dot/shader/palette edits made while the job ran.
    current->warps=std::move(rebuilt.warps);current->mapsReady=true;current->rebuildId=-1;keepNear(focus_);return true;
}
size_t PresetHistory::mapBytes()const{size_t result=0;for(const auto& preset:presets_)result+=preset.mapBytes();return result;}
PresetWorker::PresetWorker():thread_([this](std::stop_token stop){run(stop);}){}
PresetWorker::~PresetWorker(){close();}
void PresetWorker::close(){
    thread_.request_stop();{std::lock_guard lock(mutex_);closed_=true;jobStop_.request_stop();}wake_.notify_all();if(thread_.joinable())thread_.join();
    std::lock_guard lock(mutex_);request_.reset();result_.reset();phase_=PresetWorkerPhase::Stopped;
}
bool PresetWorker::generate(uint64_t number,double t1,double t2,uint32_t seed,const PaletteCatalog& catalog,int32_t overrideMode){
    std::lock_guard lock(mutex_);if(closed_||phase_!=PresetWorkerPhase::Idle)return false;
    if(nextTicket_==std::numeric_limits<int64_t>::max())throw std::overflow_error("Preset worker ticket space exhausted");
    Request request{PresetWorkKind::Generate,nextTicket_,number};request.t1=t1;request.t2=t2;request.randomSeed=seed;request.overrideMode=overrideMode;request.catalog=std::make_unique<PaletteCatalog>(catalog);
    jobStop_=std::stop_source{};request_=std::move(request);++nextTicket_;phase_=PresetWorkerPhase::Queued;wake_.notify_one();return true;
}
bool PresetWorker::rebuild(Preset& source){
    std::lock_guard lock(mutex_);if(closed_||phase_!=PresetWorkerPhase::Idle||source.mapsReady)return false;
    if(nextTicket_==std::numeric_limits<int64_t>::max())throw std::overflow_error("Preset worker ticket space exhausted");
    auto snapshot=std::make_unique<Preset>();snapshot->number=source.number;snapshot->rebuildId=nextTicket_;
    for(size_t i=0;i<source.warps.size();++i){const auto& from=source.warps[i];auto& to=snapshot->warps[i];const auto& map=from.map;
        to.mode=from.mode;to.rand_seed=from.rand_seed;to.strength=from.strength;to.starsFollowMotion=from.starsFollowMotion;
        to.map={map.mode,map.width,map.height,map.preferences,{},map.dynamic,map.seed,map.breathingStrength,map.breathingPhase,map.breathingFrequency};}
    Request request{PresetWorkKind::Rebuild,nextTicket_,source.number};request.snapshot=std::move(snapshot);
    jobStop_=std::stop_source{};request_=std::move(request);source.rebuildId=nextTicket_++;phase_=PresetWorkerPhase::Queued;wake_.notify_one();return true;
}
bool PresetWorker::cancel(){std::lock_guard lock(mutex_);if(phase_!=PresetWorkerPhase::Queued&&phase_!=PresetWorkerPhase::Running)return false;return jobStop_.request_stop();}
PresetWorkerPhase PresetWorker::phase()const{std::lock_guard lock(mutex_);return phase_;}
std::optional<PresetWorkResult> PresetWorker::poll(){
    std::lock_guard lock(mutex_);if(!result_)return std::nullopt;auto result=std::move(result_);result_.reset();phase_=PresetWorkerPhase::Idle;return result;
}
void PresetWorker::run(std::stop_token lifetime){
    for(;;){
        std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return closed_||request_.has_value()||lifetime.stop_requested();});
        if(closed_||lifetime.stop_requested())break;
        auto request=std::move(*request_);request_.reset();const auto stop=jobStop_.get_token();phase_=PresetWorkerPhase::Running;lock.unlock();
        PresetWorkResult completed;completed.kind=request.kind;completed.ticket=request.ticket;completed.number=request.number;
        try{
            if(request.kind==PresetWorkKind::Generate){Mulberry32 random(request.randomSeed);completed.preset=std::make_unique<Preset>(Preset::generate(request.number,request.t1,request.t2,*request.catalog,[&]{return random();},request.overrideMode,stop));}
            else{request.snapshot->rebuildMaps(true,stop);completed.preset=std::move(request.snapshot);}
        }catch(const std::exception& error){if(!stop.stop_requested())completed.error=std::string(error.what()).substr(0,1024);}
        catch(...){if(!stop.stop_requested())completed.error="Unknown native preset worker failure";}
        lock.lock();if(closed_||lifetime.stop_requested())break;
        // Serialize accepted cancellation with publication. A cancel that
        // returns true cannot race into a successful completion afterward.
        if(stop.stop_requested()){completed.preset.reset();completed.error.clear();completed.cancelled=true;}
        result_=std::move(completed);phase_=PresetWorkerPhase::Ready;
    }
}
ActivePresets PresetController::tick(double time,const RandomFunction& random,std::stop_token stop){
    validTime(time);if(stop.stop_requested())throw std::runtime_error("Prism Flow controller cancelled");
    if(auto completed=worker.poll()){
        if(!completed->error.empty())throw std::runtime_error(completed->error);
        if(!completed->cancelled&&completed->preset){if(completed->kind==PresetWorkKind::Generate)history.append(std::move(*completed->preset));else history.applyRebuild(completed->ticket,std::move(*completed->preset));}
    }
    if(history.size()==0){
        auto first=Preset::generate(nextNumber_++,time-.002,time-.001,catalog,random,-1,stop);const double nextStart=first.t2+(20+10*draw(random)),nextEnd=nextStart+(3+3*draw(random));
        history.append(std::move(first));history.append(Preset::generate(nextNumber_++,nextStart,nextEnd,catalog,random,-1,stop));
    }
    const auto active=history.activate(time,stop);const auto& last=history.at(history.size()-1);
    if(time>=last.t2&&worker.phase()==PresetWorkerPhase::Idle){
        if(nextNumber_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Preset identity space exhausted");
        const double t1=std::max(last.t2,time)+(20+10*draw(random)),t2=t1+(3+3*draw(random));const auto seed=uint32_t(draw(random)*4294967296.);
        if(worker.generate(nextNumber_,t1,t2,seed,catalog))++nextNumber_;
    }
    if(const auto needed=history.keepNear(active.mostActive);needed&&worker.phase()==PresetWorkerPhase::Idle)worker.rebuild(*history.find(*needed));
    return active;
}
}
