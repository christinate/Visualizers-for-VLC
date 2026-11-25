// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_picker.hpp"
#import <AppKit/AppKit.h>
#include <exception>
#include <stdexcept>

namespace visualizers {
void pumpNativeUi(){@autoreleasepool {
    [NSApplication sharedApplication];
    while(NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])[NSApp sendEvent:event];
    [NSApp updateWindows];
}}
std::optional<std::string> pickAudioInput(const std::vector<InputDevice>& devices,const std::string& current,
        const std::function<bool()>& keepOpen,const PickerAutomation* automation,const std::string& notice){@autoreleasepool {
    if(devices.size()>1024)throw std::runtime_error("Audio input list exceeds its limit");
    [NSApplication sharedApplication];
    NSAlert* alert=[[NSAlert alloc] init];[alert setMessageText:@"Audio input"];
    [alert setInformativeText:notice.empty()?@"Choose a recording input. ⌘L or Ctrl-L opens this list again.":[NSString stringWithUTF8String:notice.c_str()]];
    [alert addButtonWithTitle:@"Use input"];[alert addButtonWithTitle:@"Cancel"];
    NSPopUpButton* choices=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0,0,480,28) pullsDown:NO];
    [choices addItemWithTitle:@"Default recording input"];
    for(const auto& device:devices){
        const std::string label=device.label+(device.isDefault?" (default)":"")+" — "+device.id;
        NSString* text=[NSString stringWithUTF8String:label.c_str()];
        if(!text){[choices release];[alert release];throw std::runtime_error("Audio input text is not valid UTF-8");}
        // NSMenu permits duplicate labels; identity is always the selected row.
        NSMenuItem* item=[[NSMenuItem alloc] initWithTitle:text action:nil keyEquivalent:@""];
        [[choices menu] addItem:item];[item release];
    }
    [choices selectItemAtIndex:selectedInputRow(devices,current)];[alert setAccessoryView:choices];
    __block unsigned ticks=0;__block std::exception_ptr failure;
    NSTimer* timer=[NSTimer timerWithTimeInterval:.05 repeats:YES block:^(NSTimer*){
        try{
            if(!keepOpen()){[NSApp abortModal];return;}
            if(automation&&++ticks==automation->pollsBeforeAction){
                if(automation->cancel)[NSApp abortModal];
                else {if(automation->row)[choices selectItemAtIndex:*automation->row];[NSApp stopModalWithCode:NSAlertFirstButtonReturn];}
            }
        }catch(...){failure=std::current_exception();[NSApp abortModal];}
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
    const auto response=[alert runModal];[timer invalidate];
    const auto row=[choices indexOfSelectedItem];[choices release];[alert release];
    if(failure)std::rethrow_exception(failure);
    if(response!=NSAlertFirstButtonReturn||row<0)return std::nullopt;
    return inputRowId(devices,unsigned(row));
}}
}
