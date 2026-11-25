import assert from 'node:assert/strict';
import {MusicAnalysis} from '../browser/ui/audio.mjs';
import {half} from '../../Terrain64/assets/terrain/adapter.mjs';
const tone=new Float32Array(4096);
for(let n=0;n<2048;n++)tone[n*2]=tone[n*2+1]=.5*Math.sin(n*2*Math.PI*440/44100);
const analyzer=new MusicAnalysis(),audio=analyzer.update(tone,1);
assert(Math.abs(audio.rms-.5/Math.sqrt(2))<.01);
const peak=audio.spectrum.indexOf(Math.max(...audio.spectrum));
assert(Math.abs(peak*44100/2048-440)<44100/2048);
assert.equal(half(1),0x3c00);assert.equal(half(1.9999),0x4000);assert.equal(half(-1.9999),0xc000);
const rhythm=new MusicAnalysis(),samples=new Float32Array(4096);let last;
for(let frame=0;frame<600;frame++){
 const time=frame/30;
 for(let n=0;n<2048;n++){const t=time+n/44100,amplitude=.02+.7*Math.exp(-(t%.5)*20);samples[n*2]=samples[n*2+1]=Math.sin(t*2*Math.PI*110)*amplitude;}
 last=rhythm.update(samples,time);
}
assert(last.beatCount>=36&&last.beatCount<=41,last.beatCount);
const quiet=new MusicAnalysis().update(new Float32Array(4096),1);assert.equal(quiet.rms,0);assert.equal(quiet.beatCount,0);
console.log(JSON.stringify({result:'pass',tone_peak_hz:peak*44100/2048,rms:audio.rms,detected_beats:last.beatCount,expected_beats:40,half_rounding:'pass'}));
