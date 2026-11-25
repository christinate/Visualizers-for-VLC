"""Exercise native settings/INI migration and the next renderer launch; no recording input is opened."""
from pathlib import Path
import argparse
import json
import os
import platform
import subprocess
from check_dawn_input import owned_records, request_close

ROOT=Path(__file__).resolve().parents[1]


def run(command, folder, environment):
    folder.mkdir(parents=True, exist_ok=True)
    result=folder/'result.json'
    if result.exists():
        result.unlink()
    with (folder/'stdout.log').open('w',encoding='utf-8') as stdout,(folder/'stderr.log').open('w',encoding='utf-8') as stderr:
        child=subprocess.Popen(command,stdout=stdout,stderr=stderr,env=environment)
        try:
            code=child.wait(timeout=35)
        except subprocess.TimeoutExpired:
            request_close(folder)
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill();child.wait(timeout=5)
            raise RuntimeError('Native settings test timed out: '+str(folder))
    report=json.loads(result.read_text(encoding='utf-8'))
    assert code==0 and report['result']=='pass',report
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--ui-probe',type=Path,required=True)
    parser.add_argument('--plugin',type=Path,required=True)
    parser.add_argument('--shader',type=Path,default=ROOT.parent/'Terrain64/assets/terrain/terrain.wgsl')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/dawn-settings-check')
    parser.add_argument('--software',action='store_true')
    args=parser.parse_args();output=args.output_dir.resolve();output.mkdir(parents=True,exist_ok=True)
    probe=subprocess.run([str(args.ui_probe.resolve())],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=15)
    (output/'ui.stdout.log').write_text(probe.stdout,encoding='utf-8');(output/'ui.stderr.log').write_text(probe.stderr,encoding='utf-8')
    assert probe.returncode==0 and 'owner-close pass' in probe.stdout,probe.stderr
    profile=output/'profile';profile.mkdir(exist_ok=True)
    preferences=profile/'terrain.json'
    if preferences.exists():
        preferences.unlink()
    ini='[terrain]\nwidth=800\nheight=600\nfps=30\nfullscreen=1\nhdr=1\npeak=1200\n'
    (profile/'settings.ini').write_text(ini,encoding='utf-8',newline='\n')
    environment=os.environ.copy()
    editor=run([str(args.exe.resolve()),'--settings','terrain','--self-test','--profile',str(profile),'--output-dir',str(output/'editor')],output/'editor',environment)
    assert editor['settings_saved'] and editor['native_settings'],editor
    imported=json.loads(preferences.read_text(encoding='utf-8'))
    assert imported['width']==800 and imported['height']==600 and imported['fps']==30 and imported['fullscreen'] and imported['hdr'] and imported['peak']==1200,imported
    assert (profile/'settings.ini').read_text(encoding='utf-8')==ini,'Original INI changed'
    imported.update({'cameraSeconds':45.125,'colorBeats':8,'inputDevice':'retained-device','custom':{'preserve':[1,2,3]},'customBlob':'a'*70000})
    preferences.write_text(json.dumps(imported)+'\n',encoding='utf-8')
    folder=output/'renderer'
    arguments=[str(args.exe.resolve()),'--terrain-shader',str(args.shader.resolve()),'--plugin',str(args.plugin.resolve()),'--profile',str(profile),
               '--self-test','--exercise','--time-scale','180','--seconds','25','--output-dir',str(folder)]
    if args.software:
        arguments.extend(['--render-width','192','--render-height','128'])
        if platform.system()=='Linux':
            driver=Path('/usr/share/vulkan/icd.d/lvp_icd.json')
            if not driver.is_file():raise RuntimeError('Installed lavapipe ICD is required for this software test')
            environment['VK_DRIVER_FILES']=str(driver)
        else:
            arguments.append('--software')
    report=run(arguments,folder,environment)
    assert report['borderless'] and report['startup_fullscreen'] and report['startup_bounds_restored'] and report['restored_bounds'],report
    assert report['initial_windowed_bounds'][2:]==[800,600] and report['fullscreen_toggles']==3,report
    assert report['settings']['fps']==30 and report['settings']['cameraSeconds']==45.125 and report['settings']['colorBeats']==8,report
    assert report['settings']['hdr_requested'] and not report['settings']['hdr_active'],report
    assert not report['recording_started'] and report['closed_by']=='escape' and not owned_records(folder),report
    assert json.loads(preferences.read_text(encoding='utf-8'))==imported,'Renderer launch rewrote saved preferences'
    (output/'summary.json').write_text(json.dumps({'result':'pass','native_settings':editor,'renderer':report},indent=2)+'\n',encoding='utf-8')
    print('Native settings controls, INI/JSON migration, saved fullscreen/restore/size/FPS, Terrain options and SDR fallback: pass')


if __name__=='__main__':
    main()
