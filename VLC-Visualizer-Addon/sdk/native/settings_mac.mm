#include "settings.hpp"
#import <AppKit/AppKit.h>
bool native::editSettings(Settings& value,const std::string& title,int,char**){@autoreleasepool {
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];[NSApp activateIgnoringOtherApps:YES];
    NSAlert* alert=[[NSAlert alloc] init];[alert setMessageText:[NSString stringWithUTF8String:(title+" settings").c_str()]];[alert addButtonWithTitle:@"Save"];[alert addButtonWithTitle:@"Cancel"];
    NSView* view=[[NSView alloc] initWithFrame:NSMakeRect(0,0,430,210)];
    NSButton* full=[[NSButton alloc] initWithFrame:NSMakeRect(0,175,430,24)];[full setButtonType:NSButtonTypeSwitch];[full setTitle:@"Always open visualizer in full screen"];[full setState:value.fullscreen?NSControlStateValueOn:NSControlStateValueOff];[view addSubview:full];
    NSTextField* fields[3];int values[3]={value.width,value.height,value.fps};NSArray* labels=@[@"Window width",@"Window height",@"Frame limit"];
    for(int n=0;n<3;++n){NSTextField* label=[NSTextField labelWithString:labels[n]];[label setFrame:NSMakeRect(0,140-n*35,220,24)];[view addSubview:label];fields[n]=[[NSTextField alloc] initWithFrame:NSMakeRect(240,140-n*35,150,24)];[fields[n] setIntegerValue:values[n]];[view addSubview:fields[n]];}
    NSTextField* note=[NSTextField labelWithString:@"This OpenGL backend uses SDR output."];[note setFrame:NSMakeRect(0,0,420,30)];[view addSubview:note];[alert setAccessoryView:view];
    bool accepted=false;for(;;){if([alert runModal]!=NSAlertFirstButtonReturn)break;int w=[fields[0] intValue],h=[fields[1] intValue],f=[fields[2] intValue];if(w>=320&&w<=3840&&h>=240&&h<=2160&&f>=15&&f<=144){value.width=w;value.height=h;value.fps=f;value.fullscreen=[full state]==NSControlStateValueOn;accepted=true;break;}[alert setInformativeText:@"Use width 320–3840, height 240–2160, and frame limit 15–144."];}
    for(auto field:fields)[field release];[full release];[view release];[alert release];return accepted;
}}
