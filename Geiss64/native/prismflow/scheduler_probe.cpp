// SPDX-License-Identifier: Apache-2.0
#include "scheduler.hpp"
#include "random.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;using json=nlohmann::json;using namespace prismflow;using Clock=std::chrono::steady_clock;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid scheduler request accepted");}
static Preset timingPreset(const json& row){Preset p;p.number=row.at("global_preset_number");p.t1=row.at("t1");p.t2=row.at("t2");p.mapsReady=true;
    for(auto& warp:p.warps){warp.mode=35;warp.map.mode=35;warp.map.width=warp.map.height=kWarpMapSize;warp.map.dynamic.resize(kDynamicMotionParamCount);}return p;}
static PresetWorkResult completed(PresetWorker& worker){const auto deadline=Clock::now()+std::chrono::seconds(60);
    while(Clock::now()<deadline){if(auto value=worker.poll())return std::move(*value);std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Native preset worker completion timed out");}
static void waitReady(PresetWorker& worker){const auto deadline=Clock::now()+std::chrono::seconds(60);
    while(Clock::now()<deadline){if(worker.phase()==PresetWorkerPhase::Ready)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Controller worker completion timed out");}
static void success(const PresetWorkResult& result){require(result.preset&&result.preset->mapsReady&&!result.cancelled&&result.error.empty(),"Native worker failed a valid job");}
int main(int argc,char** argv){json report;fs::path output;std::string current;
    try{
        require(argc==3,"Expected scheduler manifest and result paths");const fs::path file=argv[1];output=argv[2];require(fs::file_size(file)<4*1024*1024,"Scheduler fixture exceeds bound");std::ifstream input(file);const auto fixture=json::parse(input);
        current="history reference";PresetHistory history;for(const auto& row:fixture.at("rows"))history.append(timingPreset(row));
        uint64_t timings=0;
        for(const auto& item:fixture.at("cases")){const double time=item.at("time");const auto active=history.active(time);const auto& ref=item.at("active");
            const auto indices=ref.at("active_preset_indices").get<std::vector<size_t>>();const auto weights=ref.at("active_preset_weights").get<std::vector<double>>();require(active.count==indices.size(),"Active preset count differs");
            for(size_t i=0;i<active.count;++i)require(active.indices[i]==indices[i]&&active.weights[i]==weights[i],"Active preset indices/weights differ");
            require(active.mostActive==ref.at("most_active_preset_index")&&active.blend==ref.at("preset_blend_t"),"Preset blend decision differs");
            require(history.next(time,true)==item.at("next_hard")&&history.next(time,false)==item.at("next_soft")&&history.previous(time)==item.at("previous"),"Preset navigation differs");++timings;
        }
        PresetHistory pruned;for(const auto& row:fixture.at("prune_rows"))pruned.append(timingPreset(row));require(pruned.size()==PresetHistory::limit,"History length not bounded");
        for(size_t i=0;i<pruned.size();++i)require(pruned.at(i).number==fixture.at("pruned").at(i),"History pruning differs");
        current="bounded static maps";PaletteCatalog catalog;Mulberry32 seeded(81721);auto original=Preset::generate(0,-2,2,catalog,[&]{return seeded();},0);
        PresetHistory bounded;size_t peak=0;const size_t maximum=7*size_t(kMaxActiveWarps)*kWarpMapSize*kWarpMapSize*2*sizeof(float);
        for(uint64_t n=0;n<142;++n){auto p=original;p.number=n;p.t1=n*25.;p.t2=p.t1+4;bounded.append(std::move(p));
            const size_t focus=bounded.size()>4?bounded.size()-4:0;bounded.keepNear(focus);peak=std::max(peak,bounded.mapBytes());require(bounded.mapBytes()<=maximum,"History retained more than seven full static-map sets");}
        require(bounded.size()==128&&bounded.at(0).number==14,"Static-map history pruning failed");
        auto activated=bounded.activate(bounded.at(70).t2+.1);require(bounded.at(activated.mostActive).mapsReady&&bounded.mapBytes()<=maximum,"History activation did not rebuild/prune maps");
        for(size_t index:{size_t(0),size_t(127),size_t(64),size_t(126),size_t(1)}){
            const double start=bounded.at(index).t1,expected=bounded.at(index).t2+.0001;
            require(bounded.next(start)==expected&&bounded.mapBytes()<=maximum,"Rapid hard-cut navigation retained excess static maps");
            bounded.previous(expected);require(bounded.mapBytes()<=maximum,"Rapid previous navigation retained excess static maps");}
        current="worker generation";PresetWorker worker;require(worker.generate(0,-2,2,81721,catalog,0),"Idle worker rejected generation");require(!worker.generate(1,25,29,1,catalog,0),"Worker accepted an unbounded second request");
        auto built=completed(worker);success(built);for(size_t i=0;i<4;++i)require(built.preset->warps[i].map.displacement==original.warps[i].map.displacement,"Worker generation differs from foreground generation");
        Preset dynamic;dynamic.mapsReady=true;const std::array<int32_t,4> modes={35,37,38,39};
        for(size_t i=0;i<4;++i){dynamic.warps[i].mode=modes[i];dynamic.warps[i].map=buildWarpMap(modes[i],int32_t(i));dynamic.warps[i].starsFollowMotion=.5;}
        dynamic.releaseMaps();require(dynamic.mapsReady&&dynamic.mapBytes()==4*kDynamicMotionParamCount*sizeof(float)&&!worker.rebuild(dynamic),"Fully dynamic preset lost ready descriptors");
        PresetHistory dynamicHistory;for(uint64_t n=0;n<142;++n){auto p=dynamic;p.number=n;p.t1=n*25.;p.t2=p.t1+4;dynamicHistory.append(std::move(p));}
        require(dynamicHistory.size()==128&&dynamicHistory.mapBytes()==128*4*kDynamicMotionParamCount*sizeof(float),"Dynamic history storage changed");
        PresetHistory live;live.append(std::move(*built.preset));live.at(0).releaseMaps();require(worker.rebuild(live.at(0)),"Worker rejected a released map rebuild");
        live.at(0).wave.wave_flat_angle=42;live.at(0).dots.stars_opacity=.321;live.at(0).shader.noise_str=.456;live.at(0).originalPalette.name="edited while worker ran";
        auto rebuilt=completed(worker);success(rebuilt);require(live.applyRebuild(rebuilt.ticket,std::move(*rebuilt.preset)),"Matching worker rebuild rejected");
        require(live.at(0).wave.wave_flat_angle==42&&live.at(0).dots.stars_opacity==.321&&live.at(0).shader.noise_str==.456&&live.at(0).originalPalette.name=="edited while worker ran","Worker overwrote live visual settings");
        for(size_t i=0;i<4;++i)require(live.at(0).warps[i].map.displacement==original.warps[i].map.displacement&&live.at(0).warps[i].starsFollowMotion==original.warps[i].starsFollowMotion,"Worker rebuild changed saved motion");
        current="stale and missing worker results";live.at(0).releaseMaps();require(worker.rebuild(live.at(0)),"Second rebuild rejected");live.at(0).rebuildId=-1;
        auto stale=completed(worker);success(stale);require(!live.applyRebuild(stale.ticket,std::move(*stale.preset))&&!live.at(0).mapsReady,"Stale rebuild replaced a newer state");
        require(worker.rebuild(live.at(0)),"Third rebuild rejected");for(uint64_t n=1;n<130;++n)live.append(timingPreset({{"global_preset_number",n},{"t1",n*25},{"t2",n*25+4}}));
        auto missing=completed(worker);success(missing);require(!live.applyRebuild(missing.ticket,std::move(*missing.preset)),"Pruned preset accepted worker result");
        current="worker failure and reuse";require(worker.generate(900,0,4,1,catalog,42),"Worker failed to queue invalid-mode test");auto failed=completed(worker);
        require(!failed.preset&&!failed.cancelled&&!failed.error.empty()&&failed.error.size()<=1024,"Worker failure did not produce bounded error");
        require(worker.generate(901,0,4,31,catalog,0),"Worker could not resume after a failed job");auto recovered=completed(worker);success(recovered);
        uint64_t cancelChecks=0;current="worker cancellation";
        for(unsigned n=0;n<4;++n){require(worker.generate(1000+n,0,4,n,catalog,34),"Worker rejected cancellation test");const bool accepted=worker.cancel();auto cancelled=completed(worker);
            if(accepted){require(cancelled.cancelled&&!cancelled.preset&&cancelled.error.empty(),"Accepted cancellation raced into a successful result");++cancelChecks;}else success(cancelled);}
        require(cancelChecks>0,"No cancellable jobs observed");worker.close();require(worker.phase()==PresetWorkerPhase::Stopped&&!worker.generate(0,0,1,0,catalog),"Closed worker accepted work");
        current="repeated worker lifetimes";
        for(unsigned n=0;n<40;++n){PresetWorker repeated;require(repeated.generate(n,0,4,n,catalog,0),"Repeated worker rejected work");auto result=completed(repeated);success(result);}
        for(unsigned n=0;n<8;++n){PresetWorker interrupted;require(interrupted.generate(n,0,4,n,catalog,34),"Interrupted worker rejected work");interrupted.close();require(interrupted.phase()==PresetWorkerPhase::Stopped,"Worker shutdown did not join");}
        current="controller lookahead";PresetController controller;Mulberry32 controls(5017);auto random=[&]{return controls();};const auto initial=controller.tick(0,random);
        require(initial.count==1&&controller.history.size()==2,"Controller did not initialize two presets");
        for(unsigned n=0;n<4;++n){const double time=controller.history.at(controller.history.size()-1).t2+.1;controller.tick(time,random);waitReady(controller.worker);const size_t count=controller.history.size();controller.tick(time,random);
            require(controller.history.size()==count+1,"Controller failed to append lookahead preset");const auto& next=controller.history.at(controller.history.size()-1);
            require(next.t1>=time+20&&next.t1<time+30&&next.t2>=next.t1+3&&next.t2<next.t1+6,"Controller durations outside reference ranges");require(controller.history.mapBytes()<=maximum,"Controller history map budget exceeded");}
        current="invalid scheduler inputs";rejects([&]{history.active(std::numeric_limits<double>::quiet_NaN());});rejects([&]{history.keepNear(history.size());});rejects([&]{history.append(timingPreset(fixture.at("rows").at(0)));});
        PresetHistory empty;rejects([&]{empty.active(0);});std::stop_source stop;stop.request_stop();rejects([&]{controller.tick(0,random,stop.get_token());});
        report={{"result","pass"},{"reference_timing_navigation_cases",timings},{"history_limit",128},{"full_map_set_limit",7},{"observed_history_map_bytes_peak",peak},{"history_map_byte_bound",maximum},
            {"dynamic_history_descriptor_bytes",dynamicHistory.mapBytes()},{"worker_live_settings_and_stale_result_checks","pass"},{"accepted_cancellations",cancelChecks},{"normal_worker_lifetimes",40},{"interrupted_worker_lifetimes",8},{"controller_lookahead_cycles",4},{"invalid_requests","rejected"}};
        std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
