const api=window.visualizer,devices=document.querySelector('#devices'),status=document.querySelector('#status');
let stream=null,context=null,node=null,demoTimer=null,demoTime=0,demoBusy=false;
const ready=await api.captureReady();
async function refresh(){
  devices.replaceChildren(new Option('Default recording input','default'),new Option('Demo rhythm','__demo'));
  try{
    let available=await navigator.mediaDevices.enumerateDevices();
    if(!['default','__demo'].includes(ready.lineIn)&&available.some(device=>device.kind==='audioinput')&&!available.some(device=>device.kind==='audioinput'&&device.label)){
      try{
        const permission=await navigator.mediaDevices.getUserMedia({audio:true,video:false});
        try{available=await navigator.mediaDevices.enumerateDevices();}finally{for(const track of permission.getTracks())track.stop();}
      }catch(error){status.textContent='Input names are unavailable: '+error.message;}
    }
    let index=0;for(const device of available)if(device.kind==='audioinput'&&device.deviceId!=='default')devices.add(new Option(device.label||'Recording input '+(++index),device.deviceId));
  }catch(error){status.textContent=error.message;}
  const requested=ready.lineIn;
  if(requested&&requested!=='default'){
    const matches=[...devices.options].filter(option=>option.value===requested||option.text.toLowerCase()===requested.toLowerCase());
    if(matches.length===1)devices.value=matches[0].value;
    else status.textContent='The requested input is unavailable. Select an input from the list.';
  }
}
async function stop(){if(demoTimer)clearInterval(demoTimer);demoTimer=null;if(node)node.disconnect();if(stream)for(const track of stream.getTracks())track.stop();stream=null;node=null;if(context)await context.close();context=null;}
async function use(){
  document.querySelector('#use').disabled=true;status.textContent='';
  try{
    const selected=devices.value;
    if(selected==='__demo'){
      await stop();const samples=new Float32Array(1024);demoTime=0;
      demoTimer=setInterval(async()=>{if(demoBusy)return;demoBusy=true;for(let n=0;n<512;++n){const t=demoTime+n/44100,pulse=Math.exp(-((t%0.5)/0.5)*10),amplitude=.08+.7*pulse;samples[n*2]=Math.sin(t*Math.PI*2*110)*amplitude;samples[n*2+1]=Math.sin(t*Math.PI*2*165)*amplitude;}demoTime+=512/44100;try{await api.writeAudio(samples,44100);}finally{demoBusy=false;}},12);
    }else{
      const next=await navigator.mediaDevices.getUserMedia({video:false,audio:{deviceId:selected==='default'?undefined:{exact:selected},echoCancellation:false,noiseSuppression:false,autoGainControl:false,channelCount:{ideal:2}}});
      let audio;
      try{
      audio=new AudioContext({sampleRate:44100});await audio.audioWorklet.addModule('capture-worklet.js');
      const capture=new AudioWorkletNode(audio,'local-capture');const source=audio.createMediaStreamSource(next),silent=audio.createGain();silent.gain.value=0;
      capture.port.onmessage=async event=>{try{await api.writeAudio(event.data,audio.sampleRate);}catch(error){status.textContent=error.message;}finally{capture.port.postMessage('ack');}};
      source.connect(capture);capture.connect(silent).connect(audio.destination);await audio.resume();await stop();stream=next;context=audio;node=capture;
      }catch(error){for(const track of next.getTracks())track.stop();if(audio)await audio.close();throw error;}
    }
    await api.captureSelected(selected,devices.selectedOptions[0].text);
  }catch(error){status.textContent=error.name+': '+error.message;}finally{document.querySelector('#use').disabled=false;}
}
document.querySelector('#use').onclick=use;document.querySelector('#refresh').onclick=refresh;document.querySelector('#cancel').onclick=()=>api.captureCancel();
window.addEventListener('keydown',event=>{if(event.key==='Escape')api.captureCancel();});window.addEventListener('beforeunload',stop);
await refresh();if(ready.lineIn==='default'||[...devices.options].some(option=>option.value===ready.lineIn||option.text.toLowerCase()===ready.lineIn?.toLowerCase()))await use();
