// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_import.hpp"
#include "random.hpp"
#include "resource_metrics.hpp"
#include "diagnostic_text.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#if !defined(_WIN32) && !defined(__APPLE__)
#include <QCoreApplication>
#endif
using namespace prismflow;using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>void until(F&& condition){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!condition()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Image worker test timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
template<class F>void rejects(F&& operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid image worker request was accepted");}
ImageImportResult result(ImagePaletteWorker& worker){std::optional<ImageImportResult> value;until([&]{value=worker.take();return bool(value);});return std::move(*value);}
visualizers::DecodedImage image(bool blue){visualizers::DecodedImage result;result.image.width=result.image.height=2;result.image.rgba={uint8_t(blue?0:255),0,uint8_t(blue?255:0),255,0,255,0,255,255,255,255,255,0,0,0,255};return result;}
PaletteDefinition expected(const visualizers::DecodedImage& value,uint32_t seed){Mulberry32 stream(seed);const RandomFunction random=[&]{return stream();};auto colors=paletteFromImageRgba(value.image.width,value.image.height,value.image.rgba,random);auto rgba=imagePaletteRgba(colors);return {rgba,"Image palette",companionColor(rgba,random),-1};}
void check(const ImageImportResult& actual,const PaletteDefinition& wanted,uint64_t token){require(actual.token==token&&actual.palette.has_value()&&actual.error.empty(),"Image worker did not publish the expected request");require(actual.palette->rgba==wanted.rgba&&actual.palette->background==wanted.background,"Image worker changed the isolated seeded palette result");}
}
int main(int argc,char** argv){
#if !defined(_WIN32) && !defined(__APPLE__)
 QCoreApplication app(argc,argv);
#endif
 json report;fs::path output;
 try{
    if(argc!=3)throw std::runtime_error("Expected real image fixtures directory and report path");const auto files=fs::absolute(argv[1]);output=argv[2];
    struct ShortError {char before='A';char bytes[4]{};char after='Z';} shortError;
    visualizers::copyDiagnostic(shortError.bytes,4,"\xf0\x9f\x8e\xb5");require(shortError.bytes[0]==0&&shortError.before=='A'&&shortError.after=='Z',"Bounded error copy split a scalar or crossed its caller buffer");
    std::mutex mutex;std::condition_variable wake;bool entered=false,released=false;std::atomic<unsigned> calls{0};
    ImagePaletteWorker worker([&](const fs::path& path,uint32_t,std::stop_token){++calls;if(path.filename()=="hold"){std::unique_lock lock(mutex);entered=true;wake.notify_one();wake.wait(lock,[&]{return released;});}if(path.filename()=="error")throw std::runtime_error(std::string(2000,'E'));if(path.filename()=="utf8")throw std::runtime_error(std::string(1023,'E')+"\xf0\x9f\x8e\xb5-tail");if(path.filename()=="invalid-utf8")throw std::runtime_error("broken-\xff");return image(path.filename()=="blue");});
    struct ReleaseOnExit {std::mutex& mutex;std::condition_variable& wake;bool& released;~ReleaseOnExit(){{std::lock_guard lock(mutex);released=true;}wake.notify_all();}} release{mutex,wake,released};
    worker.request(files/"hold",1,1);until([&]{std::lock_guard lock(mutex);return entered;});
    for(uint64_t token=2;token<=200;++token){worker.request(files/"blue",token,123);const auto state=worker.snapshot();require(state.running&&state.pending&&!state.ready,"Rapid replacements exceeded one running/pending/result slot");}
    {std::lock_guard lock(mutex);released=true;}wake.notify_one();const auto latest=result(worker);check(latest,expected(image(true),123),200);const auto replacementCalls=calls.load();require(replacementCalls==2,"Superseded pending image requests were decoded");
    worker.request(files/"blue",201,321);until([&]{return worker.snapshot().ready;});worker.request(files/"blue",202,123);check(result(worker),expected(image(true),123),202);
    worker.request(files/"blue",203,123);until([&]{return worker.snapshot().ready;});worker.cancel();require(!worker.take(),"Cancellation retained an already completed stale result");
    worker.request(files/"error",204,1);const auto failed=result(worker);require(!failed.palette&&failed.error.size()==1024&&failed.token==204,"Image error was not isolated and bounded");
    worker.request(files/"utf8",208,1);const auto unicode=result(worker);require(!unicode.palette&&unicode.error.size()==1023&&json(unicode.error).dump().size()>1023,"Long Unicode error was truncated inside a UTF-8 scalar");
    worker.request(files/"invalid-utf8",209,1);const auto invalid=result(worker);require(!invalid.palette&&invalid.error=="broken-?","Invalid codec error bytes were not made safe for UTF-8 status");
    worker.request(files/"blue",205,123);check(result(worker),expected(image(true),123),205);rejects([&]{worker.request("relative",206,1);});rejects([&]{worker.request(files/"blue",0,1);});worker.close();require(worker.snapshot().closed&&!worker.take(),"Closed worker retained a result");rejects([&]{worker.request(files/"blue",207,1);});
    for(unsigned cycle=0;cycle<40;++cycle){ImagePaletteWorker repeated([](const fs::path&,uint32_t,std::stop_token){return image(true);});repeated.request(files/"blue",cycle+1,123);check(result(repeated),expected(image(true),123),cycle+1);}
    for(unsigned cycle=0;cycle<8;++cycle){std::atomic<bool> started=false;ImagePaletteWorker interrupted([&](const fs::path&,uint32_t,std::stop_token stop){started=true;while(!stop.stop_requested())std::this_thread::yield();return image(true);});interrupted.request(files/"blue",cycle+1,1);until([&]{return started.load();});interrupted.close();require(!interrupted.take(),"Interrupted worker published after close");}
    ImagePaletteWorker real;const auto decoded=visualizers::decodeImage(files/"opaque.png");real.request(files/"opaque.png",300,123);check(result(real),expected(decoded,123),300);
    real.request(files/"missing-image.png",301,1);const auto missing=result(real);require(!missing.palette&&!missing.error.empty(),"Missing image did not produce a recoverable error");real.request(files/"opaque.png",302,123);check(result(real),expected(decoded,123),302);
    json resources=json::array();for(uint64_t cycle=0;cycle<120;++cycle){real.request(files/"opaque.png",400+cycle,123);check(result(real),expected(decoded,123),400+cycle);if(cycle==23||cycle==71||cycle==119){auto sample=visualizers::testing::resourceMetrics();sample["cycles"]=cycle+1;resources.push_back(sample);}}
    report={{"result","pass"},{"replacement_requests",200},{"decoder_calls_during_replacement",replacementCalls},{"completed_result_supersession","pass"},{"cancel_ready_result","pass"},{"failure_recovery_and_bounds","pass"},{"normal_worker_lifetimes",40},{"interrupted_worker_lifetimes",8},{"real_codec_seeded_palette","pass"}};
    report["real_import_lifetime_samples"]=std::move(resources);
    report["unicode_error_truncation_regression"]="pass";
    std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
 }catch(const std::exception& error){report={{"result","fail"},{"error",error.what()}};if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
