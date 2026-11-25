// SPDX-License-Identifier: Apache-2.0
#include "background.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
namespace fs=std::filesystem;using json=nlohmann::json;using namespace prismflow;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static json read(const fs::path& file){require(fs::file_size(file)<1024*1024,"Reference JSON exceeds its bound");std::ifstream input(file);return json::parse(input);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid or cancelled background request was accepted");}
int main(int argc,char** argv){json report;fs::path output;std::string current;
    try{
        require(argc==3,"Expected reference JSON and result JSON");output=argv[2];const fs::path file=argv[1];const auto fixtures=read(file);
        require(fixtures.is_array()&&fixtures.size()<=128,"Invalid background case list");json results=json::array();uint64_t candidatesCompared=0,choicesCompared=0;double maxError=0;
        for(const auto& fixture:fixtures){
            current=fixture.at("name");const auto foreground=fixture.at("foreground").get<Color>();const auto& options=fixture.at("options");
            BackgroundOptions p;p.adventurousness=options.value("adventurousness",p.adventurousness);p.maxLightnessDifference=options.value("max_lightness_difference",p.maxLightnessDifference);p.mixingFraction=options.value("mixing_fraction",p.mixingFraction);
            const auto native=backgroundCandidates(foreground,p);const auto reference=read(file.parent_path()/fixture.at("reference").get<std::string>());
            require(native.size()==reference.size(),"Background candidate count differs");double error=0;
            for(size_t n=0;n<native.size();++n){const auto& candidate=native[n];const std::array<double,10> values={candidate.rgb[0],candidate.rgb[1],candidate.rgb[2],candidate.score,candidate.preference,candidate.foreground,candidate.harmony,candidate.blend,candidate.lightnessDifference,candidate.probability};
                for(size_t j=0;j<values.size();++j){const double expected=reference.at(n).at(j),difference=std::abs(values[j]-expected);require(std::isfinite(values[j]),"Native background produced nonfinite values");error=std::max(error,difference);
                    if(difference>1e-8+std::abs(expected)*1e-8){report["candidate"]=n;report["component"]=j;report["native"]=values;report["reference"]=reference.at(n);throw std::runtime_error("Native background candidate ranking or score differs");}
                }
            }
            for(const auto& choice:fixture.at("choices")){unsigned calls=0;const double unit=choice.at("unit");const auto color=pickBackgroundColor(foreground,p,[&]{++calls;return unit;});
                const auto expected=choice.at("rgb").get<Color>();require(calls==choice.at("calls"),"Background random consumption differs");
                for(unsigned j=0;j<3;++j)require(std::abs(color[j]-expected[j])<=1e-8,"Selected background color differs");++choicesCompared;
            }
            candidatesCompared+=native.size();maxError=std::max(maxError,error);results.push_back({{"case",current},{"candidates",native.size()},{"maximum_absolute_error",error}});
        }
        rejects([]{backgroundCandidates({-.1,0,0});});rejects([]{backgroundCandidates({std::numeric_limits<double>::quiet_NaN(),0,0});});
        rejects([]{backgroundCandidates({.1,.2,.3},{2,18,.15});});rejects([]{backgroundCandidates({.1,.2,.3},{.5,31,.15});});
        rejects([]{pickBackgroundColor({.1,.2,.3},{},[]{return 1.;});});rejects([]{pickBackgroundColor({.1,.2,.3},{},{});});
        std::stop_source stop;stop.request_stop();rejects([&]{backgroundCandidates({.1,.2,.3},{},stop.get_token());});
        report={{"result","pass"},{"cases",results},{"candidates_compared",candidatesCompared},{"choices_compared",choicesCompared},{"maximum_absolute_error",maxError},{"invalid_and_cancelled_requests","rejected"}};
        std::ofstream(output)<<report.dump(2)<<'\n';auto summary=report;summary.erase("cases");summary["cases_compared"]=results.size();std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
