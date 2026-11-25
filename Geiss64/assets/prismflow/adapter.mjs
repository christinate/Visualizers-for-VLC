// SPDX-License-Identifier: Apache-2.0
// Local host adapter. Derived engine files retain Ryan Geiss's NOTICE.
import {WebGPUPresenter} from './webgpu_present.js';
import {Engine,JumpToNextPreset,JumpToPrevPreset} from './engine.js';
import {kMaxActiveWarps} from './const.js';
export class Renderer{
  constructor(canvas,settings){this.canvas=canvas;this.settings=settings;this.frame=0;this.time=1000;this.locked=false;this.spectrum=new Float32Array(512);this.random=[false,false,false,false,false];this.calibration={calibrating_hdr_headroom:0,selected_square_index:0,width_frac:.8};}
  dimensions(){const width=this.canvas.width,height=this.canvas.height;const wanted=this.settings.supersampling||1;this.ss=width*height*wanted*wanted<=16777216?wanted:1;return [width,height,Math.round(width*this.ss),Math.round(height*this.ss)];}
  async init(){
    const [w,h,iw,ih]=this.dimensions();this.presenter=new WebGPUPresenter(this.canvas,w,h,iw,ih,this.ss);
    await this.presenter.init(this.settings.hdr&&matchMedia('(dynamic-range: high)').matches);
    this.engine=new Engine(this.presenter,w,h,iw,ih);
    return {engine:'Prism Flow — derived from Geiss HDR',adapter:this.presenter.adapterInfo,precision:'rgba16float',feedbackFormats:this.presenter.gpu_warp.index_tex.map(texture=>texture.format),maxActiveMotions:kMaxActiveWarps,supersampling:this.ss,internalSize:[iw,ih],canvasSize:[w,h],format:this.presenter.format,hdrActive:this.presenter.use_hdr&&matchMedia('(dynamic-range: high)').matches};
  }
  resize(){const [w,h,iw,ih]=this.dimensions();this.presenter.resize(w,h,iw,ih,this.ss);this.engine.resize(w,h,iw,ih);}
  render(audio,dt,time){
    if(!this.locked)this.time+=dt;const beat=audio.beat;
    for(let n=0;n<512;++n)this.spectrum[n]=(audio.spectrum[n*2]+audio.spectrum[n*2+1])*.5;
    this.engine.render(this.time,this.time,time,this.frame++,false,...this.random,null,1,
      1/Math.max(dt,.001),1,1.5,0,beat,1,false,1,
      this.presenter.use_hdr?this.settings.peak/203:1,0,'',1,1,
      false,false,false,false,false,false,audio,this.calibration,-1,this.spectrum,'this_is_the_last_param');
    this.random.fill(false);this.presenter.draw(0,[-1,-1,-1]);
  }
  key(key){if(key===' '||key==='Enter')this.time=JumpToNextPreset(this.time,key==='Enter');else if(key==='Backspace')this.time=JumpToPrevPreset(this.time);else if(key.toLowerCase()==='l')this.locked=!this.locked;else{const index=['m','p','w','d','s'].indexOf(key.toLowerCase());if(index>=0)this.random[index]=true;}}
  async done(){await this.presenter.device.queue.onSubmittedWorkDone();}
  dispose(){this.presenter?.device.destroy();}
}
