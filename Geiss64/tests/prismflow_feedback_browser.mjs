// Executed only by the local preserved-reference test harness.
import {GPUWarp,kWarpWGSL} from '/assets/gpu_warp.js';
import {buildWarpMap} from '/assets/motion.js';
const dotMode=new URL(import.meta.url).searchParams.get('case')==='dots';
const bokehMode=new URL(import.meta.url).searchParams.get('case')==='bokeh';
function mulberry32(seed){return ()=>{let t=seed+=0x6D2B79F5;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
function base64(view){const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);let string='';for(let n=0;n<bytes.length;n+=32768)string+=String.fromCharCode(...bytes.subarray(n,n+32768));return btoa(string);}
async function readImage(device,texture){
  const pixelBytes=texture.format==='rgba16float'?8:4,width=texture.width,height=texture.height,rowBytes=Math.ceil(width*pixelBytes/256)*256;
  const buffer=device.createBuffer({size:rowBytes*height,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
  const encoder=device.createCommandEncoder();encoder.copyTextureToBuffer({texture},{buffer,bytesPerRow:rowBytes,rowsPerImage:height},{width,height});device.queue.submit([encoder.finish()]);
  await buffer.mapAsync(GPUMapMode.READ);const mapped=new Uint8Array(buffer.getMappedRange()),result=new Uint8Array(width*height*pixelBytes);
  for(let y=0;y<height;y++)result.set(mapped.subarray(y*rowBytes,y*rowBytes+width*pixelBytes),y*width*pixelBytes);buffer.unmap();buffer.destroy();return result;
}
async function readExposure(device,source){
  const buffer=device.createBuffer({size:16,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ}),encoder=device.createCommandEncoder();encoder.copyBufferToBuffer(source,0,buffer,0,16);device.queue.submit([encoder.finish()]);
  await buffer.mapAsync(GPUMapMode.READ);const result=Array.from(new Float32Array(buffer.getMappedRange()));buffer.unmap();buffer.destroy();return result;
}
async function readBuffer(device,source){
  const target=device.createBuffer({size:source.size,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ}),encoder=device.createCommandEncoder();encoder.copyBufferToBuffer(source,0,target,0,source.size);device.queue.submit([encoder.finish()]);
  await target.mapAsync(GPUMapMode.READ);const bytes=new Uint8Array(target.getMappedRange()).slice();target.unmap();target.destroy();return bytes;
}
window.runReference=async()=>{
  const result={result:'pass',files:{},maps:[],frames:[],initial_width:96,initial_height:64,init_seed:0x478125,
    init_rng_skip:1,dynamic_seed:0x253fac91,noise_files:[],float16_array:typeof Float16Array!=='undefined'};
  let device;const originalRandom=Math.random;
  try{
    if(!navigator.gpu)throw Error('Preserved reference has no WebGPU');const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('No reference GPU adapter');
    const info=adapter.info;result.adapter={vendor:info?.vendor,architecture:info?.architecture,device:info?.device,description:info?.description};device=await adapter.requestDevice();
    const errors=[];device.addEventListener('uncapturederror',event=>errors.push(event.error.message));device.lost.then(info=>{if(info.reason!=='destroyed')errors.push('Device lost: '+info.message);});
    let capturedUniforms,stage='init';
    const queue=new Proxy(device.queue,{get(target,key){
      if(key==='writeTexture')return (destination,data,layout,size)=>{
        if(stage==='init'&&destination.texture.format==='rgba8unorm'&&destination.texture.width===256&&destination.texture.height===256){const file='gaussian-'+result.noise_files.length+'.u8';result.noise_files.push(file);result.files[file]=base64(new Uint8Array(data.buffer,data.byteOffset,data.byteLength));}
        return target.writeTexture(destination,data,layout,size);
      };
      if(key==='writeBuffer')return (buffer,offset,data,...rest)=>{if(data instanceof Float32Array&&data.length===36)capturedUniforms=Array.from(data);return target.writeBuffer(buffer,offset,data,...rest);};
      const value=Reflect.get(target,key,target);return typeof value==='function'?value.bind(target):value;
    }});
    const wrapped=new Proxy(device,{get(target,key){if(key==='queue')return queue;
      if((dotMode||bokehMode)&&key==='createBuffer')return descriptor=>target.createBuffer({...descriptor,usage:descriptor.usage|((dotMode&&['Persistent warp-dot positions','GPU-generated warp-dot draw instances','Fading-dot scalar controls'].includes(descriptor.label)||bokehMode&&descriptor.label?.startsWith('Bokeh dots:')&&!(descriptor.usage&GPUBufferUsage.MAP_READ))?GPUBufferUsage.COPY_SRC:0)});
      const value=Reflect.get(target,key,target);return typeof value==='function'?value.bind(target):value;}});
    // Constructor consumes exactly this one shared-noise seed. Init's own RNG
    // starts separately so noise initialization can be compared in isolation.
    Math.random=()=>result.dynamic_seed/4294967296;const warp=new GPUWarp(wrapped,96,64,kWarpWGSL);
    Math.random=mulberry32(result.init_seed);warp.init();stage='frames';if(result.noise_files.length!==4)throw Error('Expected four captured Gaussian textures');
    if(dotMode)result.warp_dot_seed=warp.fading_dots.warp_dots.seed;
    const bokeh=warp.fading_dots.bokeh_dots;
    if(bokehMode){const palette=Float32Array.from({length:1024},(_,i)=>i%4===3?1:((i*19)%251)/100);result.bokeh_palette='bokeh-palette.f32le';result.files[result.bokeh_palette]=base64(palette);bokeh.set_bokeh_palette(palette);
      const overflowAccepted=bokeh.add_bokeh_dots([{x:1e300,y:0,z:.5}],96,64,0),packedFinite=Array.from(new Float32Array(bokeh.pending[0])).every(Number.isFinite);bokeh.pending.length=0;
      bokeh.initialize_lifetime_history(3.4028234663852886e38);await bokeh._read_bokeh_alive_count();result.reference_edge_cases={overflow_spawn_accepted:overflowAccepted,packed_spawn_finite:packedFinite,large_seed_mean_finite:Number.isFinite(bokeh.bokeh_average_lifespan)};
    }
    const mapSpecs=[[0,11],[3,7],[14,3],[24,9],[35,17],[37,19],[38,23],[39,29],[35,31],[37,37]],maps=[];
    for(let index=0;index<mapSpecs.length;index++){
      const [mode,seed]=mapSpecs[index],map=buildWarpMap(mode,seed);maps.push(map);const file='map-'+index+'.f32le',values=map.src_dxy||map.dynamic_motion_params;result.files[file]=base64(values);
      result.maps.push({mode,seed,file,dynamic:map.src_dxy===null,width:map.W,height:map.H});
    }
    const initial=new Uint16Array(96*64*4);for(let y=0;y<64;y++)for(let x=0;x<96;x++)for(let c=0;c<4;c++)initial[(y*96+x)*4+c]=0x2400+((x*37+y*71+c*617)%0x1800);
    result.initial_file='initial.f16le';result.files[result.initial_file]=base64(initial);
    for(const texture of warp.index_tex)device.queue.writeTexture({texture},initial,{bytesPerRow:96*8,rowsPerImage:64},{width:96,height:64});
    for(let frame=0;frame<16;frame++){
      let resize=null;
      if(frame===10){resize={width:64,height:96};warp.resize(resize.width,resize.height);}
      if(frame===13){resize={width:80,height:48};warp.resize(resize.width,resize.height);}
      const bindings=frame<4?[0,1,2,3,0,0,0,0]:frame===7?[0,1,2,3,4,8,6,7]:frame===9?[0,1,2,3,4,8,9,7]:[0,1,2,3,4,5,6,7];
      for(let slot=0;slot<8;slot++)warp.upload_warp_map(slot,maps[bindings[slot]]);
      const spectrum=Float32Array.from({length:512},(_,i)=>frame<4?0:.02+.7*Math.sin(i*.027+frame*.3)**2);warp.dynamic_warp.upload_spectrum(spectrum);
      const strengths=frame<4?[1.5,.6,.3,.7,0,0,0,0]:[.7,.2,.3,.1,1.5,.6,.4,1.2];
      const shader={rx_diff_grey_scott_str:frame>=3?.4:0,rx_diff_grey_scott_str2:0,diffusion_colorization:0,rx_diff_barkley_str:0,
        frequency_injector_str:frame>=6?.7:0,frequency_injector_freq_scale:1.1,painterly_str:frame>=8?.8:0,sharpen_str:0,noise_str:.2,layer2_motion_scale:.8};
      const params={shift_x:frame%3*.25,shift_y:-.2,darkening:frame<6?-.2:.3,true_time:frame*.17,oversample:frame<8?1:2,shader_params:shader};
      const waveform=dotMode&&frame%4!==0?Float32Array.from({length:64*4},(_,n)=>{const i=n>>2;return n%4===0?(i*17+frame*3)%warp.W:n%4===1?(i*11+frame*5)%warp.H:n%4===2?[.5,1,1.001,2,4,7][i%6]:.03+(i%7)*.04;}):new Float32Array();
      const dotParams={strength:frame<2||frame===6?.0005:.55,true_time:params.true_time,point_size:1+frame%5};if(dotMode)warp.set_fading_dots(dotParams);
      let bokehCase;
      if(bokehMode){
        const controls={true_time:params.true_time,focal_distance:frame%5*.25,output_width:frame<10?176:128,output_height:frame<10?96:160,opacity:.8},follow=Array.from({length:8},(_,i)=>frame<5?1:i%2?.3:.8),seed=frame===0?4:frame===8?2:null;
        bokeh.set_bokeh_frame(controls);bokeh.set_follow_motions(follow);if(seed!==null)bokeh.initialize_lifetime_history(seed);
        let historyReset=null;if(frame===14){historyReset={next_ticket:4294967280,sample_count:1024,processed_ticket:4294967280,write_offset:1007,seed_value:2};const bytes=new ArrayBuffer(16+1024*4);new Uint32Array(bytes,0,4).set([historyReset.next_ticket,1024,historyReset.processed_ticket,1007]);new Float32Array(bytes,16).fill(2);device.queue.writeBuffer(bokeh.bokeh_history_buf,0,bytes);}
        const calls=[];
        if([0,2,4,9,10,13,14].includes(frame)){
          const count=frame===4?4100:frame===9?4:frame===14?64:24,points=Array.from({length:count},(_,i)=>({x:frame===14||frame===0&&i>20?3:((i%64)+.5)/32-1,y:(Math.floor(i/64)+.5)/32-1,z:(i%11)/10,z2:1-(i%11)/10,use_background:i%3===0,palette_position:i/(count-1),fade_in:i%2?.05:.1,hold:i%2?.25:.2,fade_out:.1}));
          const birth=frame===9?params.true_time-2:params.true_time,accepted=bokeh.add_bokeh_dots(points,warp.W,warp.H,birth);calls.push({points,birth,accepted,pending:bokeh.pending.length});
        }
        bokehCase={controls,follow,seed,history_reset:historyReset,calls,background:[.2,.4,1.2],draws:[]};
      }
      for(let n=0;n<8;n++)params['w'+n]=strengths[n];warp.set_params(params);warp.step(waveform);
      const pixels=await readImage(device,warp.get_current_index_texture()),exposure=await readExposure(device,warp.exposure_buf),file='frame-'+frame+'.f16le';result.files[file]=base64(pixels);
      const item={frame,width:warp.W,height:warp.H,resize,bindings,strengths,shader,shift_x:params.shift_x,shift_y:params.shift_y,darkening:params.darkening,time:params.true_time,oversample:params.oversample,
        noise_x:capturedUniforms[20],noise_y:capturedUniforms[21],uniforms:capturedUniforms,spectrum:Array.from(spectrum),exposure,cache_slots:Array.from(warp.warp_slot_cache),file};
      if(dotMode){item.waveform=Array.from(waveform);item.dots=dotParams;
        for(const [field,buffer] of [['positions',warp.fading_dots.warp_dots.position_buf],['instances',warp.fading_dots.warp_dots.instance_buf],['controls',warp.fading_dots.warp_dots.control_buf]]){
          const name='dots-'+frame+'-'+field+'.'+(field==='controls'?'u8':'f32le');result.files[name]=base64(await readBuffer(device,buffer));item['dots_'+field]=name;}}
      if(bokehMode){
        await bokeh._read_bokeh_alive_count();bokehCase.stats={alive:bokeh.bokeh_alive_count,samples:bokeh.bokeh_lifetime_sample_count,mean:bokeh.bokeh_average_lifespan};
        for(const [field,buffer] of [['particles',bokeh.bokeh_dot_buf],['update_controls',bokeh.bokeh_update_buf],['spawns',bokeh.bokeh_spawn_buf],['history',bokeh.bokeh_history_buf],['events',bokeh.bokeh_death_events_buf],['alive',bokeh.bokeh_alive_count_buf]]){
          const name='bokeh-'+frame+'-'+field+'.u8';result.files[name]=base64(await readBuffer(device,buffer));bokehCase[field]=name;}
        for(const format of ['rgba16float','rgba8unorm','bgra8unorm']){
          const width=bokehCase.controls.output_width,height=bokehCase.controls.output_height,target=device.createTexture({size:[width,height],format,usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC}),encoder=device.createCommandEncoder();
          const pass=encoder.beginRenderPass({colorAttachments:[{view:target.createView(),clearValue:{r:.125,g:.25,b:.375,a:.75},loadOp:'clear',storeOp:'store'}]});bokeh.draw_bokeh(pass,format,{width,height,background:bokehCase.background});pass.end();device.queue.submit([encoder.finish()]);
          const file='bokeh-'+frame+'-'+format+'.'+(format==='rgba16float'?'f16le':'u8');result.files[file]=base64(await readImage(device,target));bokehCase.draws.push({format,file});target.destroy();}
        const name='bokeh-'+frame+'-draw-controls.u8';result.files[name]=base64(await readBuffer(device,bokeh.bokeh_draw_buf));bokehCase.draw_controls=name;item.bokeh=bokehCase;
      }
      result.frames.push(item);
      if(errors.length)throw Error(errors[0]);
    }
    result.cache_trace=[];
    for(let n=0;n<24;n++){
      const slot=n%8,map={...maps[0],src_dxy:maps[0].src_dxy.slice()};warp.upload_warp_map(slot,map);
      result.cache_trace.push({slot,slots:Array.from(warp.warp_slot_cache)});
    }
    await device.queue.onSubmittedWorkDone();device.destroy();return result;
  }catch(error){return {result:'fail',error:error.stack||String(error)};}
  finally{Math.random=originalRandom;if(device)device.destroy();}
};
