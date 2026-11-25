// Full-map reference generation; Node is a development tool only.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const assets=path.join(root,'assets/prismflow');
const modules=new Map(),provenance={};
function moduleUrl(file){
  if(modules.has(file))return modules.get(file);
  const bytes=fs.readFileSync(path.join(assets,file));provenance[file]=crypto.createHash('sha256').update(bytes).digest('hex');
  let source=bytes.toString('utf8').replace(/from\s+(['"])\.\/([^'"]+)\1/g,(_,quote,dependency)=>'from '+JSON.stringify(moduleUrl(dependency)));
  if(file==='motion.js')source+='\nexport {mulberry32};\n';
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
const reference=await import(moduleUrl('motion.js'));
const presetReference=await import(moduleUrl('preset.js'));
const constants=await import(moduleUrl('const.js'));
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-motion-reference'));fs.mkdirSync(output,{recursive:true});
const metadata={constants:[constants.kWarpMapSize,constants.kDynamicMotionParamCount,constants.kMaxActiveWarps,constants.kGpuWarpSlots,constants.kNoiseTexSize,constants.kNoiseTexCount],
  modes:reference.g_warp_maps.map(({mode,weight})=>({mode,weight})),
  pairs:[reference.kModePairIsSuperGood,reference.kModePairIsGood,reference.kModePairIsBad,reference.kModePairIsForbidden].map(matrix=>matrix.map(row=>Array.from(row))),
  weights:[reference.kSuperGoodModePairWeightScale,reference.kGoodModePairWeightScale,reference.kBadModePairWeightScale,reference.kForbiddenModePairWeightScale],
  random:[-2147483648,-123456789,-1,0,1,2147483647].map(seed=>{const random=reference.mulberry32(seed);return {seed,values:Array.from({length:256},()=>random())};}),selection:[]};
const originalRandom=Math.random;
try{
  const random=reference.mulberry32(0x13572468);
  const companionSets=[[],...[...Array(40)].map((_,i)=>[i]),[3,4,5],[14,17,24],[35,37,38],[16,17,35,37],[39,39,39,39]];
  for(const companions of companionSets){for(const unit of [0,1-Number.EPSILON,...Array.from({length:24},()=>random())]){
    Math.random=()=>unit;metadata.selection.push({companions,unit,mode:presetReference.PickRandomWarpMap(companions)});
  }}
}finally{Math.random=originalRandom;}
fs.writeFileSync(path.join(output,'metadata.json'),JSON.stringify(metadata)+'\n');
const cases=[];
for(const seed of [0,-123456789]){
  for(const mode of [-1,...Array.from({length:40},(_,i)=>i),40,41,43,44,98,99]){
    let value;try{value=reference.buildWarpMap(mode,seed);}catch(error){throw new Error('Reference motion failed: mode '+mode+' seed '+seed+': '+error.message);}
    const file='mode-'+mode+'-seed-'+seed;
    const values=value.src_dxy||value.dynamic_motion_params;
    for(const v of values)if(!Number.isFinite(v))throw new Error('Reference produced nonfinite motion: mode '+mode+' seed '+seed);
    fs.writeFileSync(path.join(output,file+'.f32'),Buffer.from(values.buffer,values.byteOffset,values.byteLength));
    cases.push({mode,seed,width:value.W,height:value.H,dynamic:value.src_dxy===null,preferences:value.warp_prefs,
      breathing:[value.breathing_str,value.breathing_phase,value.breathing_freq],reference:file+'.f32',count:values.length});
  }
  console.log('Generated full motion maps for seed '+seed);
}
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(cases)+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference_files:provenance,node:process.version},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}
