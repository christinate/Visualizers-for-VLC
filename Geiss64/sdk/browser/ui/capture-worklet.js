class LocalCapture extends AudioWorkletProcessor{
  constructor(){super();this.buffer=new Float32Array(1024);this.offset=0;this.pending=false;this.port.onmessage=()=>{this.pending=false;};}
  process(inputs){
    const input=inputs[0];if(!input?.length)return true;const left=input[0],right=input[1]||left;
    for(let n=0;n<left.length;++n){this.buffer[this.offset++]=left[n];this.buffer[this.offset++]=right[n];if(this.offset===this.buffer.length){if(!this.pending){this.pending=true;this.port.postMessage(this.buffer);}this.offset=0;}}
    return true;
  }
}
registerProcessor('local-capture',LocalCapture);
