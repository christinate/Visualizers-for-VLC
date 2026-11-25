"""Extract the existing BSD Geiss CPU engine, excluding Win32 UI/device code.

The checked-in output retains the original map, effect, and waveform code.
Windows LONG values are explicitly 32-bit on LP64 platforms as well.
"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'native';OUT.mkdir(exist_ok=True)
main=(ROOT/'source/main.cpp').read_text(errors='replace')

def function(source,name):
    match=re.search(r'^([\w:*<>]+(?:[ \t]+[\w:*<>]+)*[ \t]+)?'+re.escape(name)+r'\s*\([^;]*?\)\s*(?://[^\n]*\n)?\s*\{',source,re.M)
    if not match:raise ValueError(name)
    start=match.start();pos=source.index('{',match.start());depth=0;state='code';quote=''
    while pos<len(source):
        c=source[pos];pair=source[pos:pos+2]
        if state=='line':
            if c=='\n':state='code'
        elif state=='block':
            if pair=='*/':state='code';pos+=1
        elif state=='string':
            if c=='\\':pos+=1
            elif c==quote:state='code'
        elif pair=='//':state='line';pos+=1
        elif pair=='/*':state='block';pos+=1
        elif c in '\"\'':state='string';quote=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if not depth:return source[start:pos+1]+'\n'
        pos+=1
    raise ValueError('Unclosed function '+name)

globals=main[main.index('    char g_song_title[256];'):main.index('#include "effects.h"')]
mode_data=main[main.index('bool mode_motion_dampened[]'):main.index('BOOL DlgItemIsChecked')]
# Only the CPU class/data block belongs here, before UI helpers begin.
mode_data=mode_data[:mode_data.index('CModeInfo modeInfo[NUM_MODES+1];')+len('CModeInfo modeInfo[NUM_MODES+1];')]
effects=(ROOT/'source/Effects.h').read_text(errors='replace')
video=(ROOT/'source/video.h').read_text(errors='replace')
system=(ROOT/'source/Sysstuff.h').read_text(errors='replace')
code=main[:main.index('//\n//  GEISS 4.25')]
code+='\n// Native extraction: Win32 UI/DirectDraw excluded; LONG storage is fixed-width.\n#include "engine_platform.hpp"\n'
code+='int g_nCores=1;\n'+function(main,'AdjustRateToFPS')
code+=globals+'\n'+mode_data+'\n'
code+='void FX_Random_Palette(bool); void PutPalette(); float CrankPal(unsigned int,int);\n'
code+='\n'+effects+'\n'
for name in ['CrankPal','FX_Random_Palette','PutPalette']:code+=function(video,name)+'\n'
code+=function(system,'OnFxwFxhUpdated')+'\n'
for name in ['FX_Init','FX_Fini','FX_Pick_Random_Mode','FX_Apply_Mode','RenderFX','GetWaveData','RenderDots','RenderWave']:code+=function(main,name)+'\n'
code=code.replace('#include "proc_map.h"','')
code=re.sub(r'\bunsigned\s+long\b','uint32_t',code)
code=re.sub(r'\blong\b','int32_t',code)
code=code.replace('__int16','int16_t').replace('__cdecl','').replace('__fastcall','')
code=re.sub(r'\bgamma\b','geiss_gamma',code)
map_code=(ROOT/'ports/geiss_map.cpp').read_text().replace('#include "Proc_map.h"','')
code+='\n'+map_code+'\n#include "engine_adapter.inc"\n'
(OUT/'engine.cpp').write_text(code,encoding='utf-8')
print('Extracted native CPU engine:',len(code.splitlines()),'lines')
