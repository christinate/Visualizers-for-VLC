export class MusicAnalysis{
  constructor(){this.wave=new Float32Array(2048);this.spectrum=new Float32Array(1024);this.real=new Float32Array(2048);this.imag=new Float32Array(2048);this.average=.001;this.lastBeat=-10;this.beats=0;this.envelope=0;this.lastTime=0;}
  update(stereo,time){
    let squares=0,energy=0,previous=0;
    for(let n=0;n<2048;++n){const sample=((stereo[n*2]||0)+(stereo[n*2+1]||0))*.5;this.wave[n]=sample;squares+=sample*sample;energy+=Math.abs(sample-previous);previous=sample;this.real[n]=sample*(.5-.5*Math.cos(n*Math.PI*2/2047));this.imag[n]=0;}
    for(let n=1,j=0;n<2048;++n){let bit=1024;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(n<j)[this.real[n],this.real[j]]=[this.real[j],this.real[n]];}
    for(let size=2;size<=2048;size*=2){const angle=-Math.PI*2/size;for(let start=0;start<2048;start+=size)for(let j=0;j<size/2;++j){const c=Math.cos(angle*j),s=Math.sin(angle*j),a=start+j,b=a+size/2,real=this.real[b]*c-this.imag[b]*s,imag=this.real[b]*s+this.imag[b]*c;this.real[b]=this.real[a]-real;this.imag[b]=this.imag[a]-imag;this.real[a]+=real;this.imag[a]+=imag;}}
    let bass=0;for(let n=0;n<1024;++n){const value=(this.real[n]**2+this.imag[n]**2)/1048576;this.spectrum[n]=value*(n*44100/2048);if(n>=2&&n<=9)bass+=value;}
    const dt=Math.min(.1,Math.max(.001,time-this.lastTime));this.lastTime=time;const baseline=this.average;this.average+= (bass-this.average)*(1-Math.exp(-dt/1.5));let beat=false;
    if(bass>Math.max(.00003,baseline*1.55)&&time-this.lastBeat>.28){beat=true;this.lastBeat=time;this.beats++;this.envelope=1;}
    this.envelope*=Math.exp(-dt*4);
    return {wave:this.wave,spectrum:this.spectrum,rms:Math.sqrt(squares/2048),visual_energy:energy/2047,beat:beat?1:0,beatEnvelope:this.envelope,beatCount:this.beats,bass};
  }
}
