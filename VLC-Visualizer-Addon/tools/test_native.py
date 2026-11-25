"""Exercise native GUI/audio paths using original VLC and test-local modules."""
from pathlib import Path
import argparse,json,os,subprocess,sys,time,struct,math
parser=argparse.ArgumentParser();parser.add_argument('--suite',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--vlc',type=Path,required=True);parser.add_argument('--matrix',action='store_true');args=parser.parse_args()
suite=args.suite.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
cases=[(44100,2,16,False),(48000,2,24,False)]
if args.matrix:cases=[(8000,1,16,False),(44100,2,16,False),(48000,2,24,False),(96000,2,32,False),(192000,2,24,False),(48000,6,16,False),(48000,8,16,False),(48000,2,32,True)]
results=[]
for index,(rate,channels,bits,floating) in enumerate(cases):
    id='geiss' if index%2==0 else 'milkdrop';audio=output/f'{index}-{rate}-{channels}-{bits}.wav';size=bits//8
    # Repeat one second of multi-channel PCM for an eight-second playback.
    samples=bytearray()
    for n in range(rate):
        for ch in range(channels):
            value=.4*math.sin(2*math.pi*(220+ch*55)*n/rate)
            if floating:samples+=struct.pack('<f',value)
            else:samples+=int(value*((1<<(bits-1))-1)).to_bytes(size,'little',signed=True)
    data=samples*8;fmt=struct.pack('<HHIIHH',3 if floating else 1,channels,rate,rate*channels*size,channels*size,bits)
    audio.write_bytes(b'RIFF'+struct.pack('<I',36+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(data))+data)
    for file in [suite/'menu-test.json',suite/'menu-test-profile/last-run.log']:
        if file.exists():file.unlink()
    env=dict(os.environ,VLC_PLUGIN_PATH=str(suite/'vlc-plugins'),VLC_VISUALIZERS_TEST_SUITE=str(suite),VLC_VISUALIZERS_SELF_TEST='1',VLC_VISUALIZERS_TEST_RENDERER=id)
    command=[str(args.vlc),'--ignore-config','--extraintf=visualizers_menu','--aout=dummy','--play-and-exit']
    if sys.platform!='darwin':command+=['--no-one-instance','--no-qt-privacy-ask']
    command.append(str(audio))
    with open(output/f'vlc-{index}.log','w') as stream:
        result=subprocess.run(command,env=env,stdout=stream,stderr=stream,timeout=30)
    assert result.returncode==0,('VLC exit',result.returncode)
    menu=json.loads((suite/'menu-test.json').read_text());order=menu['menu_order']
    assert menu['attached'] and menu['entries']==2,menu
    assert order.count('Visualizers')==1 and order.index('Visualizers')==order.index('View')+1 and order.index('Visualizers')<order.index('Help'),order
    deadline=time.monotonic()+15
    while True:
        log=(suite/'menu-test-profile/last-run.log').read_text() if (suite/'menu-test-profile/last-run.log').exists() else ''
        if 'exit=' in log or time.monotonic()>=deadline:break
        time.sleep(.1)
    assert 'exit=0' in log and 'round-trip=pass' in log and 'restore/borderless=pass' in log,log
    peak=float(log.split('audio_peak=')[1].split()[0]);assert peak>0,log
    row={'sample_rate':rate,'channels':channels,'bits':bits,'float':floating,'renderer':id,'vlc_exit':result.returncode,'menu':menu,'renderer_log':log};results.append(row);print({k:row[k] for k in ['sample_rate','channels','bits','float','renderer','vlc_exit']},flush=True)
(output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS: native menu order, fullscreen/keys, and decoded audio cases',len(results),flush=True)
