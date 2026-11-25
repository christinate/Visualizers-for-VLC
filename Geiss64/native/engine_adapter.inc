#include "engine.h"
static winampVisModule native_input;
winampVisModule* g_this_mod=&native_input;
bool GeissInitialize(){
    FXW=640;FXH=480;iDispBits=32;iVSizePercent=100;bMMX=false;bBypassAssembly=true;
    REMAP=reinterpret_cast<unsigned char*>((reinterpret_cast<uintptr_t>(_REMAP_VALUES)+255)&~uintptr_t(255));REMAP2=REMAP+256;REMAP3=REMAP+512;
    OnFxwFxhUpdated();return FX_Init()!=0;
}
void GeissShutdown(){FX_Fini();}
const unsigned char* GeissRender(const unsigned char wave[2][576],const unsigned char spectrum[2][576],float frameRate,int* width,int* height){
    std::memcpy(native_input.waveformData,wave,sizeof(native_input.waveformData));std::memcpy(native_input.spectrumData,spectrum,sizeof(native_input.spectrumData));
    fps=std::clamp(frameRate,15.f,144.f);SoundEnabled=SoundActive=SoundReady=true;
    if(!bLocked)FX_Apply_Mode(false,-1);RenderFX();Process_Map(VS1,VS2);GetWaveData();RenderDots(VS2);RenderWave(VS2);std::swap(VS1,VS2);*width=FXW;*height=FXH;return VS1;
}
void GeissKey(int key){
    switch(key){case 'l':case 'L':bLocked=!bLocked;break;case ' ':case 'n':case 'N':FX_Pick_Random_Mode();break;case 'w':case 'W':waveform=(waveform+1)%(NUM_WAVES+1);break;
    case 'i':case 'I':g_bSlideShift=!g_bSlideShift;if(!g_bSlideShift)slider1=0;break;
    case 'q':case 'Q':effect[CHASERS]*=-1;break;case 'u':case 'U':effect[BAR]*=-1;break;case 'a':case 'A':effect[NUCLIDE]*=-1;break;case 'e':case 'E':effect[SHADE]*=-1;break;case 'y':case 'Y':effect[SOLAR]*=-1;break;case 'g':case 'G':effect[GRID]*=-1;break;case 'd':case 'D':effect[DOTS]*=-1;break;
    }
}
int GeissKeyState(){return bLocked?1:0;}
