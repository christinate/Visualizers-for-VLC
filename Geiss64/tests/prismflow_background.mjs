// Development reference generation; no JavaScript runtime in native rendering.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const file=path.join(root,'assets/prismflow/palette_bkg_color.js'),source=fs.readFileSync(file,'utf8');
const reference=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-background-reference'));fs.mkdirSync(output,{recursive:true});
const colors=[[0,0,0],[1,1,1],[.18,.18,.18],[1,0,0],[0,1,0],[0,0,1],[.9,.2,.7],[.02,.8,.3],[.0001,.0002,.0003],
  [.7,.45,.08],[.09,.22,.85],[Math.fround(.13),Math.fround(.31),Math.fround(.91)]];
const options=[{}, {adventurousness:0,max_lightness_difference:0,mixing_fraction:0},
  {adventurousness:1,max_lightness_difference:30,mixing_fraction:1}];
const fixtures=[];
for(let color=0;color<colors.length;color++)for(let variant=0;variant<options.length;variant++){
  const foreground=colors[color],p=options[variant];let candidates;
  try{candidates=reference.GetBackgroundColorCandidates(foreground,p);}catch(error){throw new Error('Background reference failed: '+error.message);}
  const choices=[];
  for(const unit of [0,.5,1-Number.EPSILON]){
    let calls=0;const rgb=reference.PickBackgroundColor(foreground,{...p,random:()=>{calls++;return unit;}});choices.push({unit,rgb,calls});
  }
  const name='color-'+color+'-options-'+variant;
  fs.writeFileSync(path.join(output,name+'.json'),JSON.stringify(candidates.map(c=>[...c.rgb,c.score,c.preference,c.foreground,c.harmony,c.blend,c.lightness_difference,c.probability]))+'\n');
  fixtures.push({name,foreground,options:p,choices,reference:name+'.json'});
}
fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(fixtures)+'\n');
fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference:'assets/prismflow/palette_bkg_color.js',sha256:crypto.createHash('sha256').update(source).digest('hex'),node:process.version},null,2)+'\n');
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+fixtures.length+' complete background candidate fixtures');
