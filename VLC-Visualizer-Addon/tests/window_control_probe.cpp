// SPDX-License-Identifier: GPL-2.0-or-later
#include "window_control.hpp"
#include <iostream>

int main(int argc,char** argv){
    if(argc!=5)return 2;
    try{
        const std::string action=argv[1];
        const auto path=suite::controlPathFromUtf8(argv[2]);
        const std::string id=argv[3];
        const auto owner=static_cast<unsigned>(std::stoul(argv[4]));
        if(action=="register"){
            suite::WindowControl window(path,id,owner,true);
            std::cout<<"ready"<<std::endl;
            for(std::string command;std::getline(std::cin,command);){
                if(command=="quit")return 0;
                if(command=="close")std::cout<<window.closeRequested()<<std::endl;
                else if(command=="input")std::cout<<window.inputRequested()<<std::endl;
                else return 2;
            }
        }else if(action=="close"){
            std::cout<<suite::closeWindows(path,id,owner)<<'\n';
        }else if(action=="input"){
            std::cout<<suite::selectCaptureInput(path,id,owner)<<'\n';
        }else return 2;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
