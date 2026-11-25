// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "image_palette.hpp"
#include "image_decode.hpp"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
namespace prismflow {
struct ImageImportResult {
    uint64_t generation=0,token=0;std::optional<PaletteDefinition> palette;
    uint32_t width=0,height=0;std::string error;
};
struct ImageImportSnapshot {bool running=false,pending=false,ready=false,closed=false;uint64_t generation=0;};
// One CPU worker, one running request, one replaceable pending request and one
// result. It never reads live presets, GUI objects or GPU handles. The render
// thread alone publishes a completed palette. Even an uninterruptible codec
// cannot publish after replacement/cancellation; destruction joins it.
class ImagePaletteWorker {
public:
    using Decoder=std::function<visualizers::DecodedImage(const std::filesystem::path&,uint32_t,std::stop_token)>;
    explicit ImagePaletteWorker(Decoder decoder={});~ImagePaletteWorker();
    ImagePaletteWorker(const ImagePaletteWorker&)=delete;ImagePaletteWorker& operator=(const ImagePaletteWorker&)=delete;
    void request(std::filesystem::path,uint64_t token,uint32_t seed);
    void cancel();void close();
    std::optional<ImageImportResult> take();
    ImageImportSnapshot snapshot()const;
private:
    struct Request {std::filesystem::path path;uint64_t generation,token;uint32_t seed;};
    Decoder decoder_;mutable std::mutex mutex_;std::condition_variable wake_;
    std::optional<Request> pending_;std::optional<ImageImportResult> result_;
    std::stop_source activeStop_;uint64_t generation_=0;bool running_=false,closed_=false;
    std::jthread thread_;
    void advance();void run(std::stop_token);
};
}
