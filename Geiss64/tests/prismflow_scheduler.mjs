// Exercise the preserved history functions without browser/GPU imports.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const bytes=fs.readFileSync(path.join(root,'assets/prismflow/engine.js')),source=bytes.toString('utf8').replace(/\r\n/g,'\n');
const begin=source.indexOf('let g_global_preset_number ='),end=source.indexOf('// PRESET BKG GEN');
if(begin<0||end<begin)throw new Error('Reference history markers changed');
function declaration(name){const begin=source.indexOf('function '+name+'('),end=source.indexOf('\n}',begin);if(begin<0||end<0)throw new Error('Reference function missing: '+name);return source.slice(begin,end+2);}
const historySource=source.slice(begin,end).replace(/export /g,'');
const reference=new Function('ShowError','console',historySource+'\n'+declaration('smoothstep')+'\n'+declaration('SmoothPresetBlendT')+`
  return {set(rows){g_presets.length=0;for(const row of rows)g_presets.push({...row,warp_maps_ready:true,RebuildWarpMaps(){this.warp_maps_ready=true;}});},
    active:GetActivePresets,next:JumpToNextPreset,previous:JumpToPrevPreset,prune(){PrunePresets();return g_presets.map(p=>p.global_preset_number);}};
`)(()=>{},{log(){}});
const rows=Array.from({length:128},(_,i)=>({global_preset_number:i,t1:i*25-2,t2:i*25+2}));reference.set(rows);
const times=[-100,0,10000];for(const row of rows)times.push(row.t1-.0001,row.t1,row.t1+.0001,(row.t1+row.t2)/2,row.t2-.0001,row.t2,row.t2+.0001,row.t2+10);
const cases=times.map(time=>({time,active:reference.active(time),next_hard:reference.next(time,true),next_soft:reference.next(time,false),previous:reference.previous(time)}));
const pruneRows=Array.from({length:142},(_,i)=>({global_preset_number:i,t1:i*25-2,t2:i*25+2}));reference.set(pruneRows);
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-scheduler-reference'));fs.mkdirSync(output,{recursive:true});
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify({rows,cases,prune_rows:pruneRows,pruned:reference.prune()})+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference:'assets/prismflow/engine.js',sha256:crypto.createHash('sha256').update(bytes).digest('hex'),node:process.version,
  extraction:'Unchanged history and smoothstep function bodies; browser error display and map rebuild are timing-only stubs. Actual native map rebuilds are tested separately.'},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:240000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+cases.length+' timing/navigation cases');
