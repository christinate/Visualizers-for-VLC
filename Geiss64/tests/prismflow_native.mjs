// Reference-only development check; Node is not a native runtime dependency.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const sourceFile=path.join(root,'assets/prismflow/motion_wormhole.js');
const source=fs.readFileSync(sourceFile,'utf8');
const {FillWormholeMotionMap}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-core-reference'));
fs.mkdirSync(output,{recursive:true});
const inputs=[
  ['default',96,64,{}],
  ['wide',128,32,{}],
  ['tall',32,128,{}],
  ['orthographic',65,65,{perspective:false,azimuth_degrees:27,center_x:.1,center_y:-.15}],
  ['reverse-flow',96,64,{r1:.4,r2:.32,rotation:-.7,inward_speed:-.1,fall_acceleration:.2,elevation_degrees:37}],
  ['stationary',64,64,{rotation:0,inward_speed:0,fall_acceleration:0}],
  ['nadir',64,64,{elevation_degrees:90}],
  ['full-size-motion',512,512,{r1:.31,r2:.47,rotation:-.6,inward_speed:.15,fall_acceleration:.2,elevation_degrees:30}],
];
const fixtures=[];
for(const [name,width,height,options] of inputs){
  const values=new Float32Array(width*height*2),stats=FillWormholeMotionMap(values,width,height,options);
  const reference=name+'.f32';
  fs.writeFileSync(path.join(output,reference),Buffer.from(values.buffer));
  fixtures.push({name,width,height,options,reference,stats:[stats.plane,stats.lip,stats.tube,stats.miss]});
}
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(fixtures,null,2)+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference:'assets/prismflow/motion_wormhole.js',sha256:crypto.createHash('sha256').update(source).digest('hex'),node:process.version},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:60000,maxBuffer:1024*1024});
  if(result.error)throw result.error;
  process.stdout.write(result.stdout);process.stderr.write(result.stderr);
  if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+fixtures.length+' complete-map reference cases in '+output);
