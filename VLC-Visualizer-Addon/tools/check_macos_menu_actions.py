"""Owner-local actual AppKit menu actions and VLC RC playback; generated media."""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import math
import os
import platform
import re
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import wave
from check_dawn_player import records,close_owned,ipc_file


def until(callback,timeout=20):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        value=callback()
        if value:return value
        time.sleep(.05)
    raise RuntimeError('Timed out waiting for '+repr(callback))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-suite',type=Path,required=True)
    parser.add_argument('--menu-module',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--audio-fixtures',type=Path,help='Optional hash-verified generated media-format matrix')
    args=parser.parse_args()
    if platform.system()!='Darwin':raise RuntimeError('Requires macOS and the build-gated AppKit test module')
    source=args.source_suite.resolve();output=args.output.resolve();output.mkdir(parents=True)
    if source.is_relative_to(output) or output.is_relative_to(source):raise RuntimeError('Separate source and fresh output paths')
    assert b'menu-control-command.json' in args.menu_module.read_bytes(),'Use a test-control build'
    suite=output/'suite';shutil.copytree(source,suite,symlinks=True)
    plugin=suite/'vlc-plugins/libvlc_visualizers_plugin.dylib';shutil.copy2(args.menu_module,plugin)
    subprocess.run(['codesign','--force','--sign','-',str(plugin)],check=True,capture_output=True)
    subprocess.run([str(suite/'VisualizersCtl'),'scan','--suite',str(suite)],check=True,capture_output=True)
    profile=suite/'menu-test-profile';profile.mkdir()
    for identifier in ('geiss','milkdrop','terrain'):
        (profile/(identifier+'.json')).write_text(json.dumps(dict(width=320,height=240,fps=30,hdr=False,engine='modern',ownerCustom='preserve')))
    audio=output/'generated-stereo.wav'
    with wave.open(str(audio),'wb') as stream:
        stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(48000)
        data=bytearray()
        for n in range(48000):
            for hz in (220,330):data+=struct.pack('<h',int(12000*math.sin(2*math.pi*hz*n/48000)))
        stream.writeframes(data*12)
    child=None;connection=None;log=None;sequence=0;rows=[];transcript=[]
    socket_root=Path(tempfile.mkdtemp(prefix='vv-menu-',dir='/tmp'));rc_socket=socket_root/'rc'
    result=dict(result='running',checks=rows,scope='Actual NSMenu dispatch and held-action NSApplication delivery inside real VLC; private RC playback of generated media. No manual pointer or microphone claim.',
                menu_module_sha256=hashlib.sha256(plugin.read_bytes()).hexdigest())
    result['rc_transcript']=transcript
    def mark(case,**details):
        rows.append(dict(case=case,result='pass',**details));print(case+': pass',flush=True)
    def rc(command):
        connection.sendall((command+'\n').encode());reply=bytearray()
        while len(reply)<65536:
            try:
                block=connection.recv(4096)
                if not block:break
                reply+=block
            except socket.timeout:break
        assert len(reply)<65536,'Unexpected unbounded RC response'
        text=reply.decode('utf-8',errors='replace');transcript.append(dict(command=command,reply=text));return text
    def playback_state(kind,number):
        def observed():
            reply=rc('status');states=re.findall(r'\(\s*(play|pause|stop) state:\s*(\d+)\s*\)',reply)
            return dict(kind=kind,number=number,reply=reply) if states and states[-1]==(kind,str(number)) else None
        return until(observed,10)
    def start(label):
        nonlocal child,connection,log
        assert child is None or child.poll() is not None
        for name in ('menu-test.json','menu-control-command.json','menu-control-response.json'):
            (suite/name).unlink(missing_ok=True)
        rc_socket.unlink(missing_ok=True)
        env=dict(os.environ,VLC_VISUALIZERS_SELF_TEST='1',VLC_VISUALIZERS_MENU_CONTROL_TEST='1',VLC_VISUALIZERS_TEST_SUITE=str(suite))
        log=(output/(label+'.vlc.log')).open('w')
        # The generic rc shortcut selects Lua's stdin console on VLC 3.0.24
        # for Mac. Select the installed native module that owns --rc-unix.
        child=subprocess.Popen([str(suite/'VlcVisualizerLauncher'),'--ignore-config','--aout=dummy','--loop','--extraintf=oldrc','--rc-fake-tty','--rc-unix',str(rc_socket),str(audio)],env=env,stdin=subprocess.PIPE,stdout=log,stderr=subprocess.STDOUT)
        def ready():
            if child.poll() is not None:raise RuntimeError('VLC exited before menu/socket readiness: '+str(child.returncode))
            return (suite/'menu-test.json').is_file() and rc_socket.exists()
        until(ready)
        connection=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);connection.settimeout(.2);connection.connect(str(rc_socket))
        menu=json.loads((suite/'menu-test.json').read_text());order=menu['menu_order']
        assert menu['entries']==3 and order.count('Visualizers')==1 and order.index('Visualizers')==order.index('View')+1 and order.index('Visualizers')<order.index('Help'),menu
        return menu
    def command(action,identifier=None):
        nonlocal sequence
        assert child.poll() is None,'VLC exited before menu action'
        sequence+=1;value=dict(pid=child.pid,sequence=sequence,action=action)
        if identifier:value['id']=identifier
        tmp=suite/'menu-control-command.tmp';tmp.write_text(json.dumps(value));tmp.replace(suite/'menu-control-command.json')
        def response():
            file=suite/'menu-control-response.json'
            if not file.is_file():return None
            data=json.loads(file.read_text())
            return data if data.get('pid')==child.pid and data.get('sequence')==sequence else None
        observed=until(response);assert observed['result']=='pass',observed
        return observed
    def open_renderer(identifier):
        observed=command('open',identifier);assert not observed['notifications'] and len(observed['launches'])==1,observed
        until(lambda:[value for _,value in records(suite,child.pid) if value['id']==identifier])
        # Registration precedes cold Metal pipeline creation. The first run
        # reached 18 frames when closed after two seconds; retain >=30 actual
        # frames and give initialization a separate bounded warm-up interval.
        time.sleep(4)
        return sequence
    def finished(identifier,seq,original=False,reason='menu'):
        until(lambda:not [value for _,value in records(suite,child.pid) if value['id']==identifier])
        if original:
            file=profile/'last-run.log'
            until(lambda:file.exists() and 'exit=' in file.read_text())
            text=file.read_text();assert 'exit=0' in text and float(text.split('audio_peak=')[1].split()[0])>.1,text
            return dict(log=text)
        file=suite/'menu-actions'/str(seq)/'result.json';until(file.is_file)
        data=json.loads(file.read_text());assert data['result']=='pass' and data['closed_by']==reason and data['frames']>=30,data
        assert data['player_pcm_windows']>0 and data['player_pcm_peak']>.1 and not data['recording_started'] and data['input_picker_opens']==0,data
        return data
    def header():
        path=ipc_file(child.pid)
        def read():
            if not path.exists():return None
            with path.open('rb') as stream:
                try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
                except BlockingIOError:return None
                values=struct.unpack('<IIIIQQ',stream.read(32));fcntl.flock(stream,fcntl.LOCK_UN)
            assert values[0]==0x31565A56 and values[1]==1 and values[3]==2,values
            return dict(rate=values[2],written=values[4],updated_ms=values[5])
        return until(read)
    try:
        menu=start('initial');view=command('inspect');assert len(view['menu'])==3
        for item in view['menu']:
            assert [v['title'] for v in item['actions']]==['Open','Settings…','Close'],item
            assert all(v['enabled'] and v['target_present'] for v in item['actions']),item
        mark('inline menu and three wired action groups',menu=menu)
        for name,identifier,original in [('terrain','terrain',False),('prismflow','geiss',False),('projectm','milkdrop',True),('geiss-classic','geiss',True)]:
            if name=='geiss-classic':
                f=profile/'geiss.json';p=json.loads(f.read_text());p['engine']='classic';f.write_text(json.dumps(p))
            seq=open_renderer(identifier);closed=command('close',identifier);assert not closed['notifications']
            observed=finished(identifier,seq,original);mark(name+' real menu Open/Close with PCM',observed=observed)
            if name=='geiss-classic':
                p['engine']='modern';f.write_text(json.dumps(p))
        for identifier in ('terrain','geiss','milkdrop'):
            # The original frontend's Settings dispatcher forwards profile
            # and self-test, selecting the production host's default report.
            report=profile/('native-'+identifier)/'result.json';report.unlink(missing_ok=True)
            observed=command('settings',identifier);assert not observed['notifications'] and len(observed['launches'])==1,observed
            until(report.is_file);settings=json.loads(report.read_text())
            assert settings['result']=='pass' and settings['native_settings'] and settings['settings_saved'],settings
            saved=json.loads((profile/(identifier+'.json')).read_text());assert saved['ownerCustom']=='preserve' and saved['width']==320 and saved['fps']==30,saved
            mark(identifier+' actual menu Settings',observed=settings)
        terrain=open_renderer('terrain');geiss=open_renderer('geiss');closed=command('close-all');assert not closed['notifications']
        mark('actual menu Close all',terrain=finished('terrain',terrain),geiss=finished('geiss',geiss))
        seq=open_renderer('terrain');first=header();rc('pause');paused_state=playback_state('pause',4)
        time.sleep(1);paused=header();time.sleep(.4);assert header()==paused and records(suite,child.pid),paused
        rc('pause');until(lambda:header()['written']>paused['written']);rc('seek 2');time.sleep(.2);seek=rc('get_time');playback_state('play',3)
        rc('stop');stopped_state=playback_state('stop',5);time.sleep(.5);stopped=header();time.sleep(.4);assert header()==stopped and records(suite,child.pid)
        rc('play');until(lambda:header()['written']>stopped['written']);command('close','terrain')
        mark('pause seek stop resume retains live renderer and PCM transport',before=first,paused=paused,stopped=stopped,paused_state=paused_state,stopped_state=stopped_state,seek_reply=seek,observed=finished('terrain',seq))
        package=suite/'plugins/terrain';manifest_file=package/'manifest.json';original=manifest_file.read_bytes();manifest=json.loads(original);module=package/manifest['entrypoint'];saved_module=module.with_suffix('.held')
        changes=[('malformed JSON',b'{'),('invalid UTF-8 JSON',b'{"name":"\xff"}'),('NUL entrypoint',json.dumps(dict(manifest,entrypoint=manifest['entrypoint']+'\0invalid')).encode()),
                 ('traversing entrypoint',json.dumps(dict(manifest,entrypoint='../'+manifest['entrypoint'])).encode()),
                 ('wrong API',json.dumps(dict(manifest,api_version=99)).encode())]
        for name,content in changes:
            command('inspect');manifest_file.write_bytes(content)
            try:
                observations=[]
                for action in ('invoke-open','invoke-settings'):
                    rejected=command(action,'terrain');assert rejected['notifications'] and not rejected['launches'],rejected;observations.append(rejected)
                disabled=command('inspect');row=next(v for v in disabled['menu'] if v['id']=='terrain');assert not row['actions'][0]['enabled'] and not row['actions'][1]['enabled']
                mark('held menu Open/Settings rejects '+name,observed=observations)
            finally:manifest_file.write_bytes(original)
        for name,path in [('assets symlink escape',package/manifest['assets']),('package symlink escape',package)]:
            command('inspect');outside=output/('held-'+path.name);path.rename(outside);path.symlink_to(outside,target_is_directory=True)
            try:
                observations=[]
                for action in ('invoke-open','invoke-settings'):
                    rejected=command(action,'terrain');assert rejected['notifications'] and not rejected['launches'],rejected;observations.append(rejected)
                disabled=command('inspect');row=next(v for v in disabled['menu'] if v['id']=='terrain');assert not row['actions'][0]['enabled'] and not row['actions'][1]['enabled']
                mark('held menu Open/Settings rejects '+name,observed=observations)
            finally:path.unlink();outside.rename(path)
        command('inspect');module.rename(saved_module)
        try:
            for action in ('invoke-open','invoke-settings'):
                rejected=command(action,'terrain');assert rejected['notifications'] and not rejected['launches'],rejected
            command('rescan');scan=json.loads((suite/'installed-visualizers.json').read_text());assert {p['id'] for p in scan['visualizers']}=={'geiss','milkdrop'}
            assert len(command('inspect')['menu'])==2
            mark('removed module rejected and removed by actual Rescan action')
        finally:saved_module.rename(module)
        command('rescan');assert len(command('inspect')['menu'])==3
        cache=(suite/'installed-visualizers.json').read_bytes();rc('quit');assert child.wait(timeout=15)==0;child.stdin.close();connection.close();connection=None;log.close();log=None
        start('persistent-restart');assert (suite/'installed-visualizers.json').read_bytes()==cache;assert len(command('inspect')['menu'])==3
        mark('three entries persist across VLC restart without a scan')
        if args.audio_fixtures:
            fixture_root=args.audio_fixtures.resolve();fixtures=json.loads((fixture_root/'fixtures.json').read_text())
            assert fixtures['source'].startswith('synthetic tones') and len(fixtures['cases'])==10
            for fixture in fixtures['cases']:
                media=fixture_root/fixture['file'];assert media.parent==fixture_root and hashlib.sha256(media.read_bytes()).hexdigest()==fixture['sha256']
                rc('clear');rc('add '+media.as_uri());rc('play');playback_state('play',3)
                for name,identifier,original in [('terrain','terrain',False),('prismflow','geiss',False),('projectm','milkdrop',True),('geiss-classic','geiss',True)]:
                    file=profile/'geiss.json';preferences=json.loads(file.read_text());preferences['engine']='classic' if name=='geiss-classic' else 'modern';file.write_text(json.dumps(preferences))
                    seq=open_renderer(identifier);transport=header();command('close',identifier)
                    observed=finished(identifier,seq,original)
                    mark('media '+fixture['id']+' / '+name,fixture_sha256=fixture['sha256'],source_stream=fixture['probe']['streams'][0],transport=transport,observed=observed)
            preferences['engine']='modern';file.write_text(json.dumps(preferences))
        seq=open_renderer('terrain');owner=child.pid;rc('quit');assert child.wait(timeout=15)==0
        mark('VLC shutdown closes owned renderer',observed=finished('terrain',seq,reason='player-exit'))
        assert not records(suite,owner) and not ipc_file(owner).exists()
        result['result']='pass'
    except Exception as error:
        result.update(result='fail',error=repr(error));raise
    finally:
        if child:
            close_owned(suite,child.pid)
            if child.poll() is None and connection:
                try:rc('quit')
                except OSError:pass
            if child.poll() is None:
                try:child.wait(timeout=10)
                except subprocess.TimeoutExpired:child.terminate();child.wait(timeout=5)
        if connection:connection.close()
        if child and child.stdin:child.stdin.close()
        if log:log.close()
        assert socket_root.resolve().parent==Path('/tmp').resolve()
        shutil.rmtree(socket_root)
        (output/'result.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
