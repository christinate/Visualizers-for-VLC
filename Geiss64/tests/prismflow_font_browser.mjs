// Local development oracle. Original Canvas font rasterization is measured,
// never used as a runtime asset or a replacement for native font services.
import {GpuTextOverlay} from '/assets/gpu_text_overlay.js';
function base64(bytes){let s='';for(let n=0;n<bytes.length;n+=32768)s+=String.fromCharCode(...bytes.subarray(n,n+32768));return btoa(s);}
function inspect(bytes,width,height){let left=width,top=height,right=0,bottom=0,ink=0;for(let y=0;y<height;++y)for(let x=0;x<width;++x)if(bytes[(y*width+x)*4+3]){left=Math.min(left,x);top=Math.min(top,y);right=Math.max(right,x);bottom=Math.max(bottom,y);++ink;}return {nonzero_alpha:ink,alpha_bounds:ink?[left,top,right,bottom]:[]};}
window.runReference=async()=>{
  let device;
  try{
    const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});if(!adapter)throw Error('No font reference adapter');device=await adapter.requestDevice();
    const result={result:'pass',files:{},frames:[],adapter:{vendor:adapter.info.vendor,architecture:adapter.info.architecture,device:adapter.info.device,description:adapter.info.description}};
    const strings=['Prism Flow 64-bit','Prism • 日本語 🎵','مرحبا بالعالم','e\u0301 & café','Prism\tFlow\n64\rbit\f','Prism Flow 64 bit ','   ','','jgj AMW','                 AMW'];
    const overlay=new GpuTextOverlay(device);overlay.init();
    for(let n=0;n<strings.length;++n){
      const edge=n>=8,options={font_family:edge?'serif':'sans-serif',font_weight:edge?'normal':'bold',supersample:edge?3:2,padding_px:edge?0:4},fontPixels=edge?18.25:18;
      overlay.set_text(strings[n],fontPixels,options);if(!strings[n]){result.frames.push({text:'',width:0,height:0,nonzero_alpha:0,alpha_bounds:[]});continue;}
      const ctx=overlay.ctx,m=ctx.measureText(strings[n]),width=overlay.canvas.width,height=overlay.canvas.height,bytes=ctx.getImageData(0,0,width,height).data,file='font-'+n+'.u8';
      result.files[file]=base64(bytes);const frame={text:strings[n],fontPixels,options,width,height,file,metrics:{left:Math.ceil(m.actualBoundingBoxLeft),right:Math.ceil(m.actualBoundingBoxRight),ascent:Math.ceil(m.actualBoundingBoxAscent),descent:Math.ceil(m.actualBoundingBoxDescent),advance:m.width},...inspect(bytes,width,height)};
      // Independently redraw in a generous bitmap. If any ink lands outside
      // the original texture after translating back, clipping is proven.
      const margin=128,canvas=new OffscreenCanvas(width+2*margin,height+2*margin),guard=canvas.getContext('2d');guard.font=ctx.font;guard.fillStyle='#fff';guard.textAlign='left';guard.textBaseline='alphabetic';
      const pad=Math.max(1,Math.ceil(options.padding_px*options.supersample));guard.fillText(strings[n],margin+pad,margin+pad+frame.metrics.ascent);
      const complete=guard.getImageData(0,0,canvas.width,canvas.height).data;let lost=0;
      for(let y=0;y<canvas.height;++y)for(let x=0;x<canvas.width;++x)if((x<margin||x>=margin+width||y<margin||y>=margin+height)&&complete[(y*canvas.width+x)*4+3])++lost;
      frame.clipped_ink_pixels=lost;result.frames.push(frame);
    }
    await device.queue.onSubmittedWorkDone();return result;
  }catch(error){return {result:'fail',error:error.stack||String(error)};}finally{device?.destroy();}
};
