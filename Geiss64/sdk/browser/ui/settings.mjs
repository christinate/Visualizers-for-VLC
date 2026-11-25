const api=window.visualizer,config=await api.config(),form=document.querySelector('form');
document.querySelector('#name').textContent=config.name+' settings';
for(const element of form.elements)if(element.name){if(element.type==='checkbox')element.checked=!!config.settings[element.name];else element.value=config.settings[element.name]??'';}
document.querySelector('#prism').hidden=config.id!=='geiss';document.querySelector('#terrain').hidden=config.id!=='terrain';document.querySelector('#modern').hidden=config.id!=='geiss';
if(config.id==='milkdrop'&&config.platform!=='win32'){form.elements.hdr.disabled=true;form.elements.peak.disabled=true;document.querySelector('#note').textContent='The native projectM backend currently presents SDR. Display settings apply on next launch.';}
document.querySelector('#cancel').onclick=()=>api.close();
form.onsubmit=async event=>{event.preventDefault();if(!form.reportValidity())return;const values={...config.settings};for(const element of form.elements)if(element.name)values[element.name]=element.type==='checkbox'?element.checked:element.type==='number'||element.name==='supersampling'?Number(element.value):element.value;await api.saveSettings(values);};
