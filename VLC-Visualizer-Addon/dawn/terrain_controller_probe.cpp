// SPDX-License-Identifier: GPL-2.0-or-later
#include "scene_dawn.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    try{
        if(argc!=4)throw std::runtime_error("Expected fixture, shader and result paths");
        std::ifstream input(argv[1]);nlohmann::json fixtures;input>>fixtures;
        if(!fixtures.is_array()||fixtures.size()>128)throw std::runtime_error("Invalid Terrain fixtures");
        std::ifstream shaderFile(argv[2]);std::string shader((std::istreambuf_iterator<char>(shaderFile)),{});
        visualizers::gpu::Context context;terrain::DawnScene scene(context,shader,192,128);
        nlohmann::json results=nlohmann::json::array();
        for(const auto& fixture:fixtures){
            visualizers::MusicFrame audio;audio.wave=fixture.at("wave").get<std::array<float,2048>>();
            audio.rms=fixture.at("rms");audio.beatEnvelope=fixture.at("beatEnvelope");audio.beatCount=fixture.at("beatCount");
            if(fixture.value("nextColor",false))scene.nextColor();
            const auto mode=scene.update(audio,fixture.at("time"),fixture.at("cameraSeconds"),fixture.at("colorBeats"));
            // Bound this diagnostic's queue and surface asynchronous GPU errors.
            context.readRgba16(scene.texture(),192,128);
            results.push_back({{"uniforms",scene.uniforms()},{"history",scene.historyRow()},
                               {"next_row",scene.nextHistoryRow()},{"mode",mode}});
        }
        std::ofstream output(argv[3]);output<<results.dump();if(!output)throw std::runtime_error("Cannot write Terrain report");
        std::cout<<"Terrain controller fixtures: "<<results.size()<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
