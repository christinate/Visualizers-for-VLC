// Local development oracle; the installed visualizer never executes JavaScript.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const source=fs.readFileSync(path.join(root,'assets/prismflow/image_palette.js'));
const {GeneratePaletteFromImageData}=await import('data:text/javascript;base64,'+source.toString('base64'));
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-image-palette-reference'));fs.mkdirSync(output,{recursive:true});
function mulberry32(seed){return ()=>{let t=seed+=0x6d2b79f5;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
const images=[];
function image(label,width,height,pixel){const data=new Uint8Array(width*height*4);for(let y=0;y<height;++y)for(let x=0;x<width;++x)data.set(pixel(x,y),4*(y*width+x));const file='image-'+images.length+'.u8';fs.writeFileSync(path.join(output,file),data);images.push({label,width,height,file,data});}
for(const c of [[0,0,0,255],[255,255,255,255],[128,128,128,255],[255,0,0,255],[0,255,0,255],[0,0,255,255],[0,255,255,255],[255,0,255,255],[255,128,0,13]])image('solid-'+c.join('-'),1,1,()=>c);
image('alpha-threshold',2,1,x=>x?[19,104,237,13]:[255,255,0,12]);
image('transparent-padding',31,7,(x,y)=>x===15&&y===3?[19,104,237,13]:[255,255,0,0]);
image('weighted-pairs',17,9,(x,y)=>(x+y)%2?[250,27,1,255]:[1,73,250,13+(x*y)%242]);
image('complementary-pairs',32,1,x=>x%2?[255,0,0,255]:[0,255,255,255]);
image('grayscale-ramp',256,1,x=>[x,x,x,255]);
image('color-ramp-wide',128,9,(x,y)=>[x*2,y*29,(x*19+y*7)%256,255]);
image('color-ramp-tall',7,128,(x,y)=>[x*41,y*2,(y*19+x*7)%256,13+(x*y)%243]);
const noise=mulberry32(0x8712345);image('seeded-color-noise',37,23,()=>[0,0,0,0].map(()=>Math.floor(noise()*256)));
image('maximum-visible-monochrome',512,512,()=>[37,61,227,255]);
image('transparent',3,2,()=>[255,17,93,0]);
image('below-alpha-threshold',5,1,x=>[x*51,255,117,12]);
const manifest={cases:[],images:images.map(({data,...item})=>item)};
const options=[{}, {smoothness:0}, {smoothness:.35}, {smoothness:1},
  {cluster_count:1,cluster_iterations:1,smoothness:.5},
  {cluster_count:32,cluster_iterations:50,smoothness:0,highlight_luminance:.97,sample_size:512},
  {cluster_count:5,cluster_iterations:3,smoothness:1,highlight_luminance:.000001},
  {cluster_count:2,cluster_iterations:1,smoothness:0,highlight_luminance:1-Number.EPSILON}];
for(let index=0;index<images.length;++index)for(let o=0;o<options.length;++o)for(const seed of [0,17,0xffffffff]){
  // One full-size monochrome case exercises the input bound without repeating
  // the same large cluster calculation for every option/seed combination.
  if(index===17&&(o!==0||seed!==17))continue;
  const image=images[index],prefix=o===6?[0,.5,1-Number.EPSILON,0,0,0]:[],next=mulberry32(seed);let calls=0;
  const random=()=>{const n=calls++;return n<prefix.length?prefix[n]:next();};
  const entry={image:index,seed,prefix,options:options[o]};
  try{entry.colors=GeneratePaletteFromImageData(image,{...options[o],random});entry.rgba=entry.colors.flatMap(rgb=>[...rgb.map(Math.fround),1]);}catch(error){entry.error=error.message;}
  entry.calls=calls;manifest.cases.push(entry);
}
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(manifest)+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({source_sha256:crypto.createHash('sha256').update(source).digest('hex'),node:process.version,scope:'Unchanged GeneratePaletteFromImageData; deterministic sRGB RGBA8 input, Mulberry32/options; no decoder approximation or browser dependency in native builds'},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const r=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:120000,maxBuffer:4*1024*1024});
  if(r.error)throw r.error;process.stdout.write(r.stdout);process.stderr.write(r.stderr);if(r.status!==0)process.exit(r.status||1);
}else console.log(JSON.stringify({images:images.length,cases:manifest.cases.length}));
