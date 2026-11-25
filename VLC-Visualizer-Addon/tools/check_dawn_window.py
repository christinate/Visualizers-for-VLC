"""Exercise only the suite's native test window; no desktop input is injected."""
from pathlib import Path
import argparse
import json
import os
import platform
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def control_directory():
    if platform.system()=='Windows':return Path(os.environ['LOCALAPPDATA'])/'VLCVisualizers/control'
    if platform.system()=='Darwin':return Path.home()/'Library/Application Support/VLCVisualizers/control'
    runtime=os.environ.get('XDG_RUNTIME_DIR')
    return Path(runtime)/'vlc-visualizers/control' if runtime else Path.home()/'.cache/vlc-visualizers/control'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--shader',type=Path,default=ROOT.parent/'Terrain64/assets/terrain/terrain.wgsl')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/dawn-window-check')
    parser.add_argument('--background',action='store_true')
    parser.add_argument('--software',action='store_true')
    parser.add_argument('--plugin',type=Path)
    parser.add_argument('--suite',type=Path,help='Use the staged production app and its registered Terrain module')
    args=parser.parse_args()
    cases=[('resize-fullscreen-escape','escape',['--exercise']),
           ('fullscreen-escape','escape',['--startup-fullscreen']),
           ('fullscreen-rightclick','rightclick',['--startup-fullscreen']),
           ('fullscreen-menu','menu',['--startup-fullscreen']),
           ('device-loss','device-loss',[])]
    reports=[];args.output_dir.mkdir(parents=True,exist_ok=True)
    for name,closure,extra in cases:
        folder=(args.output_dir/name).resolve()
        folder.mkdir(parents=True,exist_ok=True)
        previous=folder/'result.json'
        if previous.exists():previous.unlink()
        command=[str(args.exe.resolve()),'--terrain-shader',str(args.shader.resolve()),'--output-dir',str(folder),
                 '--self-test','--time-scale','180','--seconds','20','--close-test',closure,*extra]
        if args.suite:
            command=[str(args.exe.resolve()),'--suite',str(args.suite.resolve()),'--visualizer','terrain','--demo',
                     '--output-dir',str(folder),'--profile',str(folder/'profile'),
                     '--self-test','--time-scale','180','--seconds','20','--close-test',closure,*extra]
        if args.background:command.append('--background')
        if args.plugin and not args.suite:command.extend(['--plugin',str(args.plugin.resolve())])
        environment=os.environ.copy()
        if args.software:
            command.extend(['--render-width','192','--render-height','128'])
            if platform.system()=='Linux':
                driver=Path('/usr/share/vulkan/icd.d/lvp_icd.json')
                if not driver.is_file():raise RuntimeError('This software test requires the installed lavapipe Vulkan driver')
                environment['VK_DRIVER_FILES']=str(driver)
            else:command.append('--software')
        child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=environment)
        try:stdout,stderr=child.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            child.kill();child.communicate();raise RuntimeError('Native window test timed out: '+name)
        folder.mkdir(parents=True,exist_ok=True)
        (folder/'stdout.log').write_text(stdout,encoding='utf-8');(folder/'stderr.log').write_text(stderr,encoding='utf-8')
        result_file=folder/'result.json'
        if not result_file.is_file():raise RuntimeError(f'Native test {name} exited {child.returncode} without a report: {stderr[-4096:]}')
        report=json.loads(result_file.read_text(encoding='utf-8'))
        if closure=='device-loss':
            assert child.returncode==1 and 'device' in report.get('error','').lower(),report
            assert 'Native lifecycle test requested device loss' in report.get('error',''),report
        else:
            assert child.returncode==0 and report['result']=='pass',report
            assert report['closed_by']==closure and report['maximum_frames_in_flight']<=2,report
            assert report['camera_input_mask']==15 and report['native_plugin_api']==4,report
            if args.plugin:assert report['native_plugin_api']==4,report
            if extra==['--exercise']:assert report['restored_bounds'] and report['surface_configurations']>=3,report
        assert not list(control_directory().glob(str(child.pid)+'-*.json')),'Window registration was not released'
        assert not list(control_directory().glob(str(child.pid)+'-*.json.close')),'Window close request was not released'
        reports.append({'case':name,'result':'pass','exit_code':child.returncode,'observed':report})
        print(name+': pass',flush=True)
    if args.plugin:
        invalid_assets=(args.output_dir/'invalid-assets').resolve();invalid_assets.mkdir(exist_ok=True)
        invalid_shader=invalid_assets/'terrain.wgsl'
        invalid_shader.write_text('@vertex fn vertex()->@builtin(position) vec4f { return vec4f(unknown_lifetime_fixture); }\n',encoding='utf-8')
        failures=[('missing-plugin',['--plugin',str((args.output_dir/'missing-native-module').resolve())],'canonical'),
                  ('failed-shader',['--plugin',str(args.plugin.resolve()),'--terrain-shader',str(invalid_shader)],'unknown_lifetime_fixture'),
                  ('missing-recording-input',['--plugin',str(args.plugin.resolve()),'--line-in','missing-native-input-cleanup-fixture'],'Recording device is missing')]
        for name,extra,expected in failures:
            folder=(args.output_dir/name).resolve();folder.mkdir(exist_ok=True)
            result_file=folder/'result.json'
            if result_file.exists():result_file.unlink()
            command=[str(args.exe.resolve()),'--self-test','--terrain-shader',str(args.shader.resolve()),'--output-dir',str(folder),*extra]
            environment=os.environ.copy()
            if args.software:
                command.extend(['--render-width','192','--render-height','128'])
                if platform.system()=='Linux':environment['VK_DRIVER_FILES']='/usr/share/vulkan/icd.d/lvp_icd.json'
                else:command.append('--software')
            child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',errors='replace',env=environment)
            try:stdout,stderr=child.communicate(timeout=30)
            except subprocess.TimeoutExpired:
                child.kill();child.communicate();raise RuntimeError('Native initialization failure timed out: '+name)
            (folder/'stdout.log').write_text(stdout,encoding='utf-8');(folder/'stderr.log').write_text(stderr,encoding='utf-8')
            report=json.loads(result_file.read_text(encoding='utf-8'))
            assert child.returncode==1 and report['result']=='fail',report
            assert expected in report['error'],report
            assert not list(control_directory().glob(str(child.pid)+'-*.json')),'Failed initialization retained its control registration'
            assert not list(control_directory().glob(str(child.pid)+'-*.json.close')),'Failed initialization retained a close request'
            reports.append({'case':name,'result':'pass','exit_code':child.returncode,'observed':report})
            print(name+': pass',flush=True)
    (args.output_dir/'summary.json').write_text(json.dumps({'result':'pass','cases':reports},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
