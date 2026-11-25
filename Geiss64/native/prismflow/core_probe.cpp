// SPDX-License-Identifier: Apache-2.0
#include "wormhole.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

using json=nlohmann::json;namespace fs=std::filesystem;using namespace prismflow;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F&& function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,message);}
int main(int argc,char** argv){json report;fs::path output;
    try{
        static_assert(sizeof(void*)==8&&std::endian::native==std::endian::little);
        if(argc!=3)throw std::runtime_error("Expected reference fixture JSON and output JSON");output=argv[2];const fs::path fixtures=argv[1];
        if(fs::file_size(fixtures)>1024*1024)throw std::runtime_error("Reference metadata exceeds its limit");
        std::ifstream input(fixtures);const auto cases=json::parse(input);require(cases.is_array()&&cases.size()<=64,"Invalid reference case list");
        json results=json::array();uint64_t pixelsCompared=0;double overallError=0;
        for(const auto& fixture:cases){
            WormholeParameters p;const auto& options=fixture.at("options");
            p.r1=options.value("r1",p.r1);p.r2=options.value("r2",p.r2);p.rotation=options.value("rotation",p.rotation);
            p.inward_speed=options.value("inward_speed",p.inward_speed);p.fall_acceleration=options.value("fall_acceleration",p.fall_acceleration);
            p.elevation_degrees=options.value("elevation_degrees",p.elevation_degrees);p.azimuth_degrees=options.value("azimuth_degrees",p.azimuth_degrees);
            p.camera_distance=options.value("camera_distance",p.camera_distance);p.view_half_extent=options.value("view_half_extent",p.view_half_extent);
            p.perspective=options.value("perspective",p.perspective);p.center_x=options.value("center_x",p.center_x);p.center_y=options.value("center_y",p.center_y);
            p.flow_step=options.value("flow_step",p.flow_step);p.map_scale=options.value("map_scale",p.map_scale);
            const uint32_t width=fixture.at("width"),height=fixture.at("height");const uint64_t pixels=uint64_t(width)*height;
            require(pixels&&pixels<=16'777'216,"Reference dimensions exceed their budget");
            std::vector<float> native(size_t(pixels)*2),reference(native.size());const auto stats=fillWormholeMap(native,width,height,p);
            const auto referencePath=fixtures.parent_path()/fixture.at("reference").get<std::string>();require(fs::file_size(referencePath)==reference.size()*sizeof(float),"Reference map size differs");
            std::ifstream raw(referencePath,std::ios::binary);raw.read(reinterpret_cast<char*>(reference.data()),std::streamsize(reference.size()*sizeof(float)));require(bool(raw),"Cannot read reference map");
            double error=0;uint64_t differentBits=0;
            for(size_t n=0;n<native.size();++n){require(std::isfinite(native[n])&&std::isfinite(reference[n]),"Reference comparison encountered nonfinite motion");
                const double difference=std::abs(double(native[n])-reference[n]);error=std::max(error,difference);
                if(std::bit_cast<uint32_t>(native[n])!=std::bit_cast<uint32_t>(reference[n]))++differentBits;
                require(difference<=2e-7+std::abs(double(reference[n]))*1e-6,"Native wormhole map differs beyond the float comparison tolerance");
            }
            require(json(stats)==fixture.at("stats"),"Native wormhole surface classification differs");pixelsCompared+=pixels;overallError=std::max(overallError,error);
            results.push_back({{"case",fixture.at("name")},{"pixels",pixels},{"maximum_absolute_error",error},{"different_float_bits",differentBits},{"stats",stats}});
        }
        auto bad=WormholeParameters{};bad.r1=0;rejects([&]{WormholeMotion invalid(bad);},"Zero wormhole radius was accepted");
        bad=WormholeParameters{};bad.flow_step=std::numeric_limits<double>::infinity();rejects([&]{WormholeMotion invalid(bad);},"Nonfinite flow step was accepted");
        std::array<float,8> small{};rejects([&]{fillWormholeMap(small,0,2);},"Empty map was accepted");rejects([&]{fillWormholeMap(small,4,4);},"Wrong map storage size was accepted");
        WormholeMotion valid;rejects([&]{valid.sample(std::numeric_limits<double>::quiet_NaN(),0);},"Nonfinite map sample was accepted");
        bad=WormholeParameters{};bad.flow_step=100000;rejects([&]{WormholeMotion stiff(bad);stiff.sample(0,0);},"Unbounded flow integration was accepted");
        bad=WormholeParameters{};bad.map_scale=1e100;rejects([&]{fillWormholeMap(small,2,2,bad);},"Displacement beyond float32 range was accepted");
        std::stop_source cancellation;cancellation.request_stop();rejects([&]{fillWormholeMap(small,2,2,{},cancellation.get_token());},"Cancelled motion generation continued");
        report={{"result","pass"},{"pixels_compared",pixelsCompared},{"maximum_absolute_error",overallError},{"cases",results},{"invalid_and_cancelled_inputs","rejected"}};
        std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report={{"result","fail"},{"error",error.what()}};if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
