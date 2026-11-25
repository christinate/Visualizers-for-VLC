// Reference fixtures are development inputs; native rendering needs no JS runtime.
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
  let source=bytes.toString('utf8').replace(/from\s+(['"])\.\/([^'"]+)\1/g,(_,quote,dependency)=>'from '+JSON.stringify(moduleUrl(dependency)));
  // Expose private serialization helpers to the fixture generator only.
  if(file==='palette.js')source+='\nexport {SerializePalette,DeserializePalette,g_text_palettes};\n';
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
function mulberry32(seed){return ()=>{let t=seed+=0x6D2B79F5;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
const originalRandom=Math.random;let activeRandom;
// The background module captures Math.random at import time. Keep one proxy
// for both imports and all fixtures so every draw uses the same seeded stream.
Math.random=()=>activeRandom();
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-palette-reference'));fs.mkdirSync(output,{recursive:true});
const manifest={saved:[],generated:[],custom:[],mutations:[],tone:[]};
try{
  const reference=await import(moduleUrl('palette.js'));
  function stream(seed,prefix=[]){const next=mulberry32(seed);let calls=0;activeRandom=()=>{const n=calls++;return n<prefix.length?prefix[n]:next();};return ()=>calls;}
  function toneCorrection(headroom,t,rgb,maximum,result){
    const max=Math.max(maximum/headroom,1),scale=Math.pow(1/max,.57),desaturate=Math.max(0,t+max*scale-2)*.25;
    if(desaturate>1)return 'bounded_white_fade';
    if(result.some(v=>!Number.isFinite(v))){
      if(desaturate!==0||!result.every(Number.isNaN)||!rgb.every(v=>Math.fround(Math.fround(v/headroom)*scale)===0))
        throw new Error('Unexpected nonfinite reference tone: '+JSON.stringify({headroom,t,rgb,maximum,result}));
      return 'zero_luminance';
    }
    return '';
  }
  function encode(definition,calls){
    const rgba=Array.from(definition.palette),expanded=Array.from(reference.ExpandPalette(definition.palette));
    const samples=[-1,0,1/7,.13,.7,1,2].map(t=>({t,rgb:Object.values(reference.SamplePalette(definition.palette,t))}));
    const maximum=Math.max(...expanded.filter((_,i)=>i%4!==3));
    const tone=[1,2.5,16].map(headroom=>{const corrections=[];const values=Array.from({length:256},(_,i)=>{
      const rgb=expanded.slice(i*4,i*4+3),t=i*(1/255);
      const result=Array.from(reference.ReduceHdrHeadroom(headroom,t,...rgb,maximum,false));
      corrections.push(toneCorrection(headroom,t,rgb,maximum,result));
      return result;
    });return {headroom,values,corrections};});
    return {rgba,expanded,samples,tone,name:definition.name,index:definition.index,background:Array.from(definition.bkg),calls,
      serialized:reference.SerializePalette(definition.palette)};
  }
  const initialCount=reference.GetSavedPaletteCount();manifest.saved_count=initialCount;
  for(let index=0;index<initialCount;index++){
    const seed=(0x159357+index*8179)>>>0,calls=stream(seed),definition=reference.GetSavedPalette(index);
    const fixture=encode(definition,calls()),name='saved-'+index+'.json';fixture.text=reference.g_text_palettes[index];
    fs.writeFileSync(path.join(output,name),JSON.stringify(fixture)+'\n');manifest.saved.push({index,seed,reference:name});
  }
  const generationCases=[];
  for(let seed=0;seed<64;seed++)generationCases.push({seed,prefix:[]});
  for(const first of [0,.005,.005000000000000001])for(const seed of [0,1,31,0xffffffff])generationCases.push({seed,prefix:[first]});
  for(const unit of [0,.5,1-Number.EPSILON])generationCases.push({seed:17,prefix:[.5,unit]});
  for(let index=0;index<generationCases.length;index++){
    const {seed,prefix}=generationCases[index],calls=stream(seed,prefix),definition=reference.GenerateRandomPalette();
    const name='generated-'+index+'.json';fs.writeFileSync(path.join(output,name),JSON.stringify(encode(definition,calls()))+'\n');
    manifest.generated.push({seed,prefix,reference:name});
  }
  const custom=[new Float32Array([.03125,.09375,.15625,1]),new Float32Array([0,0,0,1,65504,123.125,.00001,7]),
    new Float32Array([0,0,0,1,0,0,0,1]),new Float32Array([.13,.21,.77,1,2,3,4,1,3,1,2,1])];
  for(const rgba of custom){
    const calls=stream(0x8293),definition={palette:rgba,name:'custom',index:-1,bkg:reference.GenerateCompanionColor(rgba)};
    // The black palette and very bright custom input use their own maxima.
    const fixture=encode(definition,calls());fixture.text=String(rgba.length/4)+'\t'+Array.from(rgba).filter((_,i)=>i%4!==3).join('\r\n');
    const name='custom-'+manifest.custom.length+'.json';fs.writeFileSync(path.join(output,name),JSON.stringify(fixture)+'\n');manifest.custom.push({seed:0x8293,reference:name});
  }
  for(const rgba of custom){
    const index=reference.AddPalette(rgba),serialized=reference.g_text_palettes[index];
    const background=[.03125,.09375,.15625],saved=reference.SaveBkgColor(index,background),text=reference.g_text_palettes[index];
    manifest.mutations.push({rgba:Array.from(rgba),index,serialized,background,saved,text});
  }
  // Direct SDR coverage includes exact black, subnormal float values, values
  // around shadow/white transitions and the largest representable half float.
  for(const rgb of [[0,0,0],[1e-44,0,0],[.01,.02,.03],[.48,.4799,.4801],[1,1,1],[.1,3,1],[65504,65504,65504]]){
    for(const t of [0,.5,1])for(const maximum of [1,4,65504]){
      const values=Array.from(reference.HdrToSdr(t,...rgb,maximum,false));
      manifest.tone.push({rgb,t,maximum,values,correction:toneCorrection(1,t,rgb,maximum,values)});
    }
  }
  fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(manifest)+'\n');
  fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference_hashes:hashes,node:process.version,seeded_random:'Mulberry32; reference algorithm unchanged; private helpers exported in memory'},null,2)+'\n');
}finally{Math.random=originalRandom;}
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+manifest.saved.length+' saved and '+manifest.generated.length+' generated palette fixtures');
