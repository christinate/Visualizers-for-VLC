// SPDX-License-Identifier: GPL-2.0-or-later
#include <vlc_common.h>
#include <vlc_interface.h>
#include "catalog.hpp"
#include "window_control.hpp"
#include <atomic>
#include <thread>
#include <memory>
#import <AppKit/AppKit.h>
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
#include "menu_test_mac.hpp"
#endif
extern "C" void playlist_EnableAudioFilter(playlist_t*,const char*,bool);
static NSString* text(const std::string& value){return [NSString stringWithUTF8String:value.c_str()];}
static bool controlledTest(){
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
    return menu_test::enabled();
#else
    return false;
#endif
}
static void showNotice(NSString* title,const std::string& message){
    const auto diagnostic=suite::catalogDiagnostic<native::json>(message);
    NSAlert* alert=[[NSAlert alloc] init];[alert setMessageText:title];[alert setInformativeText:text(diagnostic)];
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
    NSTimer* dismiss=nil;
    if(menu_test::active){menu_test::notification([title UTF8String],diagnostic);
        dismiss=[NSTimer timerWithTimeInterval:.1 repeats:NO block:^(NSTimer*){[NSApp stopModalWithCode:NSAlertFirstButtonReturn];}];
        [[NSRunLoop mainRunLoop] addTimer:dismiss forMode:NSModalPanelRunLoopMode];}
#endif
    [alert runModal];[alert release];
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
    [dismiss invalidate];
#endif
}
@interface SuiteNativeVisualizerMenu : NSObject <NSMenuDelegate> {
@public
    native::fs::path suite;
    native::json entries;
    NSMenuItem* item;
    NSTimer* timer;
    bool testing;
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
    menu_test::State testState;
#endif
}
- (void)attach:(NSTimer*)sender;
- (void)activate:(NSMenuItem*)sender;
- (void)rescan:(id)sender;
- (void)closeVisualizer:(NSMenuItem*)sender;
- (void)close;
@end
@implementation SuiteNativeVisualizerMenu
- (void)attach:(NSTimer*)sender {
    if(item){
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
        menu_test::poll(self,item,suite,entries,testState);
#endif
        return;
    }
    NSMenu* bar=[NSApp mainMenu];if(!bar||[bar numberOfItems]<2)return;
    item=[[NSMenuItem alloc] initWithTitle:@"Visualizers" action:nil keyEquivalent:@""];
    NSMenu* menu=[[NSMenu alloc] initWithTitle:@"Visualizers"];[menu setDelegate:self];[item setSubmenu:menu];[menu release];
    NSInteger view=[bar indexOfItemWithTitle:text(vlc_gettext("View"))];
    NSInteger index=view>=0?view+1:[bar indexOfItemWithSubmenu:[NSApp helpMenu]];
    if(index<0)index=[bar indexOfItemWithTitle:text(vlc_gettext("Help"))];
    if(index<0)index=MAX(0,[bar numberOfItems]-1);[bar insertItem:item atIndex:index];
    if(testing){[self menuNeedsUpdate:[item submenu]];native::json order=native::json::array();for(NSMenuItem* existing in [bar itemArray])order.push_back([[existing title] UTF8String]);native::write(suite/"menu-test.json",{{"attached",true},{"integration","cocoa-inline"},{"entries",entries.size()},{"menu_order",order}});if(!entries.empty()&&!controlledTest()){size_t selected=0;if(const char* wanted=getenv("VLC_VISUALIZERS_TEST_RENDERER"))for(size_t n=0;n<entries.size();++n)if(entries[n].at("id")==wanted)selected=n;NSMenuItem* first=[[[[item submenu] itemAtIndex:selected] submenu] itemAtIndex:0];[self activate:first];}}
}
- (void)menuNeedsUpdate:(NSMenu*)menu {
    [menu removeAllItems];entries=native::json::array();
    // A damaged local cache must not throw through a Cocoa delegate into VLC.
    try{auto cached=native::read(suite/"installed-visualizers.json").at("visualizers");if(cached.is_array())for(const auto& entry:cached){if(entries.size()>=128)break;if(entry.is_object()&&entry.contains("id")&&entry["id"].is_string()&&entry.contains("name")&&entry["name"].is_string()&&entry.contains("package")&&entry["package"].is_string()&&entry.contains("entrypoint")&&entry["entrypoint"].is_string())entries.push_back(entry);}}catch(...){ }
    size_t n=0;for(const auto& entry:entries){if(n>=128)break;NSMenuItem* package=[[NSMenuItem alloc] initWithTitle:text(entry.at("name")) action:nil keyEquivalent:@""];NSMenu* submenu=[[NSMenu alloc] initWithTitle:[package title]];
        bool valid=true;try{auto path=native::contained(suite,entry.at("package").get<std::string>());native::validate(suite,path);}catch(...){valid=false;}
        for(int settings=0;settings<2;++settings){NSMenuItem* action=[[NSMenuItem alloc] initWithTitle:settings?@"Settings…":@"Open" action:@selector(activate:) keyEquivalent:@""];[action setTarget:self];[action setTag:100+n*2+settings];[action setEnabled:valid];[submenu addItem:action];[action release];}
        NSMenuItem* close=[submenu addItemWithTitle:@"Close" action:@selector(closeVisualizer:) keyEquivalent:@""];[close setTarget:self];[close setTag:n];
        [submenu setAutoenablesItems:NO];[package setSubmenu:submenu];[submenu release];[menu addItem:package];[package release];++n;
    }
    if(entries.empty()){NSMenuItem* empty=[menu addItemWithTitle:@"No visualizers installed" action:nil keyEquivalent:@""];[empty setEnabled:NO];}
    NSMenuItem* closeAll=[menu addItemWithTitle:@"Close all visualizers" action:@selector(closeVisualizer:) keyEquivalent:@""];[closeAll setTarget:self];[closeAll setTag:-1];
    [menu addItem:[NSMenuItem separatorItem]];NSMenuItem* scan=[menu addItemWithTitle:@"Rescan visualizers" action:@selector(rescan:) keyEquivalent:@""];[scan setTarget:self];[menu setAutoenablesItems:NO];
}
- (void)activate:(NSMenuItem*)sender {
    try{NSInteger index=([sender tag]-100)/2;if(index<0||size_t(index)>=entries.size())return;auto entry=entries[index];auto package=native::contained(suite,entry.at("package").get<std::string>());auto current=native::validate(suite,package);if(current.at("id")!=entry.at("id"))throw std::runtime_error("Package changed since scanning");
        NSMutableArray* args=[NSMutableArray arrayWithObjects:([sender tag]%2)?@"--settings":@"--visualizer",text(entry.at("id")),@"--vlc-pid",[NSString stringWithFormat:@"%d",getpid()],nil];
        if(testing){
            [args addObjectsFromArray:@[@"--background",@"--quit-after",controlledTest()?@"60000":@"20000",@"--profile",text((suite/"menu-test-profile").string())]];
            if(!controlledTest()||[sender tag]%2)[args addObject:@"--self-test"];
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
            if(controlledTest())[args addObjectsFromArray:@[@"--output-dir",text((suite/"menu-actions"/std::to_string(testState.sequence)).string())]];
#endif
        }
        NSTask* task=[[NSTask alloc] init];[task setExecutableURL:[NSURL fileURLWithPath:text((suite/"Visualizers").string())]];[task setArguments:args];NSError* error=nil;BOOL ok=[task launchAndReturnError:&error];
#ifdef VIS_NATIVE_MENU_TEST_CONTROL
        if(ok)menu_test::launch(unsigned([task processIdentifier]),args);
#endif
        [task release];if(!ok)throw std::runtime_error([[error localizedDescription] UTF8String]);
    }catch(const std::exception& error){showNotice(@"Visualizer unavailable",error.what());}
}
- (void)rescan:(id)sender {try{native::scan(suite);}catch(const std::exception& error){showNotice(@"Scan failed",error.what());}}
- (void)closeVisualizer:(NSMenuItem*)sender {try{NSInteger index=[sender tag];std::string id=index<0?"*":entries.at(size_t(index)).at("id").get<std::string>();suite::closeWindows(suite,id,getpid());}catch(const std::exception& error){showNotice(@"Close failed",error.what());}}
- (void)close {[timer invalidate];timer=nil;if(item){[[NSApp mainMenu] removeItem:item];[item release];item=nil;}}
@end
struct MacMenuState {std::atomic<bool> stopped{false};std::thread worker;SuiteNativeVisualizerMenu* controller=nil;};
struct intf_sys_t {std::shared_ptr<MacMenuState> state;};
extern "C" int NativeMenuOpen(vlc_object_t* object){
    auto intf=reinterpret_cast<intf_thread_t*>(object);auto sys=new(std::nothrow)intf_sys_t;if(!sys)return VLC_ENOMEM;intf->p_sys=sys;sys->state=std::make_shared<MacMenuState>();auto state=sys->state;
    state->worker=std::thread([intf,state]{playlist_EnableAudioFilter(pl_Get(intf),"vlc_visualizers",true);dispatch_async(dispatch_get_main_queue(),^{if(state->stopped)return;auto controller=[[SuiteNativeVisualizerMenu alloc] init];state->controller=controller;const char* path=getenv("VLC_VISUALIZERS_TEST_SUITE");if(!path)path=getenv("VLC_VISUALIZERS_SUITE");controller->suite=path?native::fs::path(path):native::fs::path(getenv("HOME"))/"Library/Application Support/VLCVisualizers";controller->testing=getenv("VLC_VISUALIZERS_SELF_TEST")!=nullptr;controller->timer=[NSTimer scheduledTimerWithTimeInterval:.2 target:controller selector:@selector(attach:) userInfo:nil repeats:YES];});});return VLC_SUCCESS;
}
extern "C" void NativeMenuClose(vlc_object_t* object){auto intf=reinterpret_cast<intf_thread_t*>(object);auto sys=intf->p_sys;if(!sys)return;auto state=sys->state;state->stopped=true;if(state->worker.joinable())state->worker.join();dispatch_async(dispatch_get_main_queue(),^{[state->controller close];[state->controller release];state->controller=nil;});delete sys;intf->p_sys=nullptr;}
