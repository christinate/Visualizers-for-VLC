// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_ui.hpp"
#import <AppKit/AppKit.h>
#include <cmath>
#include <exception>

namespace visualizers {namespace {
NSString* text(const std::string& value){return [NSString stringWithUTF8String:value.c_str()];}
struct Control {SettingField field;__strong NSView* widget;std::string visible;SettingsJson selected;};
NSString* fieldText(NSTextField* field){NSText* editor=[field currentEditor];return editor?[editor string]:[field stringValue];}
SettingsJson changes(const std::vector<Control>& controls,const SettingsJson& original){
    auto patch=SettingsJson::object();for(const auto& item:controls){const auto& field=item.field;
        if(field.type==SettingType::Boolean){const bool checked=[(NSButton*)item.widget state]==NSControlStateValueOn;if(checked!=original.at(field.key).get<bool>())patch[field.key]=checked;}
        else if(field.type==SettingType::Choice){const auto row=[(NSPopUpButton*)item.widget indexOfSelectedItem];if(row<0||size_t(row)>=field.choices.size())throw std::runtime_error("Choose a valid "+field.label);const auto& value=field.choices[size_t(row)].value;if(value!=item.selected)patch[field.key]=value;}
        else if(field.type==SettingType::Ini){const std::string current=[[(NSTextView*)item.widget string] UTF8String];if(current!=item.visible)patch[field.key]=current;}
        else{NSTextField* input=(NSTextField*)item.widget;NSString* current=fieldText(input);if(std::string([current UTF8String])!=item.visible){
            NSNumberFormatter* formatter=(NSNumberFormatter*)[input formatter];id parsed=nil;NSRange range=NSMakeRange(0,[current length]);NSError* error=nil;
            if(![formatter getObjectValue:&parsed forString:current range:&range error:&error]||NSMaxRange(range)!=[current length]||![parsed isKindOfClass:[NSNumber class]])throw std::runtime_error("Check "+field.label+"; enter a finite number");
            const auto number=[(NSNumber*)parsed doubleValue];if(field.type==SettingType::Integer&&std::isfinite(number)&&number==std::floor(number)&&number>=field.minimum&&number<=field.maximum)patch[field.key]=int(number);else patch[field.key]=number;
        }}
    }return patch;
}
}
std::optional<SettingsJson> editNativeSettings(const SettingsJson& values,const std::string& id,const std::string& title,
        const std::function<bool()>& keepOpen,const SettingsAutomation* automation,const std::string& notice){@autoreleasepool {
    const bool ini=values.contains("sceneIni");const auto fields=settingsFields(id,ini);if(automation)validateSettingsPatch(fields,automation->edits);
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];if(!automation)[NSApp activateIgnoringOtherApps:YES];
    NSAlert* alert=[[NSAlert alloc] init];[alert setMessageText:text(title+" settings")];[alert setInformativeText:text(notice.empty()?settingsNote():notice)];[alert addButtonWithTitle:@"Save"];[alert addButtonWithTitle:@"Cancel"];
    const double height=double(fields.size())*32+(ini?132:0);NSView* view=[[NSView alloc] initWithFrame:NSMakeRect(0,0,520,height)];std::vector<Control> controls;
    for(size_t index=0;index<fields.size();++index){const auto& field=fields[index];const auto& value=values.at(field.key);const double y=height-32*(index+1);NSView* widget=nil;std::string visible;SettingsJson selected;
        if(field.type==SettingType::Boolean){NSButton* box=[[NSButton alloc] initWithFrame:NSMakeRect(0,y,520,26)];[box setButtonType:NSButtonTypeSwitch];[box setTitle:text(field.label)];[box setState:value.get<bool>()?NSControlStateValueOn:NSControlStateValueOff];widget=box;}
        else if(field.type==SettingType::Ini){
            NSTextField* label=[NSTextField labelWithString:text(field.label)];[label setFrame:NSMakeRect(0,y+4,520,22)];[view addSubview:label];
            NSScrollView* scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,520,132)];
            [scroll setHasVerticalScroller:YES];[scroll setBorderType:NSBezelBorder];
            NSTextView* editor=[[NSTextView alloc] initWithFrame:NSMakeRect(0,0,500,132)];
            [editor setRichText:NO];[editor setAutomaticQuoteSubstitutionEnabled:NO];[editor setAutomaticDashSubstitutionEnabled:NO];
            [editor setVerticallyResizable:YES];[editor setAutoresizingMask:NSViewWidthSizable];
            [editor setFont:[NSFont userFixedPitchFontOfSize:12]];[editor setString:text(value.get<std::string>())];
            [scroll setDocumentView:editor];[view addSubview:scroll];
            controls.push_back({field,editor,[[editor string] UTF8String],selected});continue;
        }else{NSTextField* label=[NSTextField labelWithString:text(field.label)];[label setFrame:NSMakeRect(0,y,315,26)];[view addSubview:label];
            if(field.type==SettingType::Choice){NSPopUpButton* menu=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(320,y,200,28) pullsDown:NO];NSInteger row=0;
                for(size_t n=0;n<field.choices.size();++n){[menu addItemWithTitle:text(field.choices[n].label)];if(field.choices[n].value==value)row=NSInteger(n);}
                [menu selectItemAtIndex:row];selected=field.choices[size_t(row)].value;widget=menu;
            }else{NSTextField* input=[[NSTextField alloc] initWithFrame:NSMakeRect(320,y,200,26)];NSNumberFormatter* formatter=[[NSNumberFormatter alloc] init];[formatter setNumberStyle:NSNumberFormatterDecimalStyle];[formatter setUsesGroupingSeparator:NO];[formatter setAllowsFloats:field.type==SettingType::Number];[formatter setMaximumFractionDigits:field.type==SettingType::Number?9:0];[input setFormatter:formatter];[input setDoubleValue:value.get<double>()];visible=[[input stringValue] UTF8String];widget=input;}
        }
        [view addSubview:widget];controls.push_back({field,widget,visible,selected});
    }
    [alert setAccessoryView:view];__block unsigned ticks=0;__block std::exception_ptr failure;
    NSTimer* timer=[NSTimer timerWithTimeInterval:.05 repeats:YES block:^(NSTimer*){try{
        if(!keepOpen()){[NSApp abortModal];return;}
        if(automation&&++ticks==2){for(const auto& item:controls){const auto& field=item.field;if(!automation->edits.contains(field.key))continue;const auto& value=automation->edits.at(field.key);
                if(field.type==SettingType::Boolean)[(NSButton*)item.widget setState:value.get<bool>()?NSControlStateValueOn:NSControlStateValueOff];
                else if(field.type==SettingType::Choice){for(size_t n=0;n<field.choices.size();++n)if(field.choices[n].value==value)[(NSPopUpButton*)item.widget selectItemAtIndex:n];}
                else if(field.type==SettingType::Ini)[(NSTextView*)item.widget setString:text(value.get<std::string>())];
                else [(NSTextField*)item.widget setDoubleValue:value.get<double>()];}
            [NSApp stopModalWithCode:automation->cancel?NSModalResponseCancel:NSAlertFirstButtonReturn];}
    }catch(...){failure=std::current_exception();[NSApp abortModal];}}];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];std::optional<SettingsJson> result;
    for(;;){const auto response=[alert runModal];if(response!=NSAlertFirstButtonReturn)break;
        try{auto patch=changes(controls,values);validateSettingsPatch(fields,patch);result=std::move(patch);break;}catch(const std::exception& error){[alert setInformativeText:text(error.what())];}}
    [timer invalidate];if(failure)std::rethrow_exception(failure);return result;
}}
}
