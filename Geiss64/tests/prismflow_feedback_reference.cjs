// Development oracle only: run with the preserved local Electron binary.
// No Electron or Node dependency is introduced into native rendering.
'use strict';
const {app,BrowserWindow,session}=require('electron');
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto');
const nativeFixes=require('../tools/native_shader_fixes.cjs');
let assets,output,inputs,highPerformance=false,vulkanReference=false,nativeCalibrationFix=false,referenceCase='feedback';
for(let n=2;n<process.argv.length;n++){
  if(process.argv[n]==='--assets')assets=path.resolve(process.argv[++n]);
  else if(process.argv[n]==='--output')output=path.resolve(process.argv[++n]);
  else if(process.argv[n]==='--inputs')inputs=path.resolve(process.argv[++n]);
  else if(process.argv[n]==='--high-performance-gpu')highPerformance=true;
  else if(process.argv[n]==='--vulkan-reference')vulkanReference=true;
  else if(process.argv[n]==='--native-calibration-fix')nativeCalibrationFix=true;
  else if(process.argv[n]==='--case')referenceCase=process.argv[++n];
  else throw Error('Unknown feedback-reference argument');
}
if(!assets||!output)throw Error('Expected --assets and --output');
if(!['feedback','presentation','dots','bokeh','text','fonts','images','scene'].includes(referenceCase))throw Error('Unknown reference case');
if(nativeCalibrationFix&&referenceCase!=='presentation')throw Error('The calibration correction is only for the presentation comparison');
if(nativeCalibrationFix&&fs.existsSync(path.join(output,'fixtures.json')))throw Error('Use a fresh output directory for corrected calibration fixtures');
if(referenceCase==='images'&&!inputs)throw Error('Expected local --inputs for image reference');
fs.mkdirSync(output,{recursive:true});const profile=path.join(output,'reference-profile');fs.mkdirSync(profile,{recursive:true});
app.setPath('userData',profile);app.setPath('sessionData',profile);app.setPath('crashDumps',path.join(profile,'crash-dumps'));
app.commandLine.appendSwitch('disable-background-networking');app.commandLine.appendSwitch('disable-component-update');app.commandLine.appendSwitch('no-pings');
if(highPerformance)app.commandLine.appendSwitch('force_high_performance_gpu');
if(vulkanReference){
  // Local developer capture only. Permit the requested software Vulkan adapter;
  // sandboxing and the external-request block remain enabled.
  app.commandLine.appendSwitch('enable-unsafe-webgpu');app.commandLine.appendSwitch('use-gl','angle');
  app.commandLine.appendSwitch('use-angle','vulkan');app.commandLine.appendSwitch('use-vulkan','native');
  app.commandLine.appendSwitch('use-webgpu-adapter','vulkan');
  app.commandLine.appendSwitch('enable-features','Vulkan');app.commandLine.appendSwitch('disable-vulkan-surface');
}
let server,window;const watchdog=setTimeout(()=>{console.error('Local feedback reference timed out');app.exit(1);},90000);
function fail(error){console.error(error.stack||String(error));fs.writeFileSync(path.join(output,'reference-status.json'),JSON.stringify({result:'fail',error:String(error)},null,2)+'\n');clearTimeout(watchdog);if(window)window.destroy();if(server)server.close();app.exit(1);}
app.whenReady().then(async()=>{
  if(app.dock)app.dock.hide();
  server=http.createServer((req,res)=>{
    try{
      if(req.method!=='GET')throw Error('Only local GET requests are allowed');
      const name=new URL(req.url,'http://127.0.0.1').pathname;let bytes,type;
      if(name==='/reference.html'){
        bytes=Buffer.from('<!doctype html><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="default-src \'none\'; script-src \'self\'; connect-src \'self\'; img-src blob:"><script type="module" src="/reference.mjs?case='+referenceCase+'"></script>');type='text/html';
      }else if(name==='/reference.mjs'){bytes=fs.readFileSync(path.join(__dirname,'prismflow_'+(referenceCase==='scene'?'scene':referenceCase==='presentation'?'present':referenceCase==='images'?'image':referenceCase==='fonts'?'font':referenceCase==='text'?'text':'feedback')+'_browser.mjs'));type='text/javascript';}
      else if(referenceCase==='images'&&name.startsWith('/input/')){const file=decodeURIComponent(name.slice(7));if(path.basename(file)!==file||file.includes('\0'))throw Error('Invalid local image input');const full=path.join(inputs,file);if(!fs.statSync(full).isFile()||fs.statSync(full).size>64*1024*1024)throw Error('Local image input exceeds bound');bytes=fs.readFileSync(full);type=file.endsWith('.json')?'application/json':'application/octet-stream';}
      else if(/^\/assets\/[a-z0-9_]+\.js$/.test(name)){const file=path.join(assets,path.basename(name));if(fs.statSync(file).size>1024*1024)throw Error('Reference source exceeds bound');bytes=fs.readFileSync(file);type='text/javascript';
        if(['presentation','scene'].includes(referenceCase)&&path.basename(name)==='webgpu_present.js'){
          if(nativeCalibrationFix)bytes=Buffer.from(nativeFixes.applyCalibrationFinalSquare(bytes.toString('utf8')));
          bytes=Buffer.concat([bytes,Buffer.from('\nexport {shaderWGSL};\n')]);
        }
        if(referenceCase==='scene'&&path.basename(name)==='engine.js')bytes=Buffer.concat([bytes,Buffer.from('\nexport function sceneReferenceSnapshot(){return g_presets.map(p=>({t1:p.t1,t2:p.t2,wave:p.wave_params,dots:p.dot_params,shader:p.shader_params,motion:p.motion_params,palette_name:p.palette_orig.name,warps:p.warps.map(w=>({mode:w.warp_map.mode,seed:w.rand_seed,str:w.str}))}));}\nexport function resetSceneReference(){g_presets.length=0;g_global_preset_number=0;g_cur_preset_index=-1;g_bkg_preset=null;g_bkg_preset_in_progress=false;g_visual_energy_att=0;g_frozen_palette=null;g_title_x=g_title_y=-1;Object.assign(aligner,new WaveAligner());}\n')]);}
      else{res.writeHead(404);res.end();return;}
      res.writeHead(200,{'Content-Type':type,'Cache-Control':'no-store'});res.end(bytes);
    }catch(error){res.writeHead(400);res.end('Invalid local reference request');}
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const origin='http://127.0.0.1:'+server.address().port;
  const isolated=session.fromPartition('prism-flow-feedback-reference');
  isolated.webRequest.onBeforeRequest({urls:['<all_urls>']},(details,callback)=>callback({cancel:!details.url.startsWith(origin+'/')&&!(referenceCase==='images'&&details.url.startsWith('blob:'+origin+'/'))}));
  isolated.setPermissionRequestHandler((_webContents,_permission,callback)=>callback(false));
  window=new BrowserWindow({show:false,width:320,height:240,webPreferences:{session:isolated,nodeIntegration:false,contextIsolation:true,sandbox:true,offscreen:true}});
  window.webContents.on('console-message',(...args)=>{
    const [event,details,legacyMessage]=args;
    const message=event.message??(typeof details==='object'?details.message:String(legacyMessage??details));
    fs.appendFileSync(path.join(output,'renderer-console.log'),message.slice(0,8192)+'\n');
  });
  await window.loadURL(origin+'/reference.html');
  // The scene module imports Engine asynchronously after installing its local
  // worker stub. Page load can finish before that module's top-level await.
  const result=await window.webContents.executeJavaScript(`(async()=>{
    const start=performance.now();while(typeof window.runReference!=='function'){
      if(performance.now()-start>10000)throw Error('Reference module did not become ready');
      await new Promise(resolve=>setTimeout(resolve,20));
    }return await window.runReference();})()`);
  if(result.result!=='pass')throw Error(result.error||'Reference renderer failed');
  const entries=Object.entries(result.files);if(entries.length>256)throw Error('Reference output count exceeds bound');let total=0;
  for(const [file,base64] of entries){if(!/^[a-z0-9_-]+\.(f32le|f16le|u8)$/.test(file)||typeof base64!=='string')throw Error('Invalid reference output filename');
    const bytes=Buffer.from(base64,'base64');total+=bytes.length;if(bytes.length>4*1024*1024||total>64*1024*1024)throw Error('Reference output exceeds bound');fs.writeFileSync(path.join(output,file),bytes);}
  const hashes={};for(const file of fs.readdirSync(assets).filter(file=>file.endsWith('.js')))hashes[file]=crypto.createHash('sha256').update(fs.readFileSync(path.join(assets,file))).digest('hex');
  const manifest={...result};delete manifest.files;manifest.provenance={electron:process.versions.electron,chrome:process.versions.chrome,high_performance_switch:highPerformance,vulkan_reference_switch:vulkanReference,reference_case:referenceCase,reference_hashes:hashes,network:'loopback-only; external session requests blocked',scope:referenceCase==='feedback'?'Unchanged GPUWarp with empty waveform and disabled particle spawning; feedback/exposure/maps only':referenceCase==='dots'?'Unchanged GPUWarp with CPU points and GPU warp-dot advection/draw; diagnostic COPY_SRC usage added to particle buffers, no bokeh spawning':referenceCase==='bokeh'?'Unchanged GPUWarp and GpuBokehDots spawning/update/deaths/stats/draw methods; diagnostic COPY_SRC buffer usage, synthetic inputs and offscreen targets':referenceCase==='text'?'Unchanged text stamp/timing methods; supplied synthetic and original-canvas masks; diagnostic COPY_SRC texture usage; native font rasterization remains separate':'Unchanged presenter upload/uniform/bind/draw and palette overlay; offscreen targets, synthetic title texture, no bokeh; export-only shader visibility shim'};
  if(referenceCase==='fonts')manifest.provenance.scope='Unchanged original set_text Canvas rasterization; native fonts are compared separately, not supplied by these masks; independent padded redraw detects clipping';
  if(referenceCase==='images')manifest.provenance.scope='Unchanged GeneratePaletteFromImage; supplied local synthetic files, diagnostic capture of original Canvas getImageData; output encoder fixtures only for local codec testing';
  if(referenceCase==='scene')manifest.provenance.scope='Full unchanged Engine, GPUWarp and presenter with native-equivalent generated audio, seeded RNG, navigation, rerandomization, resize and supersampling. Offscreen target, shader-export/reset-only visibility shims; actual delayed bokeh statistic captured for deterministic native replay. Font/image decoding parity is tested separately.';
  if(nativeCalibrationFix){
    manifest.provenance.native_shader_fixes=[nativeFixes.calibrationFixId];
    manifest.provenance.scope+='; explicit calibration-final-square-index correction applied in memory, preserved sources unchanged';
    manifest.provenance.native_fix_source_sha256=crypto.createHash('sha256').update(fs.readFileSync(path.join(__dirname,'../tools/native_shader_fixes.cjs'))).digest('hex');
  }
  fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify(manifest)+'\n');fs.writeFileSync(path.join(output,'reference-status.json'),JSON.stringify({result:'pass',frames:result.frames.length,files:entries.length,bytes:total,adapter:result.adapter},null,2)+'\n');
  console.log(JSON.stringify({result:'pass',frames:result.frames.length,files:entries.length,bytes:total,adapter:result.adapter}));
  clearTimeout(watchdog);window.destroy();server.close();app.quit();
}).catch(fail);
