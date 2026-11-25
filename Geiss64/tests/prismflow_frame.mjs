// Local reference oracle: execute the preserved engine, with GPU calls recorded.
// No application/audio data is uploaded; these are generated signal fixtures.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const assets=path.join(root,'assets/prismflow'),modules=new Map(),hashes={};
function moduleUrl(file){
  if(modules.has(file))return modules.get(file);
  const bytes=fs.readFileSync(path.join(assets,file));hashes[file]=crypto.createHash('sha256').update(bytes).digest('hex');
  let source=bytes.toString('utf8');
  if(file==='engine.js'){
    const marker='const repacked_points = new Float32Array(dots);';
    if(!source.includes(marker))throw new Error('Engine observation marker changed');
    source=source.replace(marker,`__observed={palette:Array.from(palette_to_upload),background:Array.from(bkg_color_adapted_to_headroom),
      wave:blended_wave_params,dots:blended_dot_params,shader:blended_shader_params,motion:blended_motion_params,
      strengths:Array.from(gpu_warp_str),follow:Array.from(follow_motions),shiftX:shift_x,shiftY:shift_y,
      pointSize:wave_point_size_in_pixels,amplitude:wave_ampl_scale,autoDarkening:g_auto_darkening,
      darkening:darkening+g_auto_darkening,energy:g_visual_energy_att,points:Array.from(new Float32Array(dots))};\n`+marker);
    source+=`\nlet __observed;export const frameTest={
      set(rows,centered){g_presets.splice(0,g_presets.length,...rows);g_visual_energy_att=0;g_frozen_palette=null;g_title_x=g_title_y=-1;
        Object.assign(aligner,new WaveAligner());if(centered)aligner._reinit(centered);},
      observed(){return __observed;},active:GetActivePresets};\n`;
  }
  source=source.replace(/from\s+(['"])\.\/([^'"]+)\1/g,(_,quote,dependency)=>'from '+JSON.stringify(moduleUrl(dependency)));
  const url='data:text/javascript;base64,'+Buffer.from(source).toString('base64');modules.set(file,url);return url;
}
function mulberry32(seed){return ()=>{seed=(seed+0x6D2B79F5)>>>0;let t=seed;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return ((t^t>>>14)>>>0)/4294967296;};}
globalThis.Worker=class {postMessage(){throw new Error('Unexpected background work in controlled frame oracle');}};
const output=path.resolve(process.argv[3]||path.join(root,'build/prism-frame-reference'));fs.mkdirSync(output,{recursive:true});
const originalRandom=Math.random;let activeRandom=mulberry32(1);Math.random=()=>activeRandom();
const groups=[];const waveform=[];
try{
  const {Engine,frameTest}=await import(moduleUrl('engine.js'));
  const {Blur,WaveAligner}=await import(moduleUrl('wave_align.js'));
  const {linearResampleF}=await import(moduleUrl('resample.js'));
  const sample=(n,phase=0)=>Array.from({length:n},(_,i)=>Math.fround(.6*Math.sin(i*.037+phase)+.17*Math.cos(i*.113-phase)));
  for(const n of [0,1,2,3,47,96,193,511,2048,4096]){
    const input=Float32Array.from(sample(n));
    for(const radius of [0,1,2,12,4096])waveform.push({operation:'blur',input:Array.from(input),radius,expected:Array.from(Blur(input,radius))});
    for(const count of [0,1,2,13,95,512,2048,8192])waveform.push({operation:'resample',input:Array.from(input),count,expected:Array.from(linearResampleF(input,count))});
  }
  for(const n of [1,2,47,97,511,2048,4096])for(const fraction of [1,.55,.85]){
    if(Math.floor(n*fraction)===0)continue;const align=new WaveAligner();align._reinit(n);
    const frames=[];for(let k=0;k<6;k++){const input=Float32Array.from(sample(n,k*.7));frames.push({input:Array.from(input),expected:Array.from(align.alignFromF(input,fraction,n))});}
    waveform.push({operation:'align',n,fraction,frames});
  }
  const faultInput=Float32Array.from(sample(2048)),faultAlign=new WaveAligner(),faultOutput=faultAlign.alignFromF(faultInput,.55,2048);
  const startupFault={sourceOffset:0,correctOffset:Math.floor((2048-Math.floor(2048*.55))/2),firstSample:faultOutput[0],sourceFirstSample:faultInput[0]};
  function row(index){
    const wave={wave_flat_str:1,wave_flat_angle:index===0?3.1:-3.05,wave_flat_scale:.8,wave_flat_is_stereo:index*.5,wave_flat_stereo_sep:.3,wave_flat_stereo_amplitude_scale:.7,wave_flat_cx:.13,wave_flat_cy:-.17,
      wave_circ_str:.7,wave_circ_rad:1.2,wave_circ_scale:.9,wave_circ_angle:.2+index,wave_circ_cx:.05,wave_circ_cy:-.07};
    const dots={radial_beat_dots:.8,random_beat_dots:.7,fading_dots:.6,grid_dots:.8,chaser_1:.9,chaser_2:.5,dotty_chaser:.7,stars_opacity:.8,stars_population:3+index};
    const motion={blended_net_motion:1,blended_net_zoom_motion:1.7+index*.1,blended_in_or_out_motion:.3,blended_net_clockwise_motion:.4,blended_cw_or_ccw_motion:.5,blended_motion_cx:.1,blended_motion_cy:-.2};
    const shader={rx_diff_grey_scott_str:.2,rx_diff_grey_scott_str2:.3,rx_diff_barkley_str:.1,frequency_injector_str:.4,frequency_injector_freq_scale:.7,painterly_str:.5,sharpen_str:.6,noise_str:.2,layer2_warp_scale:.7,layer1_density:.8,layer2_brightness:.9,layer2_motion_scale:1.1};
    for(const object of [motion,shader])for(const key of Object.keys(object))object[key]*=1+index*.13;
    return {global_preset_number:index,t1:[900,1004,1100][index],t2:[901,1008,1106][index],warp_maps_ready:true,
      warps:Array.from({length:4},(_,j)=>({str:.17+j*.23,warp_map:{mode:j,warp_prefs:{stars_follow_motion:.2+j*.17},breathing_str:.15+j*.1,breathing_freq:.2+j*.1,breathing_phase:.5*j}})),
      wave_params:wave,dot_params:dots,motion_params:motion,shader_params:shader,palette_orig:{name:'generated-'+index,bkg:[.2+index*.1,.4,.7]},
      palette_expanded:Array.from({length:1024},(_,i)=>Math.fround(i%4===3?1:((i*17+index*79)%512)/170))};
  }
  const dimensions=[[192,128],[129,257],[512,256],[4096,2048]];
  for(let group=0;group<12;group++){
    const rows=[row(0),row(1),row(2)], [width,height]=dimensions[group%4],fraction=group>=8?.55:1;
    if(group===4)for(const r of rows)Object.assign(r.dot_params,{stars_opacity:0,radial_beat_dots:0,random_beat_dots:0});
    if(group===5)for(const r of rows)Object.assign(r.wave_params,{wave_flat_str:0,wave_circ_str:0});
    const data={name:'frame-group-'+group,rows:structuredClone(rows),frames:[]};groups.push(data);
    frameTest.set(rows,fraction<1?2048:0);for(const r of rows){r.palette_expanded=Float32Array.from(r.palette_expanded);r.ReleaseWarpMaps=()=>{};}
    let observedStars=[],observedTitle=null;
    const presenter={gpu_warp:{fading_dots:{initialize_bokeh_lifetime_history(){},set_bokeh_follow_motions(){},get_bokeh_alive_count(){return 0;},get_bokeh_average_lifespan(){return group===6?.1:group===7?0:4;}},
      set_fading_dots(){},text_overlay:{get_intensity(){return 0;}},set_overlay_text(text,size,time,options){observedTitle={x:options.center_x,y:options.center_y,fontPixels:size};},burn_overlay_text(){}},
      upload_audio_spectrum(){},uploadPaletteRGBA8UNorm(){},SetWarpMap(){},set_bokeh_frame(){},add_bokeh_dots(stars){observedStars=stars;},
      warpAndDrawWaveform(){const observed=frameTest.observed();observed.noiseX=Math.floor(Math.random()*255);observed.noiseY=Math.floor(Math.random()*255);}};
    const engine=new Engine(presenter,width,height,width,height);
    for(let k=0;k<12;k++){
      const seed=(0x12345678+group*759+k*37)>>>0,random=mulberry32(seed);let calls=0;activeRandom=()=>{calls++;return random();};observedStars=[];observedTitle=null;
      const input={width,height,shiftTime:1000+k*1.2,time:group*50+k*.1,fps:[60,30,120,5][k%4],motionScale:group===7?.25:1,brightness:k===7?1.7:1,headroom:group===3?3:1,darkening:1,alignFraction:fraction,
        waveSmoothing:k%3,beat:k%3===0?1:0,paletteLocked:k>=3&&k<=6,frozen:k===8,experiment:group===11,averageStarLifetime:group===6?.1:group===7?0:4,embedUnits:k===1?12:k===2?40:0};
      const audio={wave:sample(2048,k*.35),rms:k===9?0:.12,visualEnergy:k<6?.1:.001};
      engine.render(input.shiftTime,input.shiftTime,input.time,k,input.paletteLocked,false,false,false,false,false,null,1,input.fps,input.waveSmoothing,1.5,0,input.beat,input.motionScale,
        input.frozen,input.brightness,input.headroom,input.experiment,'x'.repeat(input.embedUnits),input.darkening,fraction,false,false,false,false,false,false,
        {wave:Float32Array.from(audio.wave),rms:audio.rms,visual_energy:audio.visualEnergy},{calibrating_hdr_headroom:0},-1,new Float32Array(512),'this_is_the_last_param');
      data.frames.push({seed,input,audio,expected:{...frameTest.observed(),stars:observedStars,title:observedTitle,noiseX:frameTest.observed().noiseX??0,noiseY:frameTest.observed().noiseY??0,calls}});
    }
  }
  fs.writeFileSync(path.join(output,'fixtures.json'),JSON.stringify({groups,waveform,startupFault})+'\n');
  fs.writeFileSync(path.join(output,'provenance.json'),JSON.stringify({reference_hashes:hashes,node:process.version,
    observation:'Unchanged render arithmetic; an inserted observation statement copies local frame controls. Presenter records GPU inputs and consumes the original two noise-offset random draws. Three controlled presets avoid unrelated worker scheduling.',
    alignment:'Production alignFraction=1 uses original startup. Fractional-window controls preinitialize the original aligner length to test the intended centered startup; the unmodified startup fault is separately captured.',
    random:'Mulberry32 uint32 state per draw, independent of Engine constructor seed draws.'},null,2)+'\n');
}finally{Math.random=originalRandom;}
if(process.argv[2]&&process.argv[2]!=='--fixtures-only'){
  const result=spawnSync(path.resolve(process.argv[2]),[path.join(output,'fixtures.json'),path.join(output,'result.json')],{encoding:'utf8',timeout:180000,maxBuffer:4*1024*1024});
  if(result.error)throw result.error;process.stdout.write(result.stdout);process.stderr.write(result.stderr);if(result.status!==0)process.exit(result.status||1);
}else console.log('Generated '+groups.length+' frame sequences');
