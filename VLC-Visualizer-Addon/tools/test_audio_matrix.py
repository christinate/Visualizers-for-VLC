from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import json
import math
import os
import re
import struct
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[1]
suite=ROOT/'build/menu-suite';vlc=Path(os.environ.get('VLC_INSTALL',r'C:\Program Files\VideoLAN\VLC'))
if os.environ.get('VIS_AUDIO_MATRIX_CHILD')!='1':
    env=dict(os.environ,VIS_AUDIO_MATRIX_CHILD='1',VLC_PLUGIN_PATH=str(ROOT/'build/menu-module'))
    raise SystemExit(subprocess.call([sys.executable,'-u',__file__],env=env))
os.add_dll_directory(str(vlc));lib=C.CDLL(str(vlc/'libvlc.dll'));core=C.CDLL(str(vlc/'libvlccore.dll'))
def api(name,result,args):
    fn=getattr(lib,name);fn.restype=result;fn.argtypes=args;return fn
new=api('libvlc_new',C.c_void_p,[C.c_int,C.POINTER(C.c_char_p)])
mediaNew=api('libvlc_media_new_path',C.c_void_p,[C.c_void_p,C.c_char_p]);mediaFree=api('libvlc_media_release',None,[C.c_void_p])
playerNew=api('libvlc_media_player_new_from_media',C.c_void_p,[C.c_void_p]);play=api('libvlc_media_player_play',C.c_int,[C.c_void_p]);stop=api('libvlc_media_player_stop',None,[C.c_void_p]);playerFree=api('libvlc_media_player_release',None,[C.c_void_p]);release=api('libvlc_release',None,[C.c_void_p]);getInput=api('libvlc_get_input_thread',C.c_void_p,[C.c_void_p])
class Value(C.Union):_fields_=[('integer',C.c_int64),('string',C.c_char_p)]
setvar=core.var_SetChecked;setvar.restype=C.c_int;setvar.argtypes=[C.c_void_p,C.c_char_p,C.c_int,Value]
control=core.input_Control;control.restype=C.c_int;control.argtypes=[C.c_void_p,C.c_int]
unref=core.vlc_object_release;unref.argtypes=[C.c_void_p]
header=(ROOT/'vendor/vlc/include/vlc_input.h').read_text();enum=header[header.index('enum input_query_e'):];enum=enum[enum.index('{')+1:enum.index('};')];enum=re.sub(r'/\*.*?\*/','',enum,flags=re.S);query=-1
for token in enum.split(','):
    token=token.strip()
    if not token:continue
    query=int(token.split('=')[1],0) if '=' in token else query+1
    if token.split('=')[0].strip()=='INPUT_GET_AOUT':break
kernel=C.WinDLL('kernel32',use_last_error=True);kernel.OpenFileMappingW.argtypes=[W.DWORD,W.BOOL,W.LPCWSTR];kernel.OpenFileMappingW.restype=W.HANDLE;kernel.MapViewOfFile.argtypes=[W.HANDLE,W.DWORD,W.DWORD,W.DWORD,C.c_size_t];kernel.MapViewOfFile.restype=C.c_void_p;kernel.UnmapViewOfFile.argtypes=[C.c_void_p];kernel.CloseHandle.argtypes=[W.HANDLE]
folder=ROOT/'build/audio-matrix';folder.mkdir(exist_ok=True)
cases=[(8000,1,16,False),(44100,2,16,False),(48000,2,24,False),(96000,2,32,False),(192000,2,24,False),(48000,6,16,False),(48000,8,16,False),(48000,2,32,True)]
results=[]
for index,(rate,channels,bits,floating) in enumerate(cases):
    samples=bytearray()
    for n in range(rate*4):
        for channel in range(channels):
            value=.45*math.sin(n*2*math.pi*(220+channel*31)/rate)
            samples.extend(struct.pack('<f',value) if floating else int(value*((1<<(bits-1))-1)).to_bytes(bits//8,'little',signed=True))
    fmt=struct.pack('<HHIIHH',3 if floating else 1,channels,rate,rate*channels*(bits//8),channels*(bits//8),bits)
    file=folder/f'{rate}-{channels}-{bits}-{int(floating)}.wav';file.write_bytes(b'RIFF'+struct.pack('<I',36+len(samples))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(samples))+samples)
    options=[b'--ignore-config',b'--no-plugins-cache',b'--no-media-library',b'--aout=dummy',b'--no-video',b'--quiet'];argv=(C.c_char_p*len(options))(*options);instance=new(len(options),argv);assert instance
    media=mediaNew(instance,str(file).encode());player=playerNew(media);mapping=view=None
    try:
        assert play(player)==0
        value=Value();value.string=b'vlc_visualizers'
        for attempt in range(100):
            thread=getInput(player);output=C.c_void_p()
            if thread:
                status=control(thread,query,C.byref(output));unref(thread)
                if status==0 and output.value:
                    assert setvar(output,b'audio-filter',0x40,value)==0;unref(output);break
            time.sleep(.02)
        for attempt in range(100):
            mapping=kernel.OpenFileMappingW(4,False,'Local\\VLCVisualizers.Audio.'+str(os.getpid()))
            if mapping:break
            time.sleep(.02)
        assert mapping
        view=kernel.MapViewOfFile(mapping,4,0,0,0);assert view;time.sleep(.1)
        magic,version,publishedRate,publishedChannels,written,updated=struct.unpack('<IIIIQQ',C.string_at(view,32));assert magic==0x31565A56 and publishedRate==rate and publishedChannels==2 and written>0
        renderer='geiss' if index%2==0 else 'milkdrop';profile=folder/('profile-'+str(index))
        proc=subprocess.run([str(suite/'Visualizers.exe'),'--visualizer',renderer,'--vlc-pid',str(os.getpid()),'--self-test','--background','--profile',str(profile),'--quit-after','1000'],timeout=12)
        log=(profile/'last-run.log').read_text();assert proc.returncode==0 and 'audio_peak=0.000000' not in log,log
        results.append({'sample_rate':rate,'channels':channels,'bits':bits,'float':floating,'renderer':renderer,'result':'pass'})
        print(results[-1],flush=True)
    finally:
        if view:kernel.UnmapViewOfFile(view)
        if mapping:kernel.CloseHandle(mapping)
        stop(player);playerFree(player);mediaFree(media);release(instance)
(ROOT/'build/audio-matrix-result.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS: all 8 decoded-audio format cases reached a renderer')
