// Local development oracle. Native execution never consults these pixels.
import {GeneratePaletteFromImage} from '/assets/image_palette.js';
function base64(bytes){let s='';for(let n=0;n<bytes.length;n+=32768)s+=String.fromCharCode(...bytes.subarray(n,n+32768));return btoa(s);}
function randomStream(seed){return ()=>{let t=seed+=0x6d2b79f5;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
window.runReference=async()=>{
  const result={result:'pass',files:{},frames:[],adapter:{type:'Canvas2D',color_space:'srgb'}};let captured;
  const getContext=HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext=function(...args){const context=getContext.apply(this,args);if(!context||args[0]!=='2d')return context;return new Proxy(context,{get(t,key){if(key==='getImageData')return (...values)=>{const image=t.getImageData(...values);captured={width:image.width,height:image.height,data:Uint8Array.from(image.data)};return image;};const value=Reflect.get(t,key,t);return typeof value==='function'?value.bind(t):value;}});};
  try{
    const fixtures=await (await fetch('/input/fixtures.json')).json();if(fixtures.cases.length>64)throw Error('Image reference corpus exceeds bound');
    for(const test of fixtures.cases){if(test.error){result.frames.push({input:test.file,skipped_native_bound_or_corrupt:true});continue;}captured=null;
      const blob=await (await fetch('/input/'+encodeURIComponent(test.file))).blob(),file=new File([blob],test.file),frame={input:test.file};
      try{frame.palette=await GeneratePaletteFromImage(file,{sample_size:test.side??128,random:randomStream(test.seed??17)});}catch(error){if(!test.transparent)throw error;frame.expected_transparent_error=String(error);}
      if(!captured)throw Error('Original image function did not return Canvas pixels');const name='image-'+result.frames.length+'.u8';result.files[name]=base64(captured.data);Object.assign(frame,{width:captured.width,height:captured.height,file:name});result.frames.push(frame);
    }
    // Browser-generated, local JPEG/WebP input fixtures avoid pulling unrelated
    // pictures or adding an image encoder to the native release runtime.
    const canvas=new OffscreenCanvas(24,16),context=canvas.getContext('2d'),colors=['#cc3300','#00cc33','#cccc00','#0033cc','#555555','#00cccc'];for(let y=0;y<2;++y)for(let x=0;x<3;++x){context.fillStyle=colors[y*3+x];context.fillRect(x*8,y*8,8,8);}
    for(const [type,name] of [['image/jpeg','encoded-jpeg.u8'],['image/webp','encoded-webp.u8']]){const blob=await canvas.convertToBlob({type,quality:1});if(blob.type===type)result.files[name]=base64(new Uint8Array(await blob.arrayBuffer()));}
    return result;
  }catch(error){return {result:'fail',error:error.stack||String(error)};}finally{HTMLCanvasElement.prototype.getContext=getContext;}
};
