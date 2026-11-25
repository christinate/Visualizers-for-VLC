// Full preserved Engine + GPUWarp + presenter, exercised on owner machines only.
import {GPUWarp,kWarpWGSL} from '/assets/gpu_warp.js';
import {WebGPUPresenter,shaderWGSL} from '/assets/webgpu_present.js';
import {PaletteOverlayPass} from '/assets/draw_palette.js';
const NativeWorker=globalThis.Worker;
// palette_bkg_color captures Math.random at module import. Install one proxy
// before loading Engine, then change its source without replacing the proxy.
const originalRandom=Math.random;let activeRandom=originalRandom;Math.random=()=>activeRandom();
globalThis.Worker=class {postMessage(){throw Error('Unexpected worker request in bounded scene fixture');}};
const {Engine,JumpToNextPreset,resetSceneReference,sceneReferenceSnapshot}=await import('/assets/engine.js');
function mulberry32(seed){return ()=>{seed=(seed+0x6D2B79F5)>>>0;let t=seed;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
function base64(view){const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);let s='';for(let n=0;n<bytes.length;n+=32768)s+=String.fromCharCode(...bytes.subarray(n,n+32768));return btoa(s);}
async function readImage(device,texture){
 const size=texture.format==='rgba16float'?8:4,w=texture.width,h=texture.height,row=Math.ceil(w*size/256)*256;
 const buffer=device.createBuffer({size:row*h,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ}),encoder=device.createCommandEncoder();
 encoder.copyTextureToBuffer({texture},{buffer,bytesPerRow:row,rowsPerImage:h},[w,h]);device.queue.submit([encoder.finish()]);await buffer.mapAsync(GPUMapMode.READ);
 const mapped=new Uint8Array(buffer.getMappedRange()),result=new Uint8Array(w*h*size);for(let y=0;y<h;y++)result.set(mapped.subarray(y*row,y*row+w*size),y*w*size);buffer.unmap();buffer.destroy();return result;
}
window.runReference=async()=>{
 const result={result:'pass',files:{},frames:[],cases:[]};let device;
 try{
  let adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('No reference GPU adapter');result.adapter={...adapter.info.toJSON?.(),vendor:adapter.info.vendor,architecture:adapter.info.architecture,description:adapter.info.description};
  for(let scene=0;scene<3;scene++){
   if(scene)adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});
   const seed=0x31415926,ss=scene===1?2:1,linear=scene===2,headroom=linear?1000/203:1;let width=128,height=80,presetTime=1000;const random=mulberry32(seed);let randomCalls=0;activeRandom=()=>{++randomCalls;return random();};
   resetSceneReference();device=await adapter.requestDevice();const errors=[];device.addEventListener('uncapturederror',e=>errors.push(e.error.message));
   const format=linear?'rgba16float':'rgba8unorm';let target=device.createTexture({size:[width,height],format,usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC});
   const presenter=new WebGPUPresenter(null,width,height,width*ss,height*ss,ss);presenter.device=device;presenter.format=format;presenter.context={getCurrentTexture:()=>target};presenter.configure_canvas=()=>linear;presenter.use_hdr=linear;
   presenter.uniform_buf=device.createBuffer({size:256,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
   presenter.paletteTex=device.createTexture({size:[256,1],format:'rgba8unorm',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
   const module=device.createShaderModule({code:shaderWGSL});presenter.pipeline=device.createRenderPipeline({layout:'auto',vertex:{module,entryPoint:'vs_main'},fragment:{module,entryPoint:'fs_main',targets:[{format}]},primitive:{topology:'triangle-list'}});
   presenter.gpu_warp=new GPUWarp(device,width*ss,height*ss,kWarpWGSL);presenter.gpu_warp.init();presenter.gpu_warp.fill_index_u8_with_noise();presenter.palette_overlay=new PaletteOverlayPass(device,format,presenter.paletteTex);presenter.BuildBindGroup(presenter.gpu_warp.get_current_index_view());
   const engine=new Engine(presenter,width,height,width*ss,height*ss),wave=new Float32Array(2048),spectrum=new Float32Array(512),fullSpectrum=new Float32Array(1024);
   const caseInfo={scene,seed,supersampling:ss,linear,headroom,initialRandomCalls:randomCalls};result.cases.push(caseInfo);
   for(let frame=0;frame<12;frame++){
    if(frame===3){width=145;height=91;target.destroy();target=device.createTexture({size:[width,height],format,usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC});presenter.resize(width,height,width*ss,height*ss,ss);engine.resize(width,height,width*ss,height*ss);}
    const key=frame===4?32:frame>=6&&frame<=10?['m','p','w','d','s'][frame-6].charCodeAt(0):0;
    if(key===32)presetTime=JumpToNextPreset(presetTime,false);
    const dt=frame>=5?.25:1/60;presetTime+=dt;const time=frame/60;
    for(let n=0;n<wave.length;n++)wave[n]=Math.fround(.6*Math.sin(n*.037+frame*.3));
    for(let n=0;n<fullSpectrum.length;n++)fullSpectrum[n]=Math.fround(.2+.1*Math.cos(n*.03+frame));for(let n=0;n<spectrum.length;n++)spectrum[n]=(fullSpectrum[n*2]+fullSpectrum[n*2+1])*.5;
    // Record the actual delayed cached value used by this original frame.
    // Replay supplies this one scalar; all particle compute/draw still executes.
    const fading=presenter.gpu_warp.fading_dots,mean=fading.get_bokeh_average_lifespan()??4;
    const getter=fading.get_bokeh_average_lifespan;fading.get_bokeh_average_lifespan=()=>mean;
    engine.render(presetTime,presetTime,time,frame,false,key===109,key===112,key===119,key===100,key===115,null,1,1/Math.max(dt,.001),1,1.5,0,frame%3===0?1:0,1,false,1,headroom,false,'',1,1,
      false,false,false,false,false,false,{wave,rms:.12,visual_energy:.04},{calibrating_hdr_headroom:0},-1,spectrum,'this_is_the_last_param');
    fading.get_bokeh_average_lifespan=getter;presenter.draw(0,[-1,-1,-1]);
    const prefix='scene-'+scene+'-frame-'+frame,feedbackFile=prefix+'-feedback.f16le',colorFile=prefix+'-color.'+(linear?'f16le':'u8');
    result.files[feedbackFile]=base64(await readImage(device,presenter.gpu_warp.index_tex[presenter.gpu_warp.ping]));result.files[colorFile]=base64(await readImage(device,target));
    result.frames.push({scene,frame,width,height,dt,time,key,presetTime,averageStarLifetime:mean,randomCalls,snapshot:structuredClone(sceneReferenceSnapshot()),feedback_file:feedbackFile,color_file:colorFile});
   }
   await device.queue.onSubmittedWorkDone();if(errors.length)throw Error(errors.slice(0,8).join('\n'));device.destroy();device=null;
  }
  return result;
 }catch(error){return {result:'fail',error:error.stack||String(error)};}finally{Math.random=originalRandom;globalThis.Worker=NativeWorker;if(device)device.destroy();}
};
