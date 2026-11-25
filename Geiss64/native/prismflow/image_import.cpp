// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_import.hpp"
#include "random.hpp"
#include "diagnostic_text.hpp"
#include <limits>
#include <stdexcept>
namespace prismflow {
ImagePaletteWorker::ImagePaletteWorker(Decoder decoder):decoder_(decoder?std::move(decoder):visualizers::decodeImage),thread_([this](std::stop_token token){run(token);}){}
ImagePaletteWorker::~ImagePaletteWorker(){close();}
void ImagePaletteWorker::advance(){if(generation_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Image import generation exhausted");++generation_;}
void ImagePaletteWorker::request(std::filesystem::path path,uint64_t token,uint32_t seed){
    const auto encoded=path.u8string();if(!path.is_absolute()||encoded.size()>32768||encoded.find(char8_t(0))!=std::u8string::npos||!token)throw std::invalid_argument("Image import needs an absolute file path and nonzero token");
    std::lock_guard lock(mutex_);if(closed_)throw std::runtime_error("Image import worker is closed");advance();activeStop_.request_stop();result_.reset();pending_=Request{std::move(path),generation_,token,seed};wake_.notify_one();
}
void ImagePaletteWorker::cancel(){std::lock_guard lock(mutex_);if(closed_)return;advance();activeStop_.request_stop();pending_.reset();result_.reset();}
void ImagePaletteWorker::close(){
    thread_.request_stop();{std::lock_guard lock(mutex_);closed_=true;activeStop_.request_stop();pending_.reset();result_.reset();}wake_.notify_one();if(thread_.joinable())thread_.join();
}
std::optional<ImageImportResult> ImagePaletteWorker::take(){std::lock_guard lock(mutex_);auto value=std::move(result_);result_.reset();return value;}
ImageImportSnapshot ImagePaletteWorker::snapshot()const{std::lock_guard lock(mutex_);return {running_,bool(pending_),bool(result_),closed_,generation_};}
void ImagePaletteWorker::run(std::stop_token lifetime){
    for(;;){
        std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return closed_||pending_.has_value()||lifetime.stop_requested();});if(closed_||lifetime.stop_requested())return;
        auto request=std::move(*pending_);pending_.reset();activeStop_=std::stop_source{};const auto stop=activeStop_.get_token();running_=true;lock.unlock();
        ImageImportResult result;result.generation=request.generation;result.token=request.token;
        try{
            const auto image=decoder_(request.path,128,stop);visualizers::image_detail::cancelled(stop);Mulberry32 stream(request.seed);const RandomFunction random=[&]{return stream();};
            auto colors=paletteFromImageRgba(image.image.width,image.image.height,image.image.rgba,random,{},stop);auto rgba=imagePaletteRgba(colors);const auto background=companionColor(rgba,random,stop);
            result.palette=PaletteDefinition{std::move(rgba),"Image palette",background,-1};result.width=image.image.width;result.height=image.image.height;
        }catch(const std::exception& error){if(!stop.stop_requested())result.error=visualizers::boundedDiagnostic(error.what());}
        catch(...){if(!stop.stop_requested())result.error="Unknown native image import failure";}
        lock.lock();running_=false;if(closed_||lifetime.stop_requested())return;
        if(request.generation==generation_&&!stop.stop_requested())result_=std::move(result);
    }
}
}
