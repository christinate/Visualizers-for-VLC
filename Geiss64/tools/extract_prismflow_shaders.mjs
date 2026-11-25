// Development-only extraction from the preserved reference. Native builds and
// installed visualizers load tracked WGSL files and never run this JavaScript.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import nativeFixes from './native_shader_fixes.cjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const assets=path.join(root,'assets/prismflow'),output=path.join(root,'native/prismflow/shaders');
const mode=process.argv[2]||'--check';if(!['--check','--write'].includes(mode))throw new Error('Use --check or --write');
const modules=new Map(),hashes={};const extra={
  'gpu_warp.js':['kUniforms','kWarpFieldWGSL'],
  'gpu_dynamic_warp.js':['kSharedWGSL','kDynamicModes','create_noise_pipeline'],
  'gpu_advected_dots.js':['kWarpDotsWGSL','kBokehAdvectionWGSL','kBokehRecordDeathsWGSL','kBokehStatsWGSL','kBokehDrawWGSL'],
  'webgpu_present.js':['shaderWGSL'],
};
function moduleUrl(file){
  if(modules.has(file))return modules.get(file);
  // Match the LF Git snapshot even when a worktree has CRLF line endings.
  const reference=fs.readFileSync(path.join(assets,file),'utf8').replace(/\r\n/g,'\n');
  hashes[file]=crypto.createHash('sha256').update(reference).digest('hex');
  let source=reference.replace(/^([ \t]*import\b[\s\S]*?\bfrom)\s+(['"])\.\/([^'"]+)\2/gm,
    (_,prefix,quote,dependency)=>prefix+' '+JSON.stringify(moduleUrl(dependency)));
  if(extra[file])source+='\nexport {'+extra[file].join(',')+'};\n';
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
const warp=await import(moduleUrl('gpu_warp.js')),dynamic=await import(moduleUrl('gpu_dynamic_warp.js')),
  particles=await import(moduleUrl('gpu_advected_dots.js')),dots=await import(moduleUrl('dot_renderer.js')),
  text=await import(moduleUrl('gpu_text_overlay.js')),present=await import(moduleUrl('webgpu_present.js')),
  palette=await import(moduleUrl('draw_palette.js'));
const reference=fs.readFileSync(path.join(assets,'gpu_warp.js'),'utf8');const notice=reference.slice(0,reference.indexOf('*/')+2).replace(/\r\n/g,'\n');
const generated=new Map(),shaders=[];
function add(name,source,options){
  const description=options.native_fixes?'shader body has the declared native correction.':'shader body unchanged.';
  const file=name+'.wgsl',bytes=Buffer.from('// Extracted for Prism Flow native Dawn; '+description+'\n'+notice+'\n'+source+'\n');
  generated.set(file,bytes);shaders.push({name,file,sha256:crypto.createHash('sha256').update(bytes).digest('hex'),...options});
}
const render=(format='rgba16float',more={})=>({kind:'render',vertex:'vs_main',fragment:'fs_main',topology:'triangle-list',formats:[format],...more});
const compute=entry=>({kind:'compute',entry});const shared=warp.kUniforms+warp.kWarpFieldWGSL;
add('warp',warp.kWarpWGSL,render());add('auto_exposure',warp.kAutoExposureWGSL,compute('cs_main'));
for(const [motion,formula] of Object.entries(dynamic.kDynamicModes))add('dynamic_'+motion,dynamic.kSharedWGSL+formula,render('rg16float',{layout:'dynamic'}));
for(const dimension of ['2d','3d']){
  const fake={createShaderModule:({code})=>code,createComputePipeline:descriptor=>descriptor};
  const pipeline=dynamic.create_noise_pipeline(fake,dimension);add('noise_'+dimension,pipeline.compute.module,compute(pipeline.compute.entryPoint));
}
add('warp_dots',shared+particles.kWarpDotsWGSL,compute('cs_main'));
add('bokeh_advection',shared+particles.kBokehAdvectionWGSL,compute('cs_bokeh'));
add('bokeh_deaths',particles.kBokehRecordDeathsWGSL,compute('cs_record_deaths'));
add('bokeh_stats',particles.kBokehStatsWGSL,compute('cs_bokeh_stats'));
const outputFormats=['rgba16float','rgba8unorm','bgra8unorm'];
add('bokeh_draw',particles.kBokehDrawWGSL,render('rgba16float',{vertex:'vs_bokeh',fragment:'fs_bokeh',topology:'triangle-strip',formats:outputFormats,blend:'bokeh'}));
add('dots',dots.kDotPassWGSL,render('rgba16float',{topology:'triangle-strip',vertex_buffers:'dots',blend:'additive'}));
add('text_overlay',text.kGpuTextOverlayWGSL,render('rgba16float',{blend:'additive'}));
add('present',nativeFixes.applyCalibrationFinalSquare(present.shaderWGSL),render('rgba16float',{
  formats:outputFormats,native_fixes:[nativeFixes.calibrationFixId],
  reference_shader_sha256:crypto.createHash('sha256').update(present.shaderWGSL).digest('hex')}));
add('palette_overlay',palette.kPaletteOverlayWGSL,render('rgba16float',{formats:outputFormats}));
generated.set('manifest.json',Buffer.from(JSON.stringify({format_version:1,reference_hashes:hashes,shaders},null,2)+'\n'));
if(mode==='--write')fs.mkdirSync(output,{recursive:true});
for(const [file,bytes] of generated){
  const target=path.join(output,file);
  if(mode==='--write')fs.writeFileSync(target,bytes);
  else if(!fs.existsSync(target)||!fs.readFileSync(target).equals(bytes))throw new Error('Native shader differs from preserved reference: '+file);
}
console.log((mode==='--write'?'Wrote ':'Verified ')+shaders.length+' native shader modules ('+[...generated.values()].reduce((n,b)=>n+b.length,0)+' bytes including manifest)');
