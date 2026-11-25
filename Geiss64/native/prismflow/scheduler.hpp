// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "preset.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
namespace prismflow {
struct ActivePresets {
    std::array<size_t,2> indices{};std::array<double,2> weights{};
    size_t count=0,mostActive=0;double blend=0;
};
class PresetHistory {
    std::deque<Preset> presets_;size_t focus_=0;
public:
    static constexpr size_t limit=128,radius=3;
    size_t size()const{return presets_.size();}
    Preset& at(size_t index){return presets_.at(index);}
    const Preset& at(size_t index)const{return presets_.at(index);}
    Preset* find(uint64_t number);
    void append(Preset);
    ActivePresets active(double time)const;
    ActivePresets activate(double time,std::stop_token stop={});
    std::optional<uint64_t> keepNear(size_t index);
    double next(double time,bool hardCut=true,std::stop_token stop={});
    double previous(double time,std::stop_token stop={});
    bool applyRebuild(int64_t ticket,Preset&& rebuilt);
    size_t mapBytes()const;
};
enum class PresetWorkKind {Generate,Rebuild};
enum class PresetWorkerPhase {Idle,Queued,Running,Ready,Stopped};
struct PresetWorkResult {
    PresetWorkKind kind=PresetWorkKind::Generate;int64_t ticket=0;uint64_t number=0;
    std::unique_ptr<Preset> preset;std::string error;bool cancelled=false;
};
// One outstanding request: queued/running OR completed awaiting poll. There
// is no growing request/result queue and no worker access to live UI presets.
class PresetWorker {
    struct Request {
        PresetWorkKind kind;int64_t ticket;uint64_t number;double t1=0,t2=0;uint32_t randomSeed=0;int32_t overrideMode=-1;
        std::unique_ptr<PaletteCatalog> catalog;std::unique_ptr<Preset> snapshot;
    };
    mutable std::mutex mutex_;std::condition_variable wake_;
    std::optional<Request> request_;std::optional<PresetWorkResult> result_;
    std::stop_source jobStop_;PresetWorkerPhase phase_=PresetWorkerPhase::Idle;
    int64_t nextTicket_=1;bool closed_=false;
    // Declared last; the destructor explicitly joins before other state dies.
    std::jthread thread_;
    void run(std::stop_token);
public:
    PresetWorker();~PresetWorker();
    PresetWorker(const PresetWorker&)=delete;PresetWorker& operator=(const PresetWorker&)=delete;
    bool generate(uint64_t number,double t1,double t2,uint32_t randomSeed,const PaletteCatalog&,int32_t overrideMode=-1);
    bool rebuild(Preset&);
    bool cancel();void close();
    PresetWorkerPhase phase()const;
    std::optional<PresetWorkResult> poll();
};
// CPU orchestration only. Rendering, effect blending and GPU slots remain in
// the native renderer. The controller is owned and called by one host thread.
class PresetController {
    uint64_t nextNumber_=0;
public:
    PaletteCatalog catalog;PresetHistory history;PresetWorker worker;
    ActivePresets tick(double time,const RandomFunction&,std::stop_token stop={});
};
}
