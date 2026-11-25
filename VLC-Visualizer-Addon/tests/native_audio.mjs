// Compare the native analyzer with the preserved JS implementation, using the
// exact same float32 PCM bytes. This test does not open recording devices.
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {MusicAnalysis} from '../browser/ui/audio.mjs';

const [probe,folder]=process.argv.slice(2);
assert(probe&&folder,'Usage: node tests/native_audio.mjs <analysis_probe> <output-directory>');
mkdirSync(folder,{recursive:true});
const inputs=[],expected=[];let reference=new MusicAnalysis();
function frame(samples,time,{reset=false,full=false}={}){
  if(reset)reference=new MusicAnalysis();
  const header=Buffer.alloc(12);header.writeDoubleLE(time);header.writeUInt32LE((reset?1:0)|(full?2:0),8);
  inputs.push(header,Buffer.from(samples.buffer,samples.byteOffset,samples.byteLength));
  const sanitized=Float32Array.from(samples,value=>Number.isFinite(value)?value:0);
  const result=reference.update(sanitized,time);
  expected.push({...result,wave:full?Array.from(result.wave):undefined,spectrum:full?Array.from(result.spectrum):undefined});
}
function tone(frequency,amplitude,phase=0){
  const samples=new Float32Array(4096);
  for(let n=0;n<2048;++n)samples[n*2]=samples[n*2+1]=Math.sin(n*2*Math.PI*frequency/44100+phase)*amplitude;
  return samples;
}
frame(new Float32Array(4096),0,{reset:true,full:true});
for(const frequency of [55,110,440,1000,10000,20000])frame(tone(frequency,.5),1,{reset:true,full:true});
frame(tone(440,2),1,{reset:true,full:true}); // retain finite samples above full scale
const opposite=tone(440,.5);for(let n=0;n<2048;++n)opposite[n*2+1]=-opposite[n*2];frame(opposite,1,{reset:true,full:true});
const invalid=tone(440,.5);invalid[0]=NaN;invalid[1]=Infinity;invalid[77]=-Infinity;frame(invalid,1,{reset:true,full:true});
const tiny=tone(440,1e-10);frame(tiny,1,{reset:true,full:true});
let random=0x12345678;
const noise=Float32Array.from({length:4096},()=>{random=(Math.imul(random,1664525)+1013904223)>>>0;return (random/4294967296-.5)*1.8;});
frame(noise,1,{reset:true,full:true});
for(let n=0;n<600;++n){
  const time=n/30,samples=new Float32Array(4096);
  for(let i=0;i<2048;++i){const t=time+i/44100,amplitude=.02+.7*Math.exp(-(t%.5)*20);samples[i*2]=samples[i*2+1]=Math.sin(t*2*Math.PI*110)*amplitude;}
  frame(samples,time,{reset:n===0,full:[0,1,299,599].includes(n)});
}
const header=Buffer.alloc(12);header.write('VISDSP01');header.writeUInt32LE(expected.length,8);
const fixture=resolve(folder,'analysis-input.bin'),report=resolve(folder,'analysis-result.jsonl');
writeFileSync(fixture,Buffer.concat([header,...inputs]));
writeFileSync(resolve(folder,'analysis-reference.jsonl'),expected.map(value=>JSON.stringify(value)).join('\n')+'\n');
execFileSync(resolve(probe),[fixture,report],{stdio:'inherit'});
const actual=readFileSync(report,'utf8').trim().split('\n').map(JSON.parse);
assert.equal(actual.length,expected.length);
let maximumError=0;
function compare(value,wanted,label){
  assert(Number.isFinite(value),label+' is nonfinite');
  const error=Math.abs(value-wanted);maximumError=Math.max(maximumError,error);
  assert(error<=1e-7+Math.abs(wanted)*2e-5,`${label}: ${value} != ${wanted}`);
}
for(let n=0;n<actual.length;++n){
  for(const key of ['rms','visual_energy','bass','beatEnvelope'])compare(actual[n][key],expected[n][key],`${n}.${key}`);
  for(const key of ['beat','beatCount'])assert.equal(actual[n][key],expected[n][key],`${n}.${key}`);
  for(const key of ['wave','spectrum'])if(expected[n][key]){
    assert.equal(actual[n][key].length,expected[n][key].length);
    actual[n][key].forEach((value,index)=>compare(value,expected[n][key][index],`${n}.${key}[${index}]`));
  }
}
const summary={result:'pass',frames:actual.length,full_spectrum_windows:expected.filter(value=>value.spectrum).length,
  beats:actual.at(-1).beatCount,maximum_absolute_error:maximumError,reference:'browser/ui/audio.mjs',nonfinite_pcm:'zeroed per input channel'};
writeFileSync(resolve(folder,'comparison.json'),JSON.stringify(summary,null,2)+'\n');console.log(JSON.stringify(summary));
