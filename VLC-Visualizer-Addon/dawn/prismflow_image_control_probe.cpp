// SPDX-License-Identifier: GPL-2.0-or-later
#include "plugin.hpp"
#include "scene_dawn.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QGuiApplication>
#endif
using namespace visualizers::gpu;using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F&& function){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid image command was accepted");}
std::string encoded(const fs::path& file){const auto value=file.u8string();return {value.begin(),value.end()};}
}
int main(int argc,char** argv){
#if !defined(_WIN32) && !defined(__APPLE__)
 QGuiApplication app(argc,argv);
#endif
 json report;fs::path output;
 try{
    static_assert(sizeof(VisNativeCommand)==32&&sizeof(VisNativeCommandStatus)==24&&sizeof(VisNativeControlApi)==32&&sizeof(VisNativeApi)==88);
    if(argc!=5&&argc!=8&&argc!=9)throw std::runtime_error("Expected module, shaders, image fixtures, output, optional three incompatible control modules and a future-capability fixture");output=argv[4];fs::create_directories(output);const auto assets=fs::absolute(argv[2]),files=fs::absolute(argv[3]);
    for(int n=5;n<std::min(argc,8);++n){bool rejected=false;try{PluginLibrary bad(argv[n]);}catch(const std::exception& error){rejected=std::string(error.what()).find("optional control table")!=std::string::npos;}require(rejected,"Incompatible optional control table was not rejected");}
    report["incompatible_control_tables_rejected"]=argc>5?3:0;
    Context context;report["adapter"]=context.info();PluginLibrary library(fs::absolute(argv[1]),"geiss");auto plugin=library.create(context,128,80,assets,"{\"seed\":9017}");visualizers::MusicFrame audio;
    if(argc==9){PluginLibrary future(argv[8]);auto value=future.create(context,16,16,assets);require(value->controlCapabilities()==2&&!value->commandStatus()&&!value->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,"unused",1),"Unknown future control capability was polled as image import");report["unknown_capability_ignored"]=true;}
    require(plugin->controlCapabilities()==VIS_NATIVE_CONTROL_IMAGE_IMPORT,"Image control capability is missing");require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_IDLE,"Image command state did not start idle");
    rejects([&]{plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,"relative.png",1);});rejects([&]{plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,"",1);});rejects([&]{plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,std::string("/a\0b",4),1);});rejects([&]{plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,std::string("/\xc0\xaf",3),1);});rejects([&]{plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"opaque.png"),0);});
    require(!plugin->submitCommand(999,"",1),"Unknown command was not rejected as unsupported");
    unsigned frame=0;auto step=[&]{audio.rms=.12;audio.visualEnergy=.03;for(size_t n=0;n<audio.wave.size();++n)audio.wave[n]=float(.4*std::sin(n*.03+frame*.1));plugin->render(audio,frame/60.,1./60);++frame;};
    auto wait=[&](uint64_t token,uint32_t wanted){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);for(;;){step();const auto status=*plugin->commandStatus();require(status.token==token,"A superseded image command changed status");if(status.state==wanted)return status;if(status.state!=VIS_NATIVE_COMMAND_PENDING)throw std::runtime_error("Unexpected image status: "+status.message);if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Image command did not complete");std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
    step();std::string payload=encoded(files/"opaque.png");require(plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,payload,1),"Import was not accepted");std::fill(payload.begin(),payload.end(),'X');require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_PENDING,"Import must await render-thread publication");wait(1,VIS_NATIVE_COMMAND_APPLIED);
    require(plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"missing-image.png"),2),"Missing-file request was not accepted for asynchronous validation");require(!wait(2,VIS_NATIVE_COMMAND_FAILED).message.empty(),"Recoverable missing-file status omitted its error");
    plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"corrupt.png"),3);require(!wait(3,VIS_NATIVE_COMMAND_FAILED).message.empty(),"Corrupt-file status omitted its error");
    plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"transparent.png"),600);require(!wait(600,VIS_NATIVE_COMMAND_FAILED).message.empty(),"Fully transparent image did not report a recoverable no-color error");
    plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"opaque.png"),4);plugin->submitCommand(VIS_NATIVE_COMMAND_CANCEL_IMAGE,{},999);require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_PENDING,"Stale cancel cancelled a newer request");plugin->submitCommand(VIS_NATIVE_COMMAND_CANCEL_IMAGE,{},4);require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_CANCELLED,"Matching cancel did not cancel");step();require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_CANCELLED,"Cancelled worker result was later published");
    plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"opaque.png"),5);plugin->key('p');require(plugin->commandStatus()->state==VIS_NATIVE_COMMAND_CANCELLED,"Palette key did not supersede pending image work");step();
    for(uint64_t token=6;token<=205;++token)plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/(token==205?u8"image-日本語-🎵.png":u8"opaque.png")),token);wait(205,VIS_NATIVE_COMMAND_APPLIED);
    const auto pixels=context.readRgba16(plugin->image(),128,80);for(auto pixel:pixels)require(std::isfinite(decodeHalf(pixel)),"Image import damaged GPU output");plugin.reset();
    for(unsigned cycle=0;cycle<8;++cycle){auto interrupted=library.create(context,64,64,assets,"{\"seed\":9017}");interrupted->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,encoded(files/"opaque.png"),1);interrupted.reset();context.checkErrors();}
    report["result"]="pass";report["render_frames"]=frame;report["rapid_replacements"]=200;report["pending_import_module_destructions"]=8;report["borrowed_utf8_copy_unicode_errors_cancellation"]="pass";report["base_abi_size"]=sizeof(VisNativeApi);
    std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
 }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();if(!output.empty())std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
