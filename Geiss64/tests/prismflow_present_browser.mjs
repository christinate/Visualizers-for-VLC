// Local development oracle. The server appends only an export for shaderWGSL;
// original methods/shaders remain unchanged. No real canvas/HDR claim is made.
import {WebGPUPresenter,shaderWGSL} from '/assets/webgpu_present.js';
import {PaletteOverlayPass} from '/assets/draw_palette.js';
function base64(view){const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);let string='';for(let n=0;n<bytes.length;n+=32768)string+=String.fromCharCode(...bytes.subarray(n,n+32768));return btoa(string);}
async function readImage(device,texture){
  const pixelBytes=texture.format==='rgba16float'?8:4,width=texture.width,height=texture.height,row=Math.ceil(width*pixelBytes/256)*256;
  const buffer=device.createBuffer({size:row*height,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ}),encoder=device.createCommandEncoder();
  encoder.copyTextureToBuffer({texture},{buffer,bytesPerRow:row,rowsPerImage:height},{width,height});device.queue.submit([encoder.finish()]);await buffer.mapAsync(GPUMapMode.READ);
  const mapped=new Uint8Array(buffer.getMappedRange()),result=new Uint8Array(width*height*pixelBytes);for(let y=0;y<height;y++)result.set(mapped.subarray(y*row,y*row+width*pixelBytes),y*width*pixelBytes);buffer.unmap();buffer.destroy();return result;
}
window.runReference=async()=>{
  let device;const result={result:'pass',frames:[],files:{}};
  try{
    const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('No presentation reference adapter');const info=adapter.info;
    result.adapter={vendor:info?.vendor,architecture:info?.architecture,device:info?.device,description:info?.description};device=await adapter.requestDevice();const errors=[];
    device.addEventListener('uncapturederror',event=>errors.push(event.error.message));device.lost.then(info=>{if(info.reason!=='destroyed')errors.push('Device lost: '+info.message);});
    let capturedUniforms,capturedUi,capturedPacked;
    const queue=new Proxy(device.queue,{get(target,key){
      if(key==='writeBuffer')return (buffer,offset,data,...rest)=>{const floats=data instanceof Float32Array?data:new Float32Array(data);if(floats.length===24)capturedUniforms=Array.from(floats);if(floats.length===12)capturedUi=Array.from(floats);return target.writeBuffer(buffer,offset,data,...rest);};
      if(key==='writeTexture')return (destination,data,...rest)=>{if(destination.texture.width===256&&destination.texture.height===1&&destination.texture.format==='rgba8unorm')capturedPacked=new Uint8Array(data).slice();return target.writeTexture(destination,data,...rest);};
      const value=Reflect.get(target,key,target);return typeof value==='function'?value.bind(target):value;
    }});
    const wrapped=new Proxy(device,{get(target,key){if(key==='queue')return queue;const value=Reflect.get(target,key,target);return typeof value==='function'?value.bind(target):value;}});
    const titleBytes=new Uint8Array(16*8*4);for(let y=0;y<8;y++)for(let x=0;x<16;x++)titleBytes[(y*16+x)*4+3]=(x*19+y*31)%256;
    result.title_file='title.u8';result.files[result.title_file]=base64(titleBytes);const title=device.createTexture({size:[16,8],format:'rgba8unorm',usage:GPUTextureUsage.COPY_DST|GPUTextureUsage.TEXTURE_BINDING});
    device.queue.writeTexture({texture:title},titleBytes,{bytesPerRow:64},{width:16,height:8});
    const sampler=device.createSampler({minFilter:'linear',magFilter:'linear',mipmapFilter:'nearest'});
    for(let scene=0;scene<8;scene++){
      const ss=[1,2,3,.5,1,2,2,1][scene],width=[160,96,64,160,352,352,160,896][scene],height=[96,64,96,96,112,112,96,192][scene];
      const iw=width*ss,ih=height*ss,indices=new Uint16Array(iw*ih*4),palette=new Float32Array(1024);
      for(let y=0;y<ih;y++)for(let x=0;x<iw;x++)for(let c=0;c<4;c++)indices[(y*iw+x)*4+c]=0x1800+((x*47+y*131+c*1789+scene*977)%0x2400);
      for(let n=0;n<1024;n++)palette[n]=Math.fround(((n*31+scene*107)%1200)/300-.25);
      const inputFile='indices-'+scene+'.f16le',paletteFile='palette-'+scene+'.f32le',packedFile='packed-'+scene+'.u8';result.files[inputFile]=base64(indices);result.files[paletteFile]=base64(palette);
      const input=device.createTexture({size:[iw,ih],format:'rgba16float',usage:GPUTextureUsage.COPY_DST|GPUTextureUsage.TEXTURE_BINDING});device.queue.writeTexture({texture:input},indices,{bytesPerRow:iw*8},{width:iw,height:ih});
      const background=[.1+scene*.02,.25,.7],shader={layer1_density:1.4+scene*.1,layer2_brightness:.1+scene*.03};
      const rect=scene===6?{left_px:-6,top_px:24,width_px:220,height_px:96,intensity:.3}:null;
      const calibration={calibrating_hdr_headroom:scene===7?1:0,selected_square_index:6,width_frac:.85},swatches=scene===4?3:scene===5?8:0,swatchBackground=scene===4?background:null;
      for(const format of ['rgba16float','rgba8unorm','bgra8unorm']){
        capturedUniforms=null;capturedUi=null;capturedPacked=null;
        const target=device.createTexture({size:[width,height],format,usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC}),presenter=new WebGPUPresenter(null,width,height,iw,ih,ss);
        presenter.device=wrapped;presenter.format=format;presenter.context={getCurrentTexture:()=>target};presenter.uniform_buf=device.createBuffer({size:256,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
        presenter.paletteTex=device.createTexture({size:[256,1],format:'rgba8unorm',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
        const module=device.createShaderModule({code:shaderWGSL});presenter.pipeline=device.createRenderPipeline({layout:'auto',vertex:{module,entryPoint:'vs_main'},fragment:{module,entryPoint:'fs_main',targets:[{format}]},primitive:{topology:'triangle-list'}});
        presenter.gpu_warp={get_overlay_texture_view:()=>title.createView(),get_overlay_sampler:()=>sampler,get_overlay_rect:()=>rect};presenter.bokeh_calibrating=true;
        presenter.palette_overlay=new PaletteOverlayPass(wrapped,format,presenter.paletteTex);presenter.BuildBindGroup(input.createView());
        // The original palette upload also forwards to bokeh. This sink only
        // removes that unrelated forwarding; its actual upload method runs.
        presenter.gpu_warp.fading_dots={bokeh_dots:{set_bokeh_palette:()=>{}}};
        presenter.uploadPaletteRGBA8UNorm(palette);presenter.updateGlobals(0,shader,calibration,...background,1);presenter.draw(swatches,swatchBackground);
        const file='present-'+scene+'-'+format+'.'+(format==='rgba16float'?'f16le':'u8');result.files[file]=base64(await readImage(device,target));
        if(!capturedPacked||!capturedUniforms)throw Error('Missing original upload/uniform capture');result.files[packedFile]=base64(capturedPacked);
        result.frames.push({scene,width,height,index_width:iw,index_height:ih,oversample:ss,format,file,input_file:inputFile,palette_file:paletteFile,packed_file:packedFile,background,shader,title:rect,calibration,swatches,swatch_background:swatchBackground,uniforms:capturedUniforms,ui_uniforms:capturedUi});
        target.destroy();presenter.paletteTex.destroy();presenter.uniform_buf.destroy();presenter.palette_overlay.ui_ubo.destroy();
      }input.destroy();
    }
    await device.queue.onSubmittedWorkDone();if(errors.length)throw Error(errors.slice(0,8).join('\n'));return result;
  }catch(error){return {result:'fail',error:error.stack||String(error)};}finally{if(device)device.destroy();}
};
