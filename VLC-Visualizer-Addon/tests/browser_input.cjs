const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.join(__dirname,'../browser/ui/input.mjs'),'utf8');
const code=source.slice(source.indexOf('async function refresh('),source.indexOf('async function stop('));
async function check(lineIn,enumerationFails){
 const devices={options:[],value:'',replaceChildren(...options){this.options=options;this.value=options[0].value;},add(option){this.options.push(option);}},status={textContent:''};let permissions=0;
 const refresh=vm.runInNewContext(code+';refresh',{devices,status,ready:{lineIn},Option:class{constructor(text,value){this.text=text;this.value=value;}},navigator:{mediaDevices:{enumerateDevices:async()=>{if(enumerationFails)throw Error('device enumeration denied');return [{kind:'audioinput',deviceId:'recording-1',label:''}];},getUserMedia:async()=>{permissions++;throw Error('microphone permission denied');}}}});
 await refresh();assert(devices.options.some(option=>option.value==='default'));assert(devices.options.some(option=>option.value==='__demo'));
 if(lineIn==='__demo'){assert.equal(devices.value,'__demo');assert.equal(permissions,0);}
 else if(!enumerationFails){assert.equal(devices.options.length,3);assert.equal(permissions,1);}
}
(async()=>{await check('',false);await check('__demo',false);await check('__demo',true);console.log(JSON.stringify({result:'pass',checks:['input list survives permission denial','demo never opens microphone','demo selected after enumeration failure']}));})().catch(error=>{console.error(error);process.exitCode=1;});
