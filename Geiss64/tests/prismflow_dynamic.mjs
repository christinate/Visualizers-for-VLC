// Development reference inputs only; installed native rendering uses no JS.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),assets=path.join(root,'assets/prismflow');
const modules=new Map(),hashes={};
function moduleUrl(file){
  if(modules.has(file))return modules.get(file);
  const bytes=fs.readFileSync(path.join(assets,file));hashes[file]=crypto.createHash('sha256').update(bytes).digest('hex');
  let source=bytes.toString('utf8').replace(/^([ \t]*import\b[\s\S]*?\bfrom)\s+(['"])\.\/([^'"]+)\2/gm,(_,prefix,quote,dependency)=>prefix+' '+JSON.stringify(moduleUrl(dependency)));
  if(file==='gpu_dynamic_warp.js')source+='\nexport {kUniformFloatCount};\n';
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
const reference=await import(moduleUrl('gpu_dynamic_warp.js')),motion=await import(moduleUrl('motion.js'));
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-dynamic-reference'));fs.mkdirSync(output,{recursive:true});
let captured;
const manager=Object.create(reference.GpuDynamicWarp.prototype);
const scratch=new Float32Array(512);manager.noise={spectrum_f32:scratch,spectrum_bits:new Uint32Array(scratch.buffer),spectrum_half:new Uint16Array(512),spectrum_texture:{}};
manager.device={queue:{writeTexture(_target,data){captured=Uint16Array.from(data);},writeBuffer(_buffer,_offset,data){captured=Float32Array.from(data);}}};
manager.begin_frame=()=>{};
function toHalf(values){if(values.length!==512)throw new Error('Expected reference spectrum chunk');manager.upload_spectrum(Float32Array.from(values));return Uint16Array.from(captured);}
const input=[NaN,Infinity,-Infinity,-0,0,65504,65505,-65504,1e5,-1e5,1e-46,2**-25,2**-24,2**-14,1.99951171875,.33333,
  1.00048828125,1.00146484375,-1.00048828125,-1.00146484375];
for(let i=input.length;i<512;i++)input.push(Math.fround(Math.sin(i*.713)*(i%2?1000:1e-4)));
const spectrumInput=input.map(v=>Number.isNaN(v)?'nan':v===Infinity?'infinity':v===-Infinity?'-infinity':Object.is(v,-0)?'-zero':v);
const spectrum={input:spectrumInput,expected:Array.from(toHalf(input))};
const noop=()=>{},encoder={beginRenderPass:()=>({setPipeline:noop,setBindGroup:noop,draw:noop,end:noop})};
const cases=[];
for(const mode of [35,37,38,39]){
  const map=motion.buildWarpMap(mode,-123456789),parameters=Array.from(map.dynamic_motion_params),uniforms=[];
  for(const time of [0,7.25,14400]){
    const state={motion_params:Float32Array.from(parameters),data:new Float32Array(reference.kUniformFloatCount),uniform_buf:{},pipeline:{},bind_group:{}};
    manager.encode(encoder,{},state,time);uniforms.push({time,data:Array.from(captured)});
  }
  cases.push({mode,parameters,uniforms});
}
const initialSeed=0x4913abef;
function hash(value){value=(value^(value>>>16))>>>0;value=Math.imul(value,0x7feb352d)>>>0;value=(value^(value>>>15))>>>0;value=Math.imul(value,0x846ca68b)>>>0;return (value^(value>>>16))>>>0;}
const noise=[];
function noiseFixture(name,width,height,depth,seed){
  const pixels=width*height*depth,values=new Float32Array(pixels*4),salts=[0xa511e9b3,0x63d83595,0xb5297a4d,0x1b56c4e9];
  for(let i=0;i<pixels;i++){const x=hash(i^seed);for(let c=0;c<4;c++)values[i*4+c]=Math.min((hash(x^salts[c])>>>8)/16777216,.99951171875);}
  // Use the reference's actual RNE Float32->Float16 upload method. Integer
  // hash arithmetic independently predicts the unchanged WGSL noise pixels.
  const packed=new Uint16Array(values.length);for(let i=0;i<values.length;i+=512)packed.set(toHalf(values.subarray(i,i+512)),i);
  const file=name+'.f16le',floatFile=name+'.f32le';fs.writeFileSync(path.join(output,file),Buffer.from(packed.buffer));
  fs.writeFileSync(path.join(output,floatFile),Buffer.from(values.buffer));noise.push({name,width,height,depth,seed:seed>>>0,file,float_file:floatFile});
}
noiseFixture('static_3d',64,64,16,initialSeed^0xa511e9b3);noiseFixture('static_2d',256,256,1,initialSeed^0x63d83595);
for(let frame=0;frame<=4;frame++)noiseFixture('dynamic_'+frame,64,64,1,(initialSeed+frame)>>>0);
noiseFixture('wrap_dynamic_0',64,64,1,0);
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify({initial_seed:initialSeed,spectrum,cases,noise})+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({node:process.version,reference_hashes:hashes,noise:'WGSL integer hash predicted independently; half conversion uses unchanged JS upload_spectrum',byte_order:'little-endian'},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  if(!process.argv[4])throw new Error('Expected shader asset directory');
  const result=spawnSync(path.resolve(process.argv[2]),['--assets',path.resolve(process.argv[4]),'--fixtures',path.join(output,'fixtures.json'),'--output',path.join(output,'gpu')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated dynamic uniforms, spectrum edge cases and '+noise.length+' full noise textures');
