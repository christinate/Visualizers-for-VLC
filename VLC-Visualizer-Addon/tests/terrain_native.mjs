import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {Renderer} from '../../Terrain64/assets/terrain/adapter.mjs';

const [probe,shader,folder]=process.argv.slice(2);assert(probe&&shader&&folder);
mkdirSync(folder,{recursive:true});
const renderer=new Renderer({width:192,height:128},{cameraSeconds:60,colorBeats:4,peak:1000});
let uniforms,history;
renderer.device={queue:{writeBuffer(_b,_o,data){uniforms=Array.from(data);},writeTexture(_t,data){history=Array.from(data);},submit(){}},
  createCommandEncoder(){return {beginRenderPass(){return {setPipeline(){},setBindGroup(){},draw(){},end(){}};},finish(){}};}};
renderer.context={getCurrentTexture(){return {createView(){}};}};
const times=[0,.016,.034,.05,.067,.1,2,50.999,51,55,59.99,60,90,111,119.99,120,151,171,179.99,180,209,231,239.99,240];
const fixtures=[],expected=[];
for(let n=0;n<times.length;++n){
  const time=times[n],wave=Float32Array.from({length:2048},(_v,index)=>Math.sin(index*.03+n*.5)*.7);
  const audio={wave,rms:.1+n*.003,beatEnvelope:(n%5)/4,beatCount:n};
  const cameraSeconds=n<20?60:30,colorBeats=n<12?4:8,nextColor=n===9;
  renderer.settings.cameraSeconds=cameraSeconds;renderer.settings.colorBeats=colorBeats;
  if(nextColor)renderer.key(' ');
  renderer.render(audio,1/60,time);
  fixtures.push({...audio,wave:Array.from(wave),time,cameraSeconds,colorBeats,nextColor});
  expected.push({uniforms,history,next_row:renderer.row,mode:['above','under','side','valley'].indexOf(renderer.cameraMode)});
}
const input=resolve(folder,'terrain-input.json'),output=resolve(folder,'terrain-result.json');
writeFileSync(input,JSON.stringify(fixtures));writeFileSync(resolve(folder,'terrain-reference.json'),JSON.stringify(expected));
execFileSync(resolve(probe),[input,resolve(shader),output],{stdio:'inherit'});
const actual=JSON.parse(readFileSync(output,'utf8'));assert.equal(actual.length,expected.length);
let maximum=0;
for(let n=0;n<actual.length;++n){
  assert.equal(actual[n].mode,expected[n].mode,`${n}.mode`);assert.equal(actual[n].next_row,expected[n].next_row,`${n}.row`);
  assert.deepEqual(actual[n].history,expected[n].history,`${n}.half-float history`);
  actual[n].uniforms.forEach((value,index)=>{const difference=Math.abs(value-expected[n].uniforms[index]);maximum=Math.max(maximum,difference);assert(difference<=.00001,`${n}.uniform[${index}]: ${value} != ${expected[n].uniforms[index]}`);});
}
const summary={result:'pass',frames:actual.length,half_float_history:'exact',camera_and_palette_uniform_maximum_error:maximum,
  reference:'Terrain64/assets/terrain/adapter.mjs'};
writeFileSync(resolve(folder,'comparison.json'),JSON.stringify(summary,null,2)+'\n');console.log(JSON.stringify(summary));
