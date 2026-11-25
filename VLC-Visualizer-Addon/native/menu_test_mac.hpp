// SPDX-License-Identifier: GPL-2.0-or-later
// Owner-local AppKit integration fixture. Excluded from normal release builds.
#pragma once
#include "catalog.hpp"
#import <AppKit/AppKit.h>

namespace menu_test {
struct State {uint64_t sequence=0;native::json observed;};
inline State* active=nullptr; // Only the AppKit main thread accesses this fixture.
inline bool enabled(){return getenv("VLC_VISUALIZERS_SELF_TEST")&&getenv("VLC_VISUALIZERS_MENU_CONTROL_TEST");}
inline void notification(const std::string& title,const std::string& message){
    if(active)active->observed["notifications"].push_back({{"title",title},{"message",message}});
}
inline void launch(unsigned pid,NSArray<NSString*>* args){
    if(!active)return;
    native::json values=native::json::array();for(NSString* value in args)values.push_back([value UTF8String]);
    active->observed["launches"].push_back({{"frontend_pid",pid},{"arguments",values}});
}
inline native::json snapshot(NSMenuItem* root,const native::json& entries){
    native::json rows=native::json::array();NSMenu* menu=[root submenu];
    for(size_t n=0;n<entries.size();++n){
        NSMenu* submenu=[[menu itemAtIndex:n] submenu];native::json actions=native::json::array();
        for(NSMenuItem* action in [submenu itemArray]){
            auto selector=[action action];actions.push_back({{"title",[[action title] UTF8String]},
                {"selector",selector?[NSStringFromSelector(selector) UTF8String]:""},{"tag",[action tag]},
                {"enabled",bool([action isEnabled])},{"target_present",[action target]!=nil}});
        }
        rows.push_back({{"id",entries[n].at("id")},{"actions",actions}});
    }
    return rows;
}
inline void poll(id<NSMenuDelegate> controller,NSMenuItem* root,const native::fs::path& suite,
                 const native::json& entries,State& state){
    if(!enabled()||active)return;
    const auto file=suite/"menu-control-command.json";if(!native::fs::is_regular_file(file))return;
    uint64_t sequence=0;
    try{
        if(native::fs::file_size(file)>4096)throw std::runtime_error("Oversized menu fixture command");
        const auto command=native::read(file);sequence=command.at("sequence").get<uint64_t>();
        if(command.at("pid")!=getpid()||!sequence||sequence<=state.sequence)return;
        state.sequence=sequence;state.observed={{"protocol",1},{"pid",getpid()},{"sequence",sequence},
            {"result","pass"},{"notifications",native::json::array()},{"launches",native::json::array()}};
        struct Active {Active(State& s){active=&s;}~Active(){active=nullptr;}} scoped(state);
        const auto action=command.at("action").get<std::string>();state.observed["action"]=action;
        NSMenu* menu=[root submenu];NSMenu* selected=menu;NSInteger index=-1;
        if(action=="inspect"){
            [controller menuNeedsUpdate:menu];
        }else{
            if(action=="close-all"||action=="rescan"){
                const SEL wanted=NSSelectorFromString(action=="rescan"?@"rescan:":@"closeVisualizer:");
                for(NSInteger n=0;n<[menu numberOfItems];++n){NSMenuItem* item=[menu itemAtIndex:n];
                    if([item action]==wanted&&(action=="rescan"||[item tag]==-1)){index=n;break;}}
            }else{
                const auto id=command.at("id").get<std::string>();size_t n=0;
                while(n<entries.size()&&entries[n].at("id")!=id)++n;
                if(n==entries.size())throw std::runtime_error("No menu item matches fixture ID");
                selected=[[menu itemAtIndex:n] submenu];
                if(action=="open"||action=="invoke-open")index=0;
                else if(action=="settings"||action=="invoke-settings")index=1;
                else if(action=="close")index=2;
            }
            if(index<0)throw std::runtime_error("Unknown/missing menu fixture action");
            NSMenuItem* item=[selected itemAtIndex:index];
            state.observed["enabled_at_dispatch"]=bool([item isEnabled]);
            state.observed["selector"]=[NSStringFromSelector([item action]) UTF8String];
            if(action.rfind("invoke-",0)==0){
                // Explicitly emulate delivery of an action held before the
                // package changed, without refreshing this open menu first.
                state.observed["dispatch"]="NSApplication target/action delivery";
                if(![NSApp sendAction:[item action] to:[item target] from:item])throw std::runtime_error("Action target refused delivery");
            }else{
                state.observed["dispatch"]="NSMenu performActionForItemAtIndex";
                [selected performActionForItemAtIndex:index];
            }
        }
        state.observed["menu"]=snapshot(root,entries);
        native::write(suite/"menu-control-response.json",state.observed);
    }catch(const std::exception& error){
        if(sequence)state.sequence=sequence;
        native::write(suite/"menu-control-response.json",{{"protocol",1},{"pid",getpid()},
            {"sequence",sequence},{"result","fail"},{"error",error.what()}});
    }
}
}
