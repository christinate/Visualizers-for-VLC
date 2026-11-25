// SPDX-License-Identifier: GPL-2.0-or-later
#include "image_decode.hpp"
#include <QColorSpace>
#include <QImage>
#include <QImageReader>
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace visualizers {
DecodedImage decodeImage(const std::filesystem::path& file,uint32_t size,std::stop_token stop){
    image_detail::validateFile(file,size,stop);const auto gif=image_detail::gifFirstFrame(file,stop);DecodedImage result;QImage pixels;
    {QImageReader reader(QString::fromUtf8(file.string().c_str()));reader.setAutoTransform(true);reader.setDecideFormatFromContent(true);
        const auto original=reader.size();if(!original.isValid())throw std::runtime_error("Unsupported or corrupt image: "+reader.errorString().toStdString());const auto desired=image_detail::dimensions(uint32_t(original.width()),uint32_t(original.height()),size);
        if(!gif)reader.setScaledSize(QSize(int(desired[0]),int(desired[1])));pixels=reader.read();if(pixels.isNull())throw std::runtime_error("Cannot decode image: "+reader.errorString().toStdString());image_detail::cancelled(stop);
        result.sourceWidth=uint32_t(original.width());result.sourceHeight=uint32_t(original.height());result.backend="Qt 6 QImageReader";result.sourceProfileReported=pixels.colorSpace().isValid();
    } // Release the decoder's frame cache before modifying its returned bitmap.
    if(result.sourceProfileReported){pixels=pixels.convertedToColorSpace(QColorSpace::SRgb);if(pixels.isNull())throw std::runtime_error("Cannot convert image profile to sRGB");}else pixels.setColorSpace(QColorSpace::SRgb);
    // Codecs can ignore scaledSize; enforce the output cap after decoding.
    const auto finalSize=image_detail::dimensions(uint32_t(pixels.width()),uint32_t(pixels.height()),size);pixels=pixels.convertToFormat(QImage::Format_ARGB32_Premultiplied);if(pixels.isNull())throw std::runtime_error("Cannot convert image alpha layout");
    if(gif){if(uint32_t(pixels.width())!=gif->canvasWidth||uint32_t(pixels.height())!=gif->canvasHeight)throw std::runtime_error("GIF decoder did not retain its logical canvas");const auto frame=QRect(int(gif->left),int(gif->top),int(gif->width),int(gif->height)).intersected(pixels.rect());
        for(int y=0;y<pixels.height();++y){if((y&63)==0)image_detail::cancelled(stop);auto* row=reinterpret_cast<QRgb*>(pixels.scanLine(y));if(frame.isEmpty()||y<frame.top()||y>frame.bottom())std::fill(row,row+pixels.width(),0);else{std::fill(row,row+frame.left(),0);std::fill(row+frame.right()+1,row+pixels.width(),0);}}
    }
    if(uint32_t(pixels.width())!=finalSize[0]||uint32_t(pixels.height())!=finalSize[1])pixels=pixels.scaled(int(finalSize[0]),int(finalSize[1]),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    if(pixels.isNull())throw std::runtime_error("Cannot downsample image");pixels=pixels.convertToFormat(QImage::Format_RGBA8888_Premultiplied);if(pixels.isNull())throw std::runtime_error("Cannot pack image RGBA pixels");result.image.width=uint32_t(pixels.width());result.image.height=uint32_t(pixels.height());result.image.rgba.resize(size_t(result.image.width)*result.image.height*4);
    for(uint32_t y=0;y<result.image.height;++y)std::memcpy(result.image.rgba.data()+size_t(y)*result.image.width*4,pixels.constScanLine(int(y)),size_t(result.image.width)*4);
    image_detail::unpremultiply(result.image);image_detail::cancelled(stop);return result;
}
}
