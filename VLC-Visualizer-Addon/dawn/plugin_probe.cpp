// SPDX-License-Identifier: GPL-2.0-or-later
#include "plugin.hpp"
#include "scene_dawn.hpp"
#include <fstream>
#include <iostream>
#include <numbers>

extern "C" uint64_t native_abi_c_layout(void);
using namespace visualizers::gpu;
using json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string read(const fs::path& path){std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read shader fixture");return std::string((std::istreambuf_iterator<char>(input)),{});}
void hostAlive(Context& context){
    auto shader=context.shader(R"(
        @vertex fn vertex(@builtin(vertex_index) index:u32)->@builtin(position) vec4f{
            let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[index],0,1);
        }
        @fragment fn fragment()->@location(0) vec4f{return vec4f(.5,.25,2,1);}
    )");
    auto graphics=pipeline(context,shader,"vertex","fragment");auto target=context.target(16,16);
    render(context,graphics,target.CreateView());auto pixels=context.readRgba16(target,16,16);
    require(pixels[0]==0x3800&&pixels[2]==0x4000,"Plugin destruction damaged the host device");
}
}
int main(int argc,char** argv){
    json report;fs::path output="native-plugin-check";
    try{
        if(argc!=9)throw std::runtime_error("Expected plugin, assets, output, four incompatible modules and async fixture");
        output=argv[3];fs::create_directories(output);
        static_assert(sizeof(VisNativeCreateInfo)==112&&sizeof(VisNativeFrame)==96&&sizeof(VisNativeApi)==88);
        require(native_abi_c_layout()==sizeof(VisNativeCreateInfo)*1000000ull+sizeof(VisNativeFrame)*1000ull+offsetof(VisNativeApi,render),"C and C++ ABI layouts differ");
        const std::array<const char*,4> expected={"Dawn revision","different Dawn runtime","API version/table","does not export"};
        for(unsigned n=0;n<4;++n){
            bool rejected=false;try{PluginLibrary bad(argv[4+n]);}
            catch(const std::exception& error){rejected=std::string(error.what()).find(expected[n])!=std::string::npos;report["rejections"][std::to_string(n)]=error.what();}
            require(rejected,"Incompatible native module was not rejected correctly");
        }
        const auto marker=output/"callback-completed.txt";std::error_code ignored;fs::remove(marker,ignored);
        {
            Context callbackContext;
            {
                PluginLibrary callbackLibrary(argv[8],"async-probe");bool failed=false;
                try{callbackLibrary.create(callbackContext,16,16,fs::absolute(output));}
                catch(const std::exception& error){failed=std::string(error.what()).find("Intentional deferred-callback")!=std::string::npos;}
                require(failed,"Deferred callback fixture did not fail as expected");
            }
        }
        require(fs::is_regular_file(marker),"Plugin callback was not cancelled before module unload");
        report["deferred_callback_on_failed_create"]="completed before module unload";
        Context context;report["adapter"]=context.info();
        {
            PluginLibrary library(argv[1],"terrain");const auto& api=library.api();
            const WGPUStringView identityName={"wgpuCreateInstance",WGPU_STRLEN};
            require(api.runtime_get_proc_address(identityName)==wgpuGetProcAddress(identityName),"Plugin has a second Dawn implementation");
            std::array<char,128> text{};VisNativeError error{sizeof(error),uint32_t(text.size()),text.data()};
            VisNativeCreateInfo invalid{};invalid.size=sizeof(invalid);invalid.api_version=999;
            require(!api.create(&invalid,&error)&&text[0],"Invalid create version did not return an error");
            bool settingsRejected=false;
            try{library.create(context,192,128,fs::absolute(argv[2]),"{\"colorBeats\":0}");}
            catch(const std::exception& error){settingsRejected=std::string(error.what()).find("supported ranges")!=std::string::npos;}
            require(settingsRejected,"Malformed settings were not rejected");
            auto plugin=library.create(context,192,128,fs::absolute(argv[2]));
            require(!plugin->controlCapabilities()&&!plugin->commandStatus()&&!plugin->submitCommand(VIS_NATIVE_COMMAND_IMPORT_IMAGE,"unused",1),"Base-only API 4 plugin did not retain optional-control compatibility");
            terrain::DawnScene direct(context,read(fs::path(argv[2])/"terrain.wgsl"),192,128);
            terrain::SceneIni motion;
            motion.apply(terrain::readSceneFile(fs::path(argv[2])/"terrain.ini",32768,true));
            direct.setMotion(terrain::TerrainMotion::fromIni(motion));
            visualizers::MusicFrame audio;
            const std::array<double,12> times={0,.016,.034,51,59.9,60,111,120,171,180,231,240};
            unsigned width=192,height=128;
            for(unsigned n=0;n<times.size();++n){
                for(unsigned i=0;i<audio.wave.size();++i)audio.wave[i]=float(std::sin(double(i)*.03+n*.5)*.7);
                audio.rms=.15;audio.beatEnvelope=(n%5)/4.;audio.beatCount=n<8?n:(1ull<<40)+n;
                if(n==6){width=256;height=144;direct.resize(width,height);plugin->resize(width,height);}
                if(n==9){direct.nextColor();plugin->key(32);}
                direct.update(audio,times[n]);plugin->render(audio,times[n],1./60);
                uint32_t encoding=0;auto image=plugin->image(&encoding);
                require(encoding==VIS_NATIVE_OUTPUT_SRGB,"Unexpected plugin color encoding");
                require(context.readRgba16(image,width,height)==context.readRgba16(direct.texture(),width,height),"C ABI Terrain image differs from the direct renderer");
            }
            bool badResize=false;try{plugin->resize(0,0);}catch(const std::exception&){badResize=true;}
            require(badResize,"Invalid plugin dimensions were accepted");
            struct Guard {char before='A';char data[4]{};char after='Z';} guard;
            VisNativeError shortError{sizeof(shortError),4,guard.data};
            require(api.resize(nullptr,1,1,&shortError)!=0&&guard.before=='A'&&guard.after=='Z'&&guard.data[3]==0,"Error output crossed its buffer bounds");
            plugin.reset();hostAlive(context);
            // Drop the caller's library reference before context teardown.
            // Context keeps its module through GPU callback cancellation.
        }
        hostAlive(context);
        report["result"]="pass";report["api_version"]=VIS_NATIVE_API_VERSION;report["c_cpp_layout"]="match";
        report["frames_compared"]=12;report["image_comparison"]="exact RGBA16F";report["shared_runtime"]=true;
        report["host_device_after_plugin_destroy"]="usable";report["uint64_beat_count"]="exercised above 2^40";
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
