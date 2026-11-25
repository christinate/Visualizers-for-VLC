import {MusicAnalysis} from './audio.mjs';
const api=window.visualizer,config=await api.config(),canvas=document.querySelector('#canvas'),error=document.querySelector('#error');
const analysis=new MusicAnalysis(),demo=new Float32Array(4096);let renderer,frames=0,last=0,started=performance.now(),lastReport=0,resized=true,finished=false;
function dimensions(){const ratio=devicePixelRatio||1;canvas.width=Math.max(1,Math.round(innerWidth*ratio));canvas.height=Math.max(1,Math.round(innerHeight*ratio));}
function fail(value){error.style.display='block';error.textContent=String(value)+'\n\nEscape or right-click closes this window.';api.report({failure:String(value)}).catch(()=>{});}
window.addEventListener('contextmenu',event=>{event.preventDefault();api.close();});
window.addEventListener('dblclick',event=>{event.preventDefault();api.fullscreen();});
window.addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();api.close();}else if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='l'){event.preventDefault();if(!config.owner)api.chooseAudio();}else renderer?.key?.(event.key);});
window.addEventListener('resize',()=>{resized=true;});
api.onSourceChanged(value=>{config.demo=value.demo;});
window.addEventListener('beforeunload',()=>{finished=true;renderer?.dispose();});
try{
  dimensions();const module=await import(location.href.replace(/index\.html.*$/,'adapter.mjs'));renderer=new module.Renderer(canvas,config.settings);
  const info=await renderer.init();await api.report({ready:true,...info,kind:config.id,secureContext:isSecureContext});
  let previousTime=0;
  async function frame(timestamp){
    if(finished)return;if(timestamp-last<1000/config.settings.fps){requestAnimationFrame(frame);return;}
    last=timestamp;const time=(timestamp-started)*.001,dt=Math.min(.1,Math.max(.001,time-previousTime));previousTime=time;
    try{
      if(resized){await renderer.done();dimensions();renderer.resize();resized=false;}
      let samples;
      if(config.demo){
        for(let n=0;n<2048;++n){const t=time+n/44100,pulse=Math.exp(-((t%.5)/.5)*10),a=.08+.7*pulse;demo[n*2]=a*Math.sin(t*Math.PI*2*110);demo[n*2+1]=a*Math.sin(t*Math.PI*2*165);}samples=demo;
      }else samples=await api.audio();
      const audio=analysis.update(samples,time);renderer.render(audio,dt,time);await renderer.done();frames++;
      if(time-lastReport>=5){lastReport=time;await api.report({frames,time,beats:audio.beatCount,rms:audio.rms,camera:renderer.cameraMode,source:config.demo?'demo':'PCM'});}
      requestAnimationFrame(frame);
    }catch(value){fail(value.stack||value);}
  }
  requestAnimationFrame(frame);
}catch(value){fail(value.stack||value);}
