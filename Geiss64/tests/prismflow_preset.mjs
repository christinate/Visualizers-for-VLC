// Complete preset/reference checks. No JavaScript is used by native rendering.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const assets=path.join(root,'assets/prismflow'),modules=new Map(),hashes={};
function moduleUrl(file){
  if(modules.has(file))return modules.get(file);
  const bytes=fs.readFileSync(path.join(assets,file));hashes[file]=crypto.createHash('sha256').update(bytes).digest('hex');
  const source=bytes.toString('utf8').replace(/from\s+(['"])\.\/([^'"]+)\1/g,(_,quote,dependency)=>'from '+JSON.stringify(moduleUrl(dependency)));
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
function mulberry32(seed){return ()=>{let t=seed+=0x6D2B79F5;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
const originalRandom=Math.random;let activeRandom;Math.random=()=>activeRandom();
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-preset-reference'));fs.mkdirSync(output,{recursive:true});
const cases=[],samplePixels=[];for(let y=0;y<32;y++)for(let x=0;x<32;x++)samplePixels.push(Math.floor(y*511/31)*512+Math.floor(x*511/31));
try{
  const {Preset}=await import(moduleUrl('preset.js'));
  const definitions=[];for(let seed=0;seed<32;seed++)definitions.push({seed,mode:-1});
  // RNG state after the complete scene's dynamic/dot/Gaussian/index startup.
  definitions.push({seed:2141510928,mode:-1});
  for(let mode=0;mode<40;mode++)definitions.push({seed:(0x7546321+mode*8147)>>>0,mode});
  for(const mode of [40,41,43,44,98,99])definitions.push({seed:0x945128,mode});
  function snapshot(preset,calls){
    const copy={...preset,warps:preset.warps.map(w=>({...w,warp_map:{...w.warp_map,
      src_dxy:w.warp_map.src_dxy?samplePixels.flatMap(pixel=>[w.warp_map.src_dxy[pixel*2],w.warp_map.src_dxy[pixel*2+1]]):null,
      dynamic_motion_params:w.warp_map.dynamic_motion_params?Array.from(w.warp_map.dynamic_motion_params):null}})),
      palette_orig:{...preset.palette_orig,palette:Array.from(preset.palette_orig.palette),bkg:Array.from(preset.palette_orig.bkg)},
      palette_expanded:Array.from(preset.palette_expanded),calls};
    return structuredClone(copy);
  }
  for(let index=0;index<definitions.length;index++){
    const {seed,mode}=definitions[index],random=mulberry32(seed);let calls=0;activeRandom=()=>{calls++;return random();};
    const number=index,t1=number*25-2,t2=t1+4,preset=new Preset(number,t1,t2,mode),states=[{operation:'generate',state:snapshot(preset,calls)}];
    if(index<12){
      for(const [operation,method] of [['wave','RandomizeWaveParams'],['dots','RandomizeDotParams'],['shader','RandomizeShaderParams'],['palette','RandomizePalette']]){
        preset[method]();states.push({operation,state:snapshot(preset,calls)});
      }
      const slot=index%4,debugMode=[35,37,38,39,0,16,34,24,3,8,17,11][index];
      preset.SetWarpSlotDebug(slot,debugMode);states.push({operation:'slot',slot,mode:debugMode,state:snapshot(preset,calls)});
      preset.RandomizeMotion();states.push({operation:'motion',state:snapshot(preset,calls)});
    }
    const name='preset-'+index+'.json';fs.writeFileSync(path.join(output,name),JSON.stringify(states)+'\n');cases.push({seed,mode,number,t1,t2,reference:name});
  }
  fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify({sample_pixels:samplePixels,cases})+'\n');
  fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference_hashes:hashes,node:process.version,random:'Mulberry32 shared through import-time proxy'},null,2)+'\n');
}finally{Math.random=originalRandom;}
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+cases.length+' complete preset fixtures');
