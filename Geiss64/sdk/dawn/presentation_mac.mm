// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentation_mac.hpp"
#include <SDL_syswm.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>
namespace visualizers::gpu {
namespace {
CAMetalLayer* layerFor(SDL_MetalView view){
    if(!view)return nil;
    id layer=(id)SDL_Metal_GetLayer(view);
    return [layer isKindOfClass:[CAMetalLayer class]]?(CAMetalLayer*)layer:nil;
}
NSScreen* screenFor(SDL_Window* window){
    SDL_SysWMinfo info{};SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window,&info)&&info.subsystem==SDL_SYSWM_COCOA){
        NSWindow* native=info.info.cocoa.window;
        if([native screen])return [native screen];
    }
    return [NSScreen mainScreen];
}
float headroom(CGFloat value){return std::isfinite(value)?float(std::max(CGFloat(1),value)):1.f;}
}
DisplayPresentation metalDisplayPresentation(SDL_Window* window,SDL_MetalView view,bool requested){@autoreleasepool {
    DisplayPresentation result;result.edr=true;
    CAMetalLayer* layer=layerFor(view);NSScreen* screen=screenFor(window);
    if(!layer||!screen){result.reason="No Metal layer/display is available for EDR";return result;}
    result.potentialHeadroom=headroom([screen maximumPotentialExtendedDynamicRangeColorComponentValue]);
    result.headroom=headroom([screen maximumExtendedDynamicRangeColorComponentValue]);
    result.linearTransport=result.potentialHeadroom>1.f;
    result.hdrEnabled=result.linearTransport&&result.headroom>1.f;
    // Potential capability is independent of current brightness. Requesting
    // the layer is necessary before current headroom can rise above one.
    [layer setWantsExtendedDynamicRangeContent:requested&&result.linearTransport];
    result.reason=!result.linearTransport?"This display reports SDR-only EDR capability":
        result.hdrEnabled?"Metal display reports EDR headroom":"Current EDR headroom is 1";
    return result;
}}
void configureMetalPresentationLayer(SDL_MetalView view,bool linear){@autoreleasepool {
    CAMetalLayer* layer=layerFor(view);if(!layer)return;
    CGColorSpaceRef space=CGColorSpaceCreateWithName(linear?kCGColorSpaceExtendedLinearSRGB:kCGColorSpaceSRGB);
    if(!space)throw std::runtime_error("Cannot create the native Metal presentation color space");
    [layer setColorspace:space];CGColorSpaceRelease(space);
    [layer setWantsExtendedDynamicRangeContent:linear];
}}
nlohmann::json metalPresentationLayerInfo(SDL_Window* window,SDL_MetalView view){@autoreleasepool {
    CAMetalLayer* layer=layerFor(view);NSScreen* screen=screenFor(window);
    if(!layer)return {{"metal_layer_available",false}};
    char name[256]="";CGColorSpaceRef space=[layer colorspace];
    CFStringRef value=space?CGColorSpaceGetName(space):nullptr;
    if(value)CFStringGetCString(value,name,sizeof(name),kCFStringEncodingUTF8);
    return {{"metal_layer_available",true},{"metal_edr_requested",bool([layer wantsExtendedDynamicRangeContent])},
        {"metal_color_space",name},{"metal_surface_float16",[layer pixelFormat]==MTLPixelFormatRGBA16Float},
        {"metal_extended_linear_srgb",bool(value&&CFEqual(value,kCGColorSpaceExtendedLinearSRGB))},
        {"metal_srgb",bool(value&&CFEqual(value,kCGColorSpaceSRGB))},
        {"edr_headroom",screen?headroom([screen maximumExtendedDynamicRangeColorComponentValue]):1.f},
        {"edr_potential_headroom",screen?headroom([screen maximumPotentialExtendedDynamicRangeColorComponentValue]):1.f}};
}}
}
