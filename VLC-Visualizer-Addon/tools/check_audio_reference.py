"""Compare native DSP with a JS-generated fixture on a machine without Node."""
from pathlib import Path
import argparse
import itertools
import json
import math
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--expected',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    actual=args.output/'analysis-result.jsonl'
    subprocess.run([str(args.probe.resolve()),str(args.input.resolve()),str(actual.resolve())],check=True)
    maximum=0;frames=0;full=0
    def compare(value,wanted,label):
        nonlocal maximum
        assert math.isfinite(value),label+' is nonfinite'
        error=abs(value-wanted);maximum=max(maximum,error)
        assert error<=1e-7+abs(wanted)*2e-5,(label,value,wanted)
    with actual.open() as a,args.expected.open() as b:
        for n,(found,expected) in enumerate(itertools.zip_longest(a,b)):
            assert found and expected,'Reference frame count differs'
            found=json.loads(found);expected=json.loads(expected)
            for key in ('rms','visual_energy','bass','beatEnvelope'):compare(found[key],expected[key],f'{n}.{key}')
            for key in ('beat','beatCount'):assert found[key]==expected[key],(n,key)
            for key in ('wave','spectrum'):
                if key in expected:
                    assert len(found[key])==len(expected[key]),(n,key)
                    for index,(value,wanted) in enumerate(zip(found[key],expected[key])):compare(value,wanted,f'{n}.{key}[{index}]')
            full+='spectrum' in expected;frames+=1
    summary={'result':'pass','frames':frames,'full_spectrum_windows':full,'maximum_absolute_error':maximum,
             'reference':'browser/ui/audio.mjs, pre-generated with exact float PCM bytes'}
    (args.output/'comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary))


if __name__=='__main__':main()
