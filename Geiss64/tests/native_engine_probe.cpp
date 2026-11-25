// CPU-only lifecycle coverage, allowing leak checks without OpenGL drivers.
#include "engine.h"
#include <cmath>
#include <iostream>
int main(){
    unsigned char wave[2][576]{},spectrum[2][576]{};
    for(int cycle=0;cycle<16;++cycle){
        if(!GeissInitialize())return 1;
        for(int frame=0;frame<90;++frame){
            for(int n=0;n<576;++n)for(int ch=0;ch<2;++ch){wave[ch][n]=static_cast<unsigned char>(static_cast<signed char>(100*std::sin((n+frame)*(.03+.01*ch))));spectrum[ch][n]=static_cast<unsigned char>(80*std::abs(std::sin(n*.01+frame*.02)));}
            int w=0,h=0;auto pixels=GeissRender(wave,spectrum,30,&w,&h);if(!pixels||w!=640||h!=480)return 2;
            if(frame%30==0)GeissKey(' ');
        }
        GeissShutdown();
    }
    std::cout<<"PASS: 16 CPU engine init/render/quit cycles, 1440 frames\n";
}
