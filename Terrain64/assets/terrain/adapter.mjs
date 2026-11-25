// SPDX-License-Identifier: MIT
const encodeBuffer=new ArrayBuffer(4),encodeView=new DataView(encodeBuffer);
export function half(value){encodeView.setFloat32(0,value,false);const bits=encodeView.getUint32(0,false),sign=(bits>>>16)&0x8000,exp=((bits>>>23)&255)-127+15,mant=bits&0x7fffff;if(exp<=0)return sign;if(exp>=31)return sign|0x7bff;return sign|Math.min(0x7bff,(exp<<10)+((mant+0x1000)>>>13));}
function mix(a,b,t){return a.map((value,n)=>value+(b[n]-value)*t);}
export class Renderer{
  constructor(canvas,settings){this.canvas=canvas;this.settings=settings;this.row=0;this.lastHistory=-1;this.head=0;this.hue=0;this.lastBeat=0;this.cameraMode='above';this.rowData=new Uint16Array(1024);this.uniformData=new Float32Array(20);}
  async init(){
    if(!navigator.gpu)throw Error('WebGPU is unavailable');this.adapter=await navigator.gpu.requestAdapter();if(!this.adapter)throw Error('No compatible WebGPU adapter');
    this.device=await this.adapter.requestDevice();this.device.addEventListener('uncapturederror',event=>{throw Error(event.error.message);});
    this.context=this.canvas.getContext('webgpu');this.hdr=this.settings.hdr&&matchMedia('(dynamic-range: high)').matches;
    this.format=this.hdr?'rgba16float':navigator.gpu.getPreferredCanvasFormat();this.context.configure({device:this.device,format:this.format,alphaMode:'opaque',...(this.hdr?{toneMapping:{mode:'extended'}}:{})});
    const code=await (await fetch(new URL('./terrain.wgsl',import.meta.url))).text(),module=this.device.createShaderModule({code});
    const errors=(await module.getCompilationInfo()).messages.filter(message=>message.type==='error');if(errors.length)throw Error(errors.map(value=>value.message).join('\n'));
    this.pipeline=await this.device.createRenderPipelineAsync({layout:'auto',vertex:{module,entryPoint:'vertex'},fragment:{module,entryPoint:'fragment',targets:[{format:this.format}]},primitive:{topology:'triangle-list'}});
    this.uniform=this.device.createBuffer({size:80,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
    this.history=this.device.createTexture({size:[256,128],format:'rgba16float',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
    const sampler=this.device.createSampler({magFilter:'linear',minFilter:'linear',addressModeV:'repeat',addressModeU:'clamp-to-edge'});
    this.bind=this.device.createBindGroup({layout:this.pipeline.getBindGroupLayout(0),entries:[{binding:0,resource:{buffer:this.uniform}},{binding:1,resource:this.history.createView()},{binding:2,resource:sampler}]});
    return {engine:'Rhythm Terrain',precision:'rgba16float waveform history',format:this.format,hdrActive:this.hdr,cameraModes:['above','under','side','valley'],cameraSeconds:this.settings.cameraSeconds,colorBeats:this.settings.colorBeats};
  }
  resize(){}
  camera(mode,z,time){
    const center=Math.sin(z*.055)*3,front=Math.sin((z+24)*.055)*3;
    if(mode===0)return {eye:[center+12,22,z],look:[front,0,z+24]};
    if(mode===1)return {eye:[center-5,-18,z],look:[front,-7,z+24]};
    if(mode===2)return {eye:[34,6,z],look:[front,0,z+20]};
    return {eye:[center,2+Math.sin(z*.04)*.5,z],look:[front,2+Math.sin((z+24)*.04)*.5,z+24]};
  }
  render(audio,dt,time){
    const tick=Math.floor(time*30);
    if(tick!==this.lastHistory){this.lastHistory=tick;
      for(let n=0;n<256;++n){const wave=audio.wave[n*8];this.rowData[n*4]=half(wave);this.rowData[n*4+1]=half(Math.min(2,audio.beatEnvelope*.9+audio.rms));this.rowData[n*4+2]=half(Math.abs(audio.wave[(n*8+256)%2048]));this.rowData[n*4+3]=half(audio.rms);}
      this.device.queue.writeTexture({texture:this.history,origin:[0,this.row]},this.rowData,{bytesPerRow:2048},{width:256,height:1});this.head=(this.row+.5)/128;this.row=(this.row+1)%128;
    }
    if(audio.beatCount-this.lastBeat>=this.settings.colorBeats){this.hue=(this.hue+.137)%1;this.lastBeat=audio.beatCount;}
    const period=this.settings.cameraSeconds,cycle=time/period,current=Math.floor(cycle)%4,next=(current+1)%4,phase=cycle%1;
    const amount=Math.max(0,(phase-.85)/.15),smooth=amount*amount*(3-2*amount),z=time*3;
    const from=this.camera(current,z,time),to=this.camera(next,z,time),eye=mix(from.eye,to.eye,smooth),look=mix(from.look,to.look,smooth);
    this.cameraMode=['above','under','side','valley'][current];
    const u=this.uniformData;u.set([this.canvas.width,this.canvas.height,time,this.head]);u.set([...eye,0],4);u.set([...look,0],8);u.set([audio.beatEnvelope,audio.rms,audio.beatCount,0],12);u.set([this.hue,this.hdr?1:0,this.settings.peak/203,0],16);this.device.queue.writeBuffer(this.uniform,0,u);
    const encoder=this.device.createCommandEncoder(),pass=encoder.beginRenderPass({colorAttachments:[{view:this.context.getCurrentTexture().createView(),clearValue:[0,0,0,1],loadOp:'clear',storeOp:'store'}]});pass.setPipeline(this.pipeline);pass.setBindGroup(0,this.bind);pass.draw(3);pass.end();this.device.queue.submit([encoder.finish()]);
  }
  key(key){if(key.toLowerCase()==='c')this.settings.cameraSeconds=Math.max(30,this.settings.cameraSeconds);if(key===' ')this.hue=(this.hue+.137)%1;}
  async done(){await this.device.queue.onSubmittedWorkDone();}
  dispose(){this.history?.destroy();this.uniform?.destroy();this.device?.destroy();}
}
