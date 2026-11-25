'use strict';
// Local-only renderer runtime. Node/native access is confined to this process.
const {app,BrowserWindow,ipcMain,protocol,session,screen,dialog}=require('electron');
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
protocol.registerSchemesAsPrivileged([{scheme:'visualizer',privileges:{standard:true,secure:true,supportFetchAPI:true,corsEnabled:true,stream:true}}]);
app.commandLine.appendSwitch('disable-background-networking');
app.commandLine.appendSwitch('disable-component-update');
function parse(argv){
  const result={id:'',owner:0,demo:false,settings:false,capture:0,lineIn:null,quitAfter:0,test:false,background:false};
  for(let n=0;n<argv.length;++n){const a=argv[n];
    if(a==='--visualizer')result.id=argv[++n];else if(a==='--settings'){result.id=argv[++n];result.settings=true;}
    else if(a==='--suite')result.suite=argv[++n];else if(a==='--vlc-pid')result.owner=Number(argv[++n]);
    else if(a==='--capture-for')result.capture=Number(argv[++n]);else if(a==='--line-in')result.lineIn=argv[n+1]&&!argv[n+1].startsWith('--')?argv[++n]:'default';
    else if(a==='--profile')result.profile=argv[++n];else if(a==='--quit-after')result.quitAfter=Number(argv[++n]);
    else if(a==='--demo')result.demo=true;else if(a==='--self-test')result.test=true;else if(a==='--background')result.background=true;
    else if(a==='--capture')result.captureImage=argv[++n];else if(a==='--hdr-test')result.hdrTest=true;
  }return result;
}
const initial=parse(process.argv.slice(1));
const defaultProfile=process.platform==='win32'?path.join(process.env.LOCALAPPDATA,'VLCVisualizers','profiles'):process.platform==='darwin'?path.join(app.getPath('home'),'Library/Application Support/VLCVisualizers/profiles'):path.join(process.env.XDG_CONFIG_HOME||path.join(app.getPath('home'),'.config'),'vlc-visualizers/profiles');
const initialProfile=path.resolve(initial.profile||defaultProfile);
const initialSuite=path.resolve(initial.suite||path.join(__dirname,'..'));
fs.mkdirSync(initialProfile,{recursive:true});
app.setPath('userData',path.join(initialProfile,'browser-'+crypto.createHash('sha256').update(initialSuite).digest('hex').slice(0,12)));
const gotLock=app.requestSingleInstanceLock();
const contexts=new Map(),captures=new Map();let bridge;
const safeId=id=>typeof id==='string'&&/^[a-z0-9_-]{1,64}$/.test(id);
function contained(base,relative){
  if(typeof relative!=='string'||path.isAbsolute(relative)||relative.split(/[\\/]/).includes('..'))throw Error('Invalid package path');
  const root=fs.realpathSync(base),value=fs.realpathSync(path.join(base,relative)),rel=path.relative(root,value);
  if(rel.startsWith('..')||path.isAbsolute(rel))throw Error('Package path escapes its directory');return value;
}
function loadJson(file){if(fs.statSync(file).size>1048576)throw Error('JSON exceeds 1 MiB');return JSON.parse(fs.readFileSync(file,'utf8'));}
function writeJson(file,value){fs.mkdirSync(path.dirname(file),{recursive:true});const tmp=file+'.tmp-'+process.pid;fs.writeFileSync(tmp,JSON.stringify(value,null,2)+'\n',{mode:0o600});fs.renameSync(tmp,file);}
function controlDirectory(){
  if(process.platform==='win32')return path.join(process.env.LOCALAPPDATA,'VLCVisualizers','control');
  if(process.platform==='darwin')return path.join(app.getPath('home'),'Library/Application Support/VLCVisualizers/control');
  return process.env.XDG_RUNTIME_DIR?path.join(process.env.XDG_RUNTIME_DIR,'vlc-visualizers/control'):path.join(app.getPath('home'),'.cache/vlc-visualizers/control');
}
function settingsFor(profile,id){
  const defaults={width:640,height:480,fps:60,fullscreen:false,hdr:false,peak:600,supersampling:1,engine:'modern',cameraSeconds:60,colorBeats:4};
  // Carry forward the original Windows host's saved display preferences.
  if(process.platform==='win32'&&!fs.existsSync(path.join(profile,id+'.json')))try{
    const file=path.join(profile,'settings.ini');if(fs.statSync(file).size>1048576)throw Error('Settings file too large');
    let section='';for(const line of fs.readFileSync(file,'utf8').split(/\r?\n/)){
      const header=line.trim().match(/^\[([^\]]+)\]$/);if(header){section=header[1];continue;}
      if(section!==id)continue;const setting=line.match(/^\s*(width|height|fps|peak|fullscreen|hdr)\s*=\s*(\d+)\s*$/);
      if(setting)defaults[setting[1]]=['fullscreen','hdr'].includes(setting[1])?Number(setting[2])!==0:Number(setting[2]);
    }
  }catch{}
  try{Object.assign(defaults,loadJson(path.join(profile,id+'.json')));}catch{}
  const bounded=(value,min,max,fallback)=>Number.isFinite(Number(value))?Math.max(min,Math.min(max,Number(value))):fallback;
  return {width:bounded(defaults.width,320,3840,640),height:bounded(defaults.height,240,2160,480),fps:bounded(defaults.fps,15,144,60),fullscreen:!!defaults.fullscreen,hdr:!!defaults.hdr,peak:bounded(defaults.peak,200,2000,600),supersampling:[1,2].includes(Number(defaults.supersampling))?Number(defaults.supersampling):1,engine:defaults.engine==='classic'?'classic':'modern',cameraSeconds:bounded(defaults.cameraSeconds,30,180,60),colorBeats:bounded(defaults.colorBeats,1,16,4),inputDevice:typeof defaults.inputDevice==='string'?defaults.inputDevice:''};
}
function live(pid){try{process.kill(pid,0);return true;}catch(e){return e.code==='EPERM';}}
function log(context,value){try{const file=path.join(context.profile,'browser-'+context.id+'.log');if(fs.existsSync(file)&&fs.statSync(file).size>8*1024*1024)fs.renameSync(file,file+'.previous');fs.appendFileSync(file,JSON.stringify({timestamp:new Date().toISOString(),...value})+'\n');}catch{}}
function markWindow(context){
  const directory=controlDirectory();fs.mkdirSync(directory,{recursive:true,mode:0o700});
  const token=crypto.randomBytes(16).toString('hex');const record=path.join(directory,process.pid+'-'+token+'.json');
  writeJson(record,{protocol:1,pid:process.pid,owner_pid:context.owner,id:context.id,token,suite:fs.realpathSync(context.suite)});
  context.control={record,token,request:record+'.close'};
}
function removeControl(context){if(!context.control)return;for(const file of [context.control.record,context.control.request])try{fs.unlinkSync(file);}catch{}}
function get(event){const context=contexts.get(event.sender.id);if(!context||event.senderFrame!==event.sender.mainFrame||!event.senderFrame.url.startsWith('visualizer://'))throw Error('Unknown visualizer context');return context;}
function makeWindow(context,kind){
  const settings=context.settings;
  const window=new BrowserWindow({width:kind==='renderer'?settings.width:600,height:kind==='renderer'?settings.height:550,frame:kind!=='renderer',resizable:true,show:!context.background||kind!=='renderer',title:kind==='renderer'?context.name:kind==='settings'?context.name+' settings':'Choose audio input',backgroundColor:'#080b12',webPreferences:{preload:path.join(__dirname,'preload.cjs'),nodeIntegration:false,contextIsolation:true,sandbox:true,backgroundThrottling:false}});
  const contentsId=window.webContents.id;
  window.setMenu(null);context.window=window;context.kind=kind;contexts.set(contentsId,context);
  window.webContents.setWindowOpenHandler(()=>({action:'deny'}));
  window.webContents.on('will-navigate',(event,url)=>{if(!url.startsWith('visualizer://'))event.preventDefault();});
  window.webContents.on('before-input-event',(event,input)=>{
    if(kind!=='renderer'||input.type!=='keyDown')return;
    if(input.key==='Escape'){event.preventDefault();window.close();}
    else if((input.control||input.meta)&&input.key.toLowerCase()==='l'&&!context.owner){event.preventDefault();chooseAudio(context);}
    else if(input.key==='F11'||(input.alt&&input.key==='Enter')){event.preventDefault();toggle(window);}
  });
  window.webContents.on('render-process-gone',(_event,details)=>{log(context,{failure:'renderer-process-gone',details});});
  window.on('closed',()=>{
    contexts.delete(contentsId);removeControl(context);
    if(kind==='renderer'){if(context.captureKey)closeCapture(context.captureKey);bridge.release(context.sourcePid||0);}
    if(kind==='capture'){captures.delete(context.captureKey);bridge.release(context.captureKey);}
    if(!contexts.size)app.quit();
  });
  if(kind==='renderer'){markWindow(context);if(settings.fullscreen)toggle(window);if(context.background)window.showInactive();}
  return window;
}
function toggle(window){if(process.platform==='darwin')window.setSimpleFullScreen(!window.isSimpleFullScreen());else window.setFullScreen(!window.isFullScreen());}
function closeCapture(key){const capture=captures.get(key);if(capture&&!capture.window.isDestroyed())capture.window.destroy();captures.delete(key);bridge.release(key);}
function chooseAudio(context){
  if(context.owner)return;
  const key=context.captureKey||(context.captureKey=crypto.randomInt(0x40000000,0x7fffffff));
  let capture=captures.get(key);
  if(capture&&!capture.window.isDestroyed()){capture.window.show();capture.window.focus();return;}
  capture={...context,owner:0,control:null,captureKey:key,target:context,lineIn:context.lineIn,background:false};
  const window=makeWindow(capture,'capture');captures.set(key,capture);window.loadURL('visualizer://runtime/input.html');
}
function publicConfig(context){
  return {id:context.id,name:context.name,kind:context.kind,settings:context.settings,sourcePid:context.sourcePid||0,captureKey:context.captureKey||0,owner:context.owner,demo:context.demo,lineIn:context.lineIn,test:context.test,entry:context.entry,platform:process.platform,hdrTest:!!context.hdrTest};
}
async function dispatch(args){
  if(!safeId(args.id))throw Error('Select a valid visualizer ID');
  const suite=fs.realpathSync(path.resolve(args.suite||initialSuite));const packageRoot=contained(suite,path.join('plugins',args.id));const manifest=loadJson(path.join(packageRoot,'manifest.json'));
  if(manifest.id!==args.id||typeof manifest.name!=='string')throw Error('Package manifest mismatch');
  const profile=path.resolve(args.profile||defaultProfile);fs.mkdirSync(profile,{recursive:true});
  const context={...args,suite,profile,name:manifest.name,entry:manifest.entrypoint,settings:settingsFor(profile,args.id),sourcePid:args.owner||0,pcm:new Float32Array(4096)};
  if(args.capture){
    context.captureKey=args.capture;context.owner=args.capture;context.sourcePid=0;
    let capture=captures.get(args.capture);if(capture){capture.window.show();capture.window.focus();return;}
    const window=makeWindow(context,'capture');captures.set(args.capture,context);window.loadURL('visualizer://runtime/input.html');return;
  }
  if(args.settings){makeWindow(context,'settings').loadURL('visualizer://runtime/settings.html');return;}
  if(manifest.api_version!==3||manifest.adapter!=='webgpu')throw Error('This renderer requires a native host');
  contained(packageRoot,manifest.entrypoint);
  const window=makeWindow(context,'renderer');window.loadURL('visualizer://'+args.id+'/'+manifest.entrypoint);
  if(!args.owner&&!args.demo)chooseAudio(context);
  if(args.quitAfter>0)setTimeout(()=>{if(!window.isDestroyed())window.close();},Math.min(args.quitAfter,86400000));
}
if(!gotLock){app.quit();}else{
  app.on('second-instance',(_event,argv)=>{dispatch(parse(argv.slice(1))).catch(error=>dialog.showErrorBox('Visualizer',error.message));});
  app.whenReady().then(async()=>{
    const native=process.env.VLC_VISUALIZERS_AUDIO_BRIDGE||path.join(__dirname,'native','visualizer_audio.node');bridge=require(native);
    session.defaultSession.setPermissionRequestHandler((contents,permission,callback,details)=>{
      const context=contexts.get(contents.id);callback(!!context&&context.kind==='capture'&&permission==='media'&&(details.mediaTypes||[]).every(type=>type==='audio')&&String(details.requestingUrl||'').startsWith('visualizer://runtime/'));
    });
    session.defaultSession.setPermissionCheckHandler((contents,permission,origin)=>!!contexts.get(contents?.id)&&contexts.get(contents.id).kind==='capture'&&permission==='media'&&origin.startsWith('visualizer://runtime'));
    session.defaultSession.webRequest.onBeforeRequest({urls:['http://*/*','https://*/*','ws://*/*','wss://*/*']},(_details,callback)=>callback({cancel:true}));
    protocol.handle('visualizer',request=>{
      try{
        const url=new URL(request.url);let base;
        if(url.hostname==='runtime')base=path.join(__dirname,'ui');
        else if(safeId(url.hostname))base=contained(initialSuite,path.join('plugins',url.hostname));
        else throw Error('Unknown local origin');
        const relative=decodeURIComponent(url.pathname).replace(/^\/+/,'');const file=contained(base,relative);
        const extension=path.extname(file);const types={'.html':'text/html','.js':'application/javascript','.mjs':'application/javascript','.json':'application/json','.css':'text/css','.wgsl':'text/plain','.png':'image/png','.jpg':'image/jpeg','.txt':'text/plain'};
        if(!types[extension])throw Error('Unsupported resource');
        return new Response(fs.readFileSync(file),{headers:{'Content-Type':types[extension],'Access-Control-Allow-Origin':'*','Cross-Origin-Opener-Policy':'same-origin','Cross-Origin-Embedder-Policy':'require-corp','Content-Security-Policy':"default-src 'self' visualizer:; script-src 'self' visualizer:; worker-src 'self' visualizer:; connect-src 'self' visualizer:; img-src 'self' visualizer: data:; style-src 'self' visualizer: 'unsafe-inline'; object-src 'none'; base-uri 'none'"}});
      }catch(error){return new Response(error.message,{status:404});}
    });
    ipcMain.handle('config',event=>publicConfig(get(event)));
    ipcMain.handle('audio-read',event=>{
      const context=get(event);if(context.kind!=='renderer')return null;
      if(context.sourcePid)bridge.readAudio(context.sourcePid,context.pcm);else context.pcm.fill(0);
      return context.pcm;
    });
    ipcMain.handle('audio-write',(event,samples,rate)=>{
      const context=get(event);if(context.kind!=='capture'||!(samples instanceof Float32Array)||samples.length>32768||samples.length%2||!Number.isFinite(rate))throw Error('Invalid recording packet');
      return bridge.writeAudio(context.captureKey,samples,rate,2);
    });
    ipcMain.handle('capture-ready',event=>{const context=get(event);return {lineIn:context.lineIn||context.settings.inputDevice||'',demo:context.demo};});
    ipcMain.handle('capture-selected',(event,device,label)=>{
      const context=get(event);if(context.kind!=='capture'||typeof device!=='string'||device.length>1024)throw Error('Invalid recording source');
      if(context.target){context.target.sourcePid=context.captureKey;context.target.demo=false;context.target.window.webContents.send('audio-source-changed',{demo:false,sourcePid:context.captureKey});}
      context.settings.inputDevice=device;writeJson(path.join(context.profile,context.id+'.json'),context.settings);log(context,{audioInput:label,device});context.window.hide();return true;
    });
    ipcMain.handle('capture-cancel',event=>{const context=get(event);context.window.hide();return true;});
    ipcMain.handle('audio-choose',event=>chooseAudio(get(event)));
    ipcMain.handle('fullscreen',event=>toggle(get(event).window));
    ipcMain.handle('close',event=>get(event).window.close());
    ipcMain.handle('settings',event=>{const context=get(event);return dispatch({...context,settings:true,capture:0});});
    ipcMain.handle('settings-save',(event,values)=>{
      const context=get(event);if(context.kind!=='settings'||!values||typeof values!=='object'||JSON.stringify(values).length>8192)throw Error('Invalid settings');
      writeJson(path.join(context.profile,context.id+'.json'),values);context.window.close();return true;
    });
    ipcMain.handle('report',(event,value)=>{
      const context=get(event);if(!value||JSON.stringify(value).length>16384)throw Error('Invalid renderer report');
      log(context,value);
      if(value.failure&&context.test){app.exit(11);return false;}
      if(value.ready&&context.test){
        const gpu=app.getGPUFeatureStatus();writeJson(path.join(context.profile,context.id+'-gpu.json'),{...value,gpu,display:screen.getDisplayMatching(context.window.getBounds())});
        setTimeout(async()=>{try{if(!context.window.isDestroyed()){const image=await context.window.webContents.capturePage(undefined,{stayHidden:true});fs.writeFileSync(path.join(context.profile,context.id+'-preview.png'),image.toPNG());}}catch(error){log(context,{captureError:error.message});}},2500);
      }
      return true;
    });
    setInterval(()=>{
      for(const context of [...contexts.values()]){
        try{
          if(context.control&&fs.existsSync(context.control.request)&&fs.readFileSync(context.control.request,'utf8').trim()===context.control.token)context.window.close();
          else if(context.owner&&!live(context.owner))context.window.close();
        }catch{}
      }
    },100).unref();
    setInterval(()=>{for(const context of contexts.values())if(context.kind==='renderer')log(context,{metrics:app.getAppMetrics().map(value=>({pid:value.pid,type:value.type,memory:value.memory}))});},5000).unref();
    await dispatch(initial);
  }).catch(error=>{fs.appendFileSync(path.join(initialProfile,'browser-startup.log'),error.stack+'\n');if(!initial.test)dialog.showErrorBox('Visualizer',error.message);app.exit(10);});
}
