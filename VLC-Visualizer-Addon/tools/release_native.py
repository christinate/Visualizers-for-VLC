"""Prepare immutable native Linux suite and independent standalone archives."""
from pathlib import Path
import argparse
import json
import shutil
import sys
from stage_native import arguments,stage,ROOT
sys.path.insert(0,str(ROOT/'sdk'))
from release_guard import fresh_directory,checksums,candidate_version
from unix_packaging import record,copy_plan,write_package,archive_package,audit_linux


def main():
    parser=argparse.ArgumentParser(description=__doc__);arguments(parser)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise FileExistsError('Use a fresh candidate output directory')
    suite,components,audit=stage(args);version=candidate_version(ROOT)
    output=fresh_directory(args.output)
    (output/'dependency-audit.json').write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    variants=[('VLC-Visualizer-Addon','addon','suite',tuple(components),'vlc-visualizers'),
              ('PrismFlow64','geiss','renderer',('runtime','geiss'),'prismflow64'),
              ('MilkDrop2-64','milkdrop','renderer',('runtime','milkdrop'),'milkdrop2-64'),
              ('Terrain64','terrain','renderer',('runtime','terrain'),'terrain64'),
              ('Beach','beach','renderer',('runtime','beach'),'beach'),
              ('Objects','objects','renderer',('runtime','objects'),'objects'),
              ('UnderSea','undersea','renderer',('runtime','undersea'),'undersea'),
              ('CoralCanyon','coralcanyon','renderer',('runtime','coralcanyon'),'coralcanyon')]
    for name,owner,kind,selected,folder in variants:
        package=fresh_directory(output/('stage-'+name))
        rows={}
        for component in selected:
            plan={relative:suite/relative for relative in components[component]};copy_plan(plan,package)
            rows[component]=[record(package/relative,package) for relative in sorted(plan)]
        default='${XDG_DATA_HOME:-${HOME}/.local/share}/'+folder
        write_package(package,version,owner,kind,rows,default)
        package_audit=audit_linux(package)
        (package/'dependency-audit.json').write_text(json.dumps(package_audit,indent=2)+'\n',encoding='utf-8')
        archive=archive_package(package,output,name,version)
        # The authoritative suite and archives remain. Avoid four redundant
        # expanded package copies on machines with little staging space.
        for path in sorted(package.rglob('*'),key=lambda p:len(p.parts),reverse=True):
            if path.is_symlink() or path.is_file():path.unlink()
            elif path.is_dir():path.rmdir()
        package.rmdir()
        print(archive,flush=True)
    checksums(output)


if __name__=='__main__':main()
