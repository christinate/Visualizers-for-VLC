// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_capture.hpp"
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <atomic>
#include <stdexcept>

namespace visualizers {
namespace {std::atomic<bool> requested{false};}
CapturePermission capturePermission(){@autoreleasepool {
    id usage=[[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"];
    if(![usage isKindOfClass:[NSString class]]||[(NSString*)usage length]==0)return CapturePermission::MissingUsageDescription;
    switch([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]){
        case AVAuthorizationStatusAuthorized:return CapturePermission::Granted;
        case AVAuthorizationStatusDenied:return CapturePermission::Denied;
        case AVAuthorizationStatusRestricted:return CapturePermission::Restricted;
        default:return requested.load(std::memory_order_acquire)?CapturePermission::Pending:CapturePermission::NotDetermined;
    }
}}
void requestCapturePermission(){@autoreleasepool {
    if(![NSThread isMainThread])throw std::runtime_error("Request recording permission on the UI thread");
    if(capturePermission()!=CapturePermission::NotDetermined)return;
    if(requested.exchange(true,std::memory_order_acq_rel))return;
    // The completion is asynchronous on an arbitrary dispatch queue. It owns
    // no stack/UI pointers and cannot outlive unloadable plugin code.
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL){requested.store(false,std::memory_order_release);}];
}}
}
