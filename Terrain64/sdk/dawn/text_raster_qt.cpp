// SPDX-License-Identifier: GPL-2.0-or-later
#include "text_raster.hpp"
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <algorithm>
#include <stdexcept>
namespace visualizers {
RasterizedText rasterizeText(std::string_view utf8,const TextRasterOptions& options){
    raster_detail::validate(options);const auto text=raster_detail::normalizeText(utf8);if(text.empty())return {};const auto family=raster_detail::fontFamily(options.family);
    auto* app=qobject_cast<QGuiApplication*>(QCoreApplication::instance());if(!app||QThread::currentThread()!=app->thread())throw std::runtime_error("Native font rasterization requires the host UI session/thread");
    const auto characters=QString::fromUtf8(text.data(),qsizetype(text.size()));QImage device(1,1,QImage::Format_ARGB32_Premultiplied);device.setDotsPerMeterX(3780);device.setDotsPerMeterY(3780);QFont font(QString::fromUtf8(family.data(),qsizetype(family.size())));font.setPointSizeF(options.fontPixels*options.supersample*72./device.logicalDpiY());font.setWeight(QFont::Weight(options.weight));font.setStyleStrategy(QFont::PreferAntialias);font.setKerning(true);
    const QFontMetricsF fm(font,&device);const auto bounds=fm.tightBoundingRect(characters);TextRasterMetrics metrics{-bounds.left(),bounds.right(),-bounds.top(),bounds.bottom(),fm.horizontalAdvance(characters),0,options.supersample,QFontInfo(font).family().toUtf8().toStdString()};if(std::all_of(text.begin(),text.end(),[](char c){return c==' ';}))metrics.left=metrics.right=metrics.ascent=metrics.descent=0;
    auto result=raster_detail::allocate(options,metrics);QImage image(int(result.image.width),int(result.image.height),QImage::Format_ARGB32_Premultiplied);if(image.isNull())throw std::runtime_error("Cannot allocate native text image");image.setDotsPerMeterX(3780);image.setDotsPerMeterY(3780);image.fill(Qt::transparent);
    {QPainter painter(&image);if(!painter.isActive())throw std::runtime_error("Cannot create native text painter");painter.setRenderHint(QPainter::TextAntialiasing,true);painter.setFont(font);painter.setPen(Qt::white);painter.drawText(QPointF(result.metrics.padding+result.metrics.left,result.metrics.padding+result.metrics.ascent),characters);}
    for(uint32_t y=0;y<result.image.height;++y){const auto* row=reinterpret_cast<const QRgb*>(image.constScanLine(int(y)));for(uint32_t x=0;x<result.image.width;++x){const uint8_t a=qAlpha(row[x]);const size_t n=(size_t(y)*result.image.width+x)*4;result.image.rgba[n]=result.image.rgba[n+1]=result.image.rgba[n+2]=a?255:0;result.image.rgba[n+3]=a;}}return result;
}
}
