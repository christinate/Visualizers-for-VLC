// SPDX-License-Identifier: GPL-2.0-or-later
#include "music_analysis.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("Expected fixture and report paths");
        if constexpr(std::endian::native!=std::endian::little)throw std::runtime_error("Fixture requires a little-endian test machine");
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2]);
        if(!input||!output)throw std::runtime_error("Cannot open test files");
        char magic[8];input.read(magic,8);if(std::string(magic,8)!="VISDSP01")throw std::runtime_error("Invalid analysis fixture");
        uint32_t frames=0;input.read(reinterpret_cast<char*>(&frames),sizeof(frames));
        if(!frames||frames>4096)throw std::runtime_error("Invalid fixture count");
        visualizers::MusicAnalysis analyzer;std::array<float,4096> stereo{};
        for(unsigned n=0;n<frames;++n){
            double time=0;uint32_t flags=0;input.read(reinterpret_cast<char*>(&time),sizeof(time));input.read(reinterpret_cast<char*>(&flags),sizeof(flags));
            input.read(reinterpret_cast<char*>(stereo.data()),sizeof(stereo));if(!input)throw std::runtime_error("Truncated analysis fixture");
            if(flags&1)analyzer=visualizers::MusicAnalysis{};
            const auto& frame=analyzer.update(stereo,time);
            nlohmann::json report={{"rms",frame.rms},{"visual_energy",frame.visualEnergy},{"bass",frame.bass},{"beat",frame.beat?1:0},
                {"beatEnvelope",frame.beatEnvelope},{"beatCount",frame.beatCount}};
            if(flags&2){report["wave"]=frame.wave;report["spectrum"]=frame.spectrum;}
            output<<report.dump()<<'\n';
        }
        if(!output)throw std::runtime_error("Cannot write analysis report");
        std::cout<<"{\"result\":\"pass\",\"frames\":"<<frames<<"}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
