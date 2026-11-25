// SPDX-License-Identifier: Apache-2.0
#include "motion.hpp"
#include "random.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace fs=std::filesystem;using json=nlohmann::json;using namespace prismflow;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static json read(const fs::path& path){require(fs::file_size(path)<=1024*1024,"Reference metadata exceeds its bound");std::ifstream input(path);return json::parse(input);}
int main(int argc,char** argv){json report;fs::path output;std::string current;
    try{
        static_assert(sizeof(void*)==8&&std::endian::native==std::endian::little);
        require(argc==3,"Expected motion reference JSON and output JSON");output=argv[2];const fs::path fixtureFile=argv[1];const auto folder=fixtureFile.parent_path();
        const auto metadata=read(folder/"metadata.json"),fixtures=read(fixtureFile);require(fixtures.is_array()&&fixtures.size()<=256,"Invalid motion case list");
        require(json::array({kWarpMapSize,kDynamicMotionParamCount,kMaxActiveWarps,kGpuWarpSlots,kNoiseTexSize,kNoiseTexCount})==metadata.at("constants"),"Native motion constants differ");
        json modes=json::array();for(const auto& mode:motionModes())modes.push_back({{"mode",mode.mode},{"weight",mode.weight}});require(modes==metadata.at("modes"),"Motion selection weights differ");
        const std::array<ModeRelation,4> relations={ModeRelation::SuperGood,ModeRelation::Good,ModeRelation::Bad,ModeRelation::Forbidden};
        for(unsigned n=0;n<relations.size();++n)require(json(modePairs(relations[n]))==metadata.at("pairs").at(n),"Motion compatibility matrix differs");
        require(json::array({kSuperGoodModePairWeightScale,kGoodModePairWeightScale,kBadModePairWeightScale,kForbiddenModePairWeightScale})==metadata.at("weights"),"Motion pair weights differ");
        for(const auto& rng:metadata.at("random")){Mulberry32 random(uint32_t(rng.at("seed").get<int32_t>()));for(const auto& expected:rng.at("values"))require(random()==expected.get<double>(),"Seeded random stream differs");}
        require(jsInt32(4294967295.)==-1&&jsInt32(-2147483649.)==2147483647&&jsInt32(std::numeric_limits<double>::infinity())==0,"JS integer conversion differs");
        for(const auto& selection:metadata.at("selection")){const auto companions=selection.at("companions").get<std::vector<int32_t>>();require(pickRandomWarpMap(companions,selection.at("unit"))==selection.at("mode"),"Weighted motion selection differs");}
        json results=json::array();uint64_t components=0,differentBits=0;double maximumError=0,metadataError=0;unsigned dynamicCases=0;
        for(const auto& fixture:fixtures){
            const int32_t mode=fixture.at("mode"),seed=fixture.at("seed");current="mode "+std::to_string(mode)+", seed "+std::to_string(seed);
            const auto map=buildWarpMap(mode,seed);require(map.mode==mode&&map.seed==seed&&map.width==fixture.at("width")&&map.height==fixture.at("height"),"Motion map identity/dimensions differ");
            require(map.isDynamic()==fixture.at("dynamic"),"Static/dynamic mode classification differs");
            json preferences=json::object();for(const auto& field:preferenceFields)preferences[field.name]=map.preferences.*field.member;
            require(preferences==fixture.at("preferences"),"Motion preferences differ");
            const std::array<double,3> breathing={map.breathingStrength,map.breathingPhase,map.breathingFrequency};
            for(unsigned n=0;n<breathing.size();++n){const double expected=fixture.at("breathing").at(n),difference=std::abs(breathing[n]-expected);metadataError=std::max(metadataError,difference);
                if(difference>1e-13+std::abs(expected)*1e-13){report["native_breathing"]=breathing;report["reference_breathing"]=fixture.at("breathing");throw std::runtime_error("Motion breathing parameters differ");}}
            const auto& values=map.isDynamic()?map.dynamic:map.displacement;require(values.size()==fixture.at("count"),"Motion map component count differs");
            const auto reference=folder/fixture.at("reference").get<std::string>();require(fs::file_size(reference)==values.size()*sizeof(float),"Reference map byte count differs");
            std::vector<float> expected(values.size());std::ifstream input(reference,std::ios::binary);input.read(reinterpret_cast<char*>(expected.data()),std::streamsize(expected.size()*sizeof(float)));require(bool(input),"Cannot read reference map");
            double error=0;uint64_t different=0;
            for(size_t n=0;n<values.size();++n){require(std::isfinite(values[n])&&std::isfinite(expected[n]),"Nonfinite motion map component");const double difference=std::abs(double(values[n])-expected[n]);error=std::max(error,difference);
                if(std::bit_cast<uint32_t>(values[n])!=std::bit_cast<uint32_t>(expected[n]))++different;
                if(difference>2e-7+std::abs(double(expected[n]))*1e-6)throw std::runtime_error("Motion component "+std::to_string(n)+" differs: native="+std::to_string(values[n])+", reference="+std::to_string(expected[n]));
            }
            components+=values.size();differentBits+=different;maximumError=std::max(maximumError,error);dynamicCases+=map.isDynamic();
            results.push_back({{"mode",mode},{"seed",seed},{"dynamic",map.isDynamic()},{"maximum_absolute_error",error},{"different_float_bits",different}});
            if(results.size()%16==0)std::cerr<<"Compared "<<results.size()<<'/'<<fixtures.size()<<" complete motion fixtures\n";
        }
        current="cancellation/invalid-mode checks";std::stop_source stop;stop.request_stop();bool cancelled=false,invalid=false;
        try{buildWarpMap(4,0,stop.get_token());}catch(const std::exception& error){cancelled=std::string(error.what()).find("cancelled")!=std::string::npos;}
        try{buildWarpMap(42,0);}catch(const std::invalid_argument&){invalid=true;}require(cancelled&&invalid,"Motion cancellation or unsupported-mode handling failed");
        bool invalidSelection=false;try{pickRandomWarpMap({},1.0);}catch(const std::invalid_argument&){invalidSelection=true;}require(invalidSelection,"Out-of-range random choice was accepted");
        report={{"result","pass"},{"cases",results},{"components_compared",components},{"dynamic_cases",dynamicCases},{"maximum_absolute_error",maximumError},{"maximum_metadata_error",metadataError},{"different_float_bits",differentBits},{"random_values_compared",1536},{"compatibility_tables","exact"},{"weighted_selection_cases",metadata.at("selection").size()}};
        std::ofstream(output)<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");summary["cases_compared"]=results.size();std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
