// Development-only oracle: original JavaScript methods/shaders on native Dawn.
// Nothing from this runner or its Node binding belongs in installed payloads.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {pathToFileURL}=require('node:url');
const nativeFixes=require('../tools/native_shader_fixes.cjs');
let moduleFile,provenanceFile,assets,output,backend,referenceCase='presentation',calibrationFix=false;
for(let n=2;n<process.argv.length;++n){
  const option=process.argv[n];
  if(option==='--module')moduleFile=path.resolve(process.argv[++n]);
  else if(option==='--provenance')provenanceFile=path.resolve(process.argv[++n]);
  else if(option==='--assets')assets=path.resolve(process.argv[++n]);
  else if(option==='--output')output=path.resolve(process.argv[++n]);
  else if(option==='--backend')backend=process.argv[++n];
  else if(option==='--case')referenceCase=process.argv[++n];
  else if(option==='--native-calibration-fix')calibrationFix=true;
  else throw Error('Unknown native reference argument: '+option);
}
if(!moduleFile||!provenanceFile||!assets||!output||!['d3d12','metal','vulkan'].includes(backend))throw Error('Expected module/provenance/assets/output/backend');
if(!['presentation','scene'].includes(referenceCase)||calibrationFix&&referenceCase!=='presentation')throw Error('Unsupported native reference case/correction');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const pin=JSON.parse(fs.readFileSync(path.join(__dirname,'../sdk/dawn/dependencies.lock.json')));
const provenance=JSON.parse(fs.readFileSync(provenanceFile));
if(provenance.dawn_revision!==pin.source_revision||provenance.module_sha256!==sha(fs.readFileSync(moduleFile)))throw Error('Dawn reference binding provenance mismatch');
fs.mkdirSync(output,{recursive:false});const modules=path.join(output,'reference-modules');fs.mkdirSync(modules);
fs.writeFileSync(path.join(modules,'package.json'),' {"type":"module"}\n');
const hashes={};
for(const file of fs.readdirSync(assets).filter(name=>/^[a-z0-9_]+\.js$/.test(name))){
  const full=path.join(assets,file);if(fs.statSync(full).size>1024*1024)throw Error('Reference source exceeds bound');
  const bytes=fs.readFileSync(full);hashes[file]=sha(bytes);let source=bytes.toString('utf8');
  if(file==='webgpu_present.js'){
    if(calibrationFix)source=nativeFixes.applyCalibrationFinalSquare(source);
    source+='\nexport {shaderWGSL};\n';
  }
  if(file==='engine.js'&&referenceCase==='scene')source+='\nexport function sceneReferenceSnapshot(){return g_presets.map(p=>({t1:p.t1,t2:p.t2,wave:p.wave_params,dots:p.dot_params,shader:p.shader_params,motion:p.motion_params,palette_name:p.palette_orig.name,warps:p.warps.map(w=>({mode:w.warp_map.mode,seed:w.rand_seed,str:w.str}))}));}\nexport function resetSceneReference(){g_presets.length=0;g_global_preset_number=0;g_cur_preset_index=-1;g_bkg_preset=null;g_bkg_preset_in_progress=false;g_visual_energy_att=0;g_frozen_palette=null;g_title_x=g_title_y=-1;Object.assign(aligner,new WaveAligner());}\n';
  fs.writeFileSync(path.join(modules,file),source);
}
const harnessFile=path.join(__dirname,referenceCase==='scene'?'prismflow_scene_browser.mjs':'prismflow_present_browser.mjs');
const harness=fs.readFileSync(harnessFile);const adapted=harness.toString('utf8').replace(/(["'])\/assets\//g,'$1./reference-modules/');
fs.writeFileSync(path.join(output,'reference.mjs'),adapted);
const watchdog=setTimeout(()=>{console.error('Native reference timed out');process.exit(1);},120000);
async function main(){
  if(referenceCase==='scene'&&typeof Float16Array!=='function')throw Error('Full-scene reference requires native Float16Array (Node 24+); do not use the old renderer fallback converter');
  const binding=require(moduleFile);Object.assign(globalThis,binding.globals);
  globalThis.window=globalThis;let gpu=binding.create(['backend='+backend]);
  Object.defineProperty(globalThis,'navigator',{value:{gpu},configurable:true});
  globalThis.fetch=()=>Promise.reject(Error('Network requests are disabled in the local reference'));
  globalThis.XMLHttpRequest=undefined;globalThis.WebSocket=undefined;
  // The bounded scene never rasterizes text. Preserve its original blank-mask
  // initialization, and fail explicitly if a fixture starts using Canvas.
  globalThis.OffscreenCanvas=class {
    constructor(width,height){if(width!==1||height!==1)throw Error('Canvas rasterization is outside this fixture');this.width=width;this.height=height;}
    getContext(kind){if(kind!=='2d')throw Error('Unexpected Canvas context');return new Proxy({},{get(){throw Error('Canvas drawing is outside this native reference fixture');}});}
  };
  await import(pathToFileURL(path.join(output,'reference.mjs')).href);
  const result=await globalThis.runReference();if(result.result!=='pass')throw Error(result.error||'Original native reference failed');
  const entries=Object.entries(result.files);if(entries.length>256)throw Error('Reference file count exceeds bound');let total=0;
  for(const [file,base64] of entries){
    if(!/^[a-z0-9_-]+\.(f32le|f16le|u8)$/.test(file)||typeof base64!=='string')throw Error('Invalid reference filename');
    const bytes=Buffer.from(base64,'base64');total+=bytes.length;if(bytes.length>4*1024*1024||total>64*1024*1024)throw Error('Reference output exceeds bound');
    fs.writeFileSync(path.join(output,file),bytes,{flag:'wx'});
  }
  delete result.files;
  result.provenance={host:'original JavaScript on development-only native Dawn Node binding',node:process.versions.node,
    reference_case:referenceCase,reference_hashes:hashes,dawn_revision:pin.source_revision,module_sha256:provenance.module_sha256,
    backend_requested:backend,native_float16_array:typeof Float16Array==='function',harness_sha256:sha(harness),adapted_harness_sha256:sha(Buffer.from(adapted)),
    native_shader_fixes:calibrationFix?[nativeFixes.calibrationFixId]:[],network:'No server; fetch/WebSocket/XMLHttpRequest disabled',
    canvas:'Blank 1x1 initialization only; any Canvas drawing fails. Font/image rasterization is checked separately.',
    workers:'Preserved bounded fixture rejects worker requests; background scheduling is checked separately.',
    scope:'Original method/shader bodies retained except explicitly listed native_shader_fixes; export/reset visibility shims and local fixture-import rewriting. Renderer port is not used to generate outputs.'};
  fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(result)+'\n',{flag:'wx'});
  const summary={result:'pass',frames:result.frames.length,files:entries.length,bytes:total,adapter:result.adapter,host:result.provenance.host};
  fs.writeFileSync(path.join(output,'reference-status.json'),JSON.stringify(summary,null,2)+'\n',{flag:'wx'});
  console.log(JSON.stringify(summary));clearTimeout(watchdog);delete globalThis.navigator;gpu=null;if(global.gc)global.gc();
}
main().catch(error=>{
  clearTimeout(watchdog);const message=String(error.stack||error).slice(0,8192);
  fs.writeFileSync(path.join(output,'reference-status.json'),JSON.stringify({result:'fail',error:message},null,2)+'\n');
  console.error(message);process.exit(1);
});
