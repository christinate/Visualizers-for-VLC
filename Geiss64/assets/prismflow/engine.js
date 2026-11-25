/*
 * Geiss HDR
 * Copyright (c) 2026 Ryan Geiss
 * www.geisswerks.com/geiss_hdr
 *
 * License: Apache-2.0 - see /LICENSE.txt
 * 
 * Attribution Notice: see /NOTICE.txt
 *   Derivative works and redistributions must retain this NOTICE file.
 *
 * Naming / Branding Notice: see /NOTICE.txt
 *   "Geiss" and "Geiss HDR" are reserved names for the original Geiss HDR project.
 *   The Apache-2.0 license does not grant permission to use those names, or
 *   confusingly similar names, for derivative works except as needed to describe
 *   the origin of the work or reproduce the content of the NOTICE file.
 * 
 * Output permissions (for still images and still image sequences generated
 *   using this software): see /OUTPUTS.txt
 */

import { Blur, WaveAligner } from "./wave_align.js"
import { TransformHelper } from "./coords.js"
import { ShowError } from "./error.js"
import { Preset, Warp, blendStructs } from "./preset.js"
import { kGpuWarpSlots } from "./const.js"
import { linearResampleF } from "./resample.js"
//import { GenerateRandomPalette, SamplePalette, HdrToSdr, AddPalette, SetOverridePalette, PrintOverridePalette, ClearOverridePalette, OverridePaletteRel, OverridePaletteAbs, AdjustOverridePalette, GetOverridePalette, PrintSavedPalettes } from "./palette.js"
import { ReduceHdrHeadroom } from "./palette.js"
import { kMinMaxBokehDotXY, kBokehDotCapacity } from "./gpu_advected_dots.js"

export const GENERAL_TRANSITION_SPEED = 1.0;//1.3;//1.0
const PRESET_DURATION_SEC_MIN = 20.0 / GENERAL_TRANSITION_SPEED;//14
const PRESET_DURATION_SEC_MAX = 30.0 / GENERAL_TRANSITION_SPEED;//26
const PRESET_FADE_SEC_MIN     =  3.0 / GENERAL_TRANSITION_SPEED;
const PRESET_FADE_SEC_MAX     =  6.0 / GENERAL_TRANSITION_SPEED;

export function GetRandomPresetDuration() {
  return PRESET_DURATION_SEC_MIN + (PRESET_DURATION_SEC_MAX - PRESET_DURATION_SEC_MIN) * Math.random();
}
export function GetRandomPresetFadeDuration() {
  return PRESET_FADE_SEC_MIN + (PRESET_FADE_SEC_MAX - PRESET_FADE_SEC_MIN) * Math.random();
}

const BASE_MOTION_SCALE = 109.0 * 0.9 * 0.94;//120.0;

const WAVE_STRETCH = 1.5;			// Flat wave only.
const WAVE_MAG_SCALE = 1.1;//1.0     // All waves.  ~The color intensity of the drawn wave points.
// When the wave 'visual_energy' is above this amount, the wave amplitude will be reduced (limited),
// to keep it from being an ugly mess spraying all over the screen.
const kMaxVisualEnergy = 0.009;//0.012;//0.014;//0.012;//0.010;

const SHIFT_MAGNITUDE = 0.95 * 0.65;//1.0;
const SHIFT_FREQ = 0.7;

const TITLE_EMBED_DURATION_SECONDS = 0.8;//0.79;
const TITLE_EMBED_FADE_IN_POWER = 0.5;//0.3;
const TITLE_EMBED_SIZE = 1.1;

const kWaveOpacityScale = 0.75;//1.0;

const kBokehParticleFadeIn  = 0.5;//1.0;
const kBokehParticleFadeOut = 0.5;//1.0;    // Only relevant for non-zoomy warps.
const kBokehParticleHoldTime = 4.0 - kBokehParticleFadeIn - kBokehParticleFadeOut;    // Only relevant for non-zoomy warps.
const kBokehParticleMaxLifetime = kBokehParticleFadeIn + kBokehParticleFadeOut + kBokehParticleHoldTime;

let g_title_x = -1;
let g_title_y = -1;

let g_last_mode_desc = "";
let g_last_palette_desc = "";
let g_last_misc_debug_text = "";
let g_last_pure_palette_orig = null;

//let rms_att = 0.0;
let g_visual_energy_att = 0.0;

// There are 3 types of darkening that add together:
// 1. User darkening (passed to 'render' as 'darkening')
// 2. g_auto_darkening, which kicks in when the motion is very zoomy and
//      we're using a single flat wave.
// 3. image brightness-based darkening, which is handled in gpu_warp.js
//      (search for 'exposure' and 'decay').
let g_auto_darkening = 0.0;

let g_frozen_palette = null;

const aligner = new WaveAligner();

// -------------------------
// PRESETS
// -------------------------

// Preset history buffer.  
// Oldest one is at [0], 
//   current one is at [g_cur_preset_index], 
//   future ones might be beyond that.
let g_global_preset_number = 0;
const g_presets = [];    // To add a new preset: g_presets.push(new Preset());
let g_cur_preset_index = -1;

//        0             t1     t2                fading
// --*--*-|-------------*------*----------------*---------*----------->
//   t1 t2               fading                 t1        t2
// 0000000000000000000000000ooo                 
//                       iii11111111111111111111111111ooooo
//                                               iiiii22222222222222222... 

// hard_cut == true: 
//   This version does an immediate hard-cut to the next preset.
// hard_cut == false: 
//   Jumps us forward to where the next preset would start fading in.
//   If already fading, does nothing.
// Reminder: t1 = The time at which it starts to fade in.  (weight at t1 = 0%)
//           t2 = The time at which it is fully faded in.  (weight at t2 = 100%)
export function JumpToNextPreset(preset_time, hard_cut = true) {
  if (hard_cut) {
    // Hard cut.
    for (let i = 0; i < g_presets.length; i++) {
      if (preset_time < (g_presets[i].t1 * 0.1 + 0.9 * g_presets[i].t2)) {
        preset_time = g_presets[i].t2 + 0.0001;
        if (!g_presets[i].warp_maps_ready) {
          g_presets[i].RebuildWarpMaps();
        }
        return preset_time;
      }
    }
    return preset_time;
  } else {
    // Soft cut: Begin the fade to the next preset.
    // If a fade is already in progress, do nothing.
    for (let i = 0; i < g_presets.length - 1; i++) {
      if (preset_time < g_presets[i].t2) {
        // We're already fading, or this is the first preset; do nothing.
        return preset_time;
      }
      if (preset_time < g_presets[i + 1].t1) {
        // Jump forward to the next preset's start-fade-in time.
        preset_time = g_presets[i + 1].t1 + 0.0001;
        if (!g_presets[i + 1].warp_maps_ready) {
          g_presets[i + 1].RebuildWarpMaps();
        }
        return preset_time;
      }
    }
    return preset_time;
  }
}

export function JumpToPrevPreset(preset_time) {
  for (let i = g_presets.length - 1; i >= 0; i--) {
    if (preset_time > (g_presets[i].t1 * 0.9 + 0.1 * g_presets[i].t2)) {
      let i2 = Math.max(0, i - 1);
      preset_time = g_presets[i2].t2 + 0.0001;
      if (!g_presets[i2].warp_maps_ready) {
        g_presets[i2].RebuildWarpMaps();
      }
      return preset_time;
    }
  }
  return preset_time;
}




// To keep the history from getting too long and gobbling memory:
function PrunePresets() {
  // See also: kPresetKeepInMemoryRadius
  // See also: kGpuWarpMapCacheSize in gpu_warp.js
  const kMaxPresetHistorySize = 128;//64;

  while (g_presets.length > kMaxPresetHistorySize) {
    //console.log(`### pruning an old preset`);
    g_presets.shift();    // Removes and returns presets[0].
  }
}

// Determine which preset(s) in g_presets[] are active right now, and if so,
// how to blend between them.
function GetActivePresets(preset_time) {
  let active_preset_indices = [];
  let active_preset_weights = [];
  let most_active_preset_index = -1;
  let preset_blend_t = 0.0;

  //        0             t1     t2                fading
  // --*--*-|-------------*------*----------------*---------*----------->
  //   t1 t2               fading                 t1        t2
  // 0000000000000000000000000ooo                 
  //                       iii11111111111111111111111111ooooo
  //                                               iiiii22222222222222222... 

  if (preset_time < g_presets[0].t2) {
    ShowError(`ERROR: GetActivePresets: no presets back that far.`);
    console.log(`time ${preset_time}  g_presets[0].t1 ${g_presets[0].t1} g_presets[0].t2 ${g_presets[0].t2}`);        
    active_preset_indices.push(0);
    active_preset_weights.push(1.0);
    most_active_preset_index = 0;
  } else {
    for (let i = 1; i < g_presets.length; i++) {
      if (preset_time < g_presets[i].t1) {
        active_preset_indices.push(i - 1);
        active_preset_weights.push(1.0);
        most_active_preset_index = i - 1;
        break;
      }
      if (preset_time < g_presets[i].t2) {
        // Blend between two presets.
        preset_blend_t = (preset_time - g_presets[i].t1) / (g_presets[i].t2 - g_presets[i].t1);
        preset_blend_t = SmoothPresetBlendT(preset_blend_t);
        
        active_preset_indices.push(i - 1);
        active_preset_indices.push(i);
        active_preset_weights.push(1.0 - preset_blend_t);
        active_preset_weights.push(preset_blend_t);

        most_active_preset_index = (preset_blend_t >= 0.5) ? i : (i - 1);
        break;   
      }
    }

    if (active_preset_indices.length == 0) {
      active_preset_indices.push(g_presets.length - 1);
      active_preset_weights.push(1.0);
      most_active_preset_index = g_presets.length - 1;
    }
  }
  
  return {
    active_preset_indices,
    active_preset_weights,
    most_active_preset_index,
    preset_blend_t,
  };
}    


// -------------------------
// PRESET BKG GEN
// -------------------------

// Worker for background warp map generation.
const g_bkg_worker = new Worker("./background_worker.js", { type: "module" });

let g_bkg_preset = null;
let g_bkg_preset_in_progress = false;
const g_bkg_gen_verbose = false;
const g_bkg_rebuild_verbose = false;

g_bkg_worker.onmessage = (e) => {
  const copied_preset = e.data.result;

  // The data for 'preset' was cloned between the threads.
  // ...BUT the methods were stripped, so we need to restore those.
  // SEE ALSO: Same code in background_worker.js.
  Object.setPrototypeOf(copied_preset, Preset.prototype);
  for (const warp of copied_preset.warps) {
    Object.setPrototypeOf(warp, Warp.prototype);
  }

  if (e.data.type === "build_preset_done") {    
    g_bkg_preset = copied_preset;
    g_bkg_preset_in_progress = false;
		if (g_bkg_gen_verbose) {
      console.log(`# bkg generation of warp map complete!  t1 ${g_bkg_preset.t1} t2 ${g_bkg_preset.t2}`);
    }
  }
  if (e.data.type === "rebuild_warp_maps_done") {
    const preset = g_presets.find(
      p => p.rebuild_id === copied_preset.rebuild_id
    );
    if (preset) {
      if (preset.warp_maps_ready) {
        // Something happened during the background generation (such as randomizing motion)
        // that caused the preset's warp maps to be regenerated in the main thread, so we
        // can ignore the background result.
        console.log(`Note: Ignoring bkg-rebuilt preset with rebuild_id ${copied_preset.rebuild_id} because matching preset already had its warp map updated by the main thread.`);
      } else {
        // Copy .warps[] only, as the user could have randomized waves, dots, etc in the meantime,
        // and we don't want to overwrite those.
        Object.assign(preset.warps, copied_preset.warps);
        preset.warp_maps_ready = true;
        preset.rebuild_id = -1;
      }
    } else {
      console.log(`ERROR: unable to match bkg-rebuilt preset by its index ${copied_preset.rebuild_id}.`);
    }

    g_bkg_preset_rebuilding = false;
    if (g_bkg_rebuild_verbose) {
      console.log(`# bkg rebuild of warp maps complete`);
    }
  }
};

function generate_preset_in_background(preset_time, payload) {
	if (g_bkg_gen_verbose) console.log(`# kicking off bkg generation of a preset, at time ${preset_time}`);
  g_bkg_preset = null;
  g_bkg_preset_in_progress = true;
  g_bkg_worker.postMessage({ type: "build_preset", payload });
}

// -------------------------
// PRESET BKG REBUILD
// -------------------------
let g_bkg_preset_rebuild_next_id = 0;
let g_bkg_preset_rebuilding = false;

function rebuild_preset_in_background(preset) {
  g_bkg_preset_rebuilding = true;

  preset.rebuild_id = g_bkg_preset_rebuild_next_id++;

  const payload = { 
    preset : preset 
  };

  if (g_bkg_rebuild_verbose) console.log(`# kicking off bkg rebuild of preset ${preset.global_preset_number}, rebuild id ${preset.rebuild_id}`);

  // Note: This will make a COPY of the preset and work on it in the other thread.
  g_bkg_worker.postMessage({ type: "rebuild_warp_maps", payload });
}



// -------------------------
// MATH HELPER FUNCTIONS
// -------------------------

// Wave flat angle will be in the [0..pi] range most of the time;
//   we want to make sure that if 0.1 has to rotate to 3.05, it does
//   it the quicker way.
function WrapAngleNear(A, B) {
  // Normally you would wrap on pi*2, but we wrap just on pi; no one will 
  // notice if the flat wave suddenly flips.
  const TWO_PI = Math.PI;// * 2;
  let A2 = A + TWO_PI * Math.round((B - A) / TWO_PI);
  return A2;
}

function lerp(a, b, t) {
	return a * (1.0 - t) + b * t;
}

function smoothstep(u) {
  return u * u * (3 - 2 * u);
}

function smootherstep(u) {
  return u * u * u * (6 * u * u - 15 * u + 10);
}

function RandomChaserStrength() {
	return Math.max(0.01, Math.random() * Math.random());
}

function GetDiag(w, h) {
	return Math.sqrt(w * w + h * h) | 0;
}

function SmoothPresetBlendT(t) {
  return smoothstep(t);
}


// -------------------------
// DEBUGGING
// -------------------------

export function GetWarpDesc() {
  return g_last_mode_desc;
}

export function GetMiscDebug() {
  return g_last_misc_debug_text;
}

export function GetPaletteDesc() {
  if (g_frozen_palette) {
    return `[Frozen] ${g_last_palette_desc}`;
  } else {
    return g_last_palette_desc;
  }
}

export function GetPaletteOrig() {
  return g_last_pure_palette_orig;
}


// -------------------------
// ENGINE CLASS
// -------------------------

export class Engine {
  // cw, ch: client size
  // iw, ih: index buffer size (might be 2x smaller, 3x smaller, etc)
  constructor(presenter, cw, ch, iw, ih) {
    this.cw = cw;
    this.ch = ch;
    this.iw = iw;
    this.ih = ih;

    // Double buffers of indices
    // TODO: nuke these 4
    this.a = new Uint8Array(this.iw * this.ih);
    this.b = new Uint8Array(this.iw * this.ih);
    this.front = this.a;
    this.back = this.b;

		this.presenter = presenter;

    this.presenter.gpu_warp.fading_dots.initialize_bokeh_lifetime_history(kBokehParticleMaxLifetime); // Maximum lifetime, in seconds.

    // Starter: seed with a small blob so feedback has something to warp
    this.seed();
  }

	
  // cw, ch: client size
  // iw, ih: index buffer size (might be 2x smaller, 3x smaller, etc)
  resize(cw, ch, iw, ih) {
    this.cw = cw;
    this.ch = ch;
    this.iw = iw;
    this.ih = ih;

		// TODO: resample the old image into the new one.
    this.a = new Uint8Array(this.iw * this.ih);
    this.b = new Uint8Array(this.iw * this.ih);
    this.front = this.a;
    this.back = this.b;

    this.seed();
  }

  SetWarpSlotDebug(dbg_warp_slot, mode, preset_time) {
    // Determine which preset(s) are currently active.
    let {
      active_preset_indices,      // int[]
      active_preset_weights,      // float[]
      most_active_preset_index,   // int
      preset_blend_t,             // float
    } = GetActivePresets(preset_time);
    
    for (let i = 0; i < active_preset_indices.length; i++) {
      let preset = g_presets[active_preset_indices[i]];
      preset.SetWarpSlotDebug(dbg_warp_slot, mode);
    }    
  }

  seed() {  
    // Full-frame random noise so warp is immediately visible
    for (let i = 0; i < this.front.length; i++) {
      this.front[i] = Math.random() * 31 | 0;
    }
  }

  //                                     preset_time    shift_time    true_time
  //                                     -----------    ----------    ---------
  // Speed can be decreased by user?     yes            yes           no
  // Speed can be increased by user?     yes            no            no
  // Can it jump fwd/back?               yes            no            no
  render(preset_time, shift_time, true_time, frame, palette_locked,
         randomize_motion, randomize_palette, randomize_wave, randomize_dots, randomize_shader, 
         overwrite_palette, scale_palette_brightness,
         fps, wave_smoothing, user_wave_point_size, xy_oscilloscope_gap, beat, user_motion_scale,
  			 frozen, brightness, hdr_display_headroom,//remap_to_sdr, 
         experiment, embed_string, darkening, align_frac,
     		 toggle_grid_dots, toggle_fading_dots, g_toggle_chasers, g_toggle_dotty_chaser,
				 toggle_random_beat_dots, toggle_radial_beat_dots, audioFrame, hdr_calib_params, dbg_warp_slot, spectrum,
  			 last_param) {
  			 //bpm, bpm_confidence) {
		const W = this.iw;  // display image size; not canvas size.
		const H = this.ih;
		const diagonal = Math.sqrt(W * W + H * H) | 0;

		if (last_param != "this_is_the_last_param") {
			console.log("ERROR: Parameter integrity check failed");
			ShowError("ERROR: Parameter integrity check failed (Engine::render)");
			return;
		}

    this.presenter.upload_audio_spectrum(spectrum);

    // At init time, start with 2 random presets.
    if (g_presets.length == 0) {
      // Generate the first preset, and the one it will transition into.
      let t1 = preset_time - 0.002;
      let t2 = preset_time - 0.001;
      g_presets.push(new Preset(g_global_preset_number++, t1, t2));

      t1 = t2 + GetRandomPresetDuration();
      t2 = t1 + GetRandomPresetFadeDuration();
      g_presets.push(new Preset(g_global_preset_number++, t1, t2));
    }

    // If a background-generated preset finished, add it to g_presets[].
    if (g_bkg_preset !== null) {
      g_presets.push(g_bkg_preset);
      g_bkg_preset = null;
    }    

    // Kick off background generation of a new Preset, if appropriate.
    //
    //        0             t1     t2                fading
    // --*--*-|-------------*------*----------------*---------*----------->
    //   t1 t2               fading                 t1        t2
    // 0000000000000000000000000ooo                 
    //                       iii11111111111111111111111111ooooo
    //                                               iiiii22222222222222222... 
    const last_preset = g_presets[g_presets.length - 1];
    if (preset_time >= last_preset.t2 && !g_bkg_preset_in_progress) {
      // Kick off background generation of a new preset.
      let t1 = Math.max(last_preset.t2, preset_time) + GetRandomPresetDuration();
      let t2 = t1 + GetRandomPresetFadeDuration();
      const payload = { 
          t1 : t1, 
          t2 : t2,
          global_preset_number : g_global_preset_number++ };
      generate_preset_in_background(preset_time, payload);
    }


    // Determine which preset(s) are currently active.
    let {
      active_preset_indices,      // int[]
      active_preset_weights,      // float[]
      most_active_preset_index,   // int
      preset_blend_t,             // float
    } = GetActivePresets(preset_time);

    // Make sure that all currently-active presets have their warp maps ready.  If not, 
    // generate them (blocking) now.
    for (let i = 0; i < active_preset_indices.length; i++) {
      let preset = g_presets[active_preset_indices[i]];
      if (!preset.warp_maps_ready) {
        console.log(`Warning: an active preset's warp maps were not ready; had to force-generate them.`);
        preset.RebuildWarpMaps();
      }
    }
    
    // Release warp maps for presets that we're storing longer-term.
    // Higher values here will use SIGNIFICANTLY more memory -- about 16 MB per increment
    // (512x512 warp map * 2 channels (x,y) * 4 bytes/channel (float) * 4 warps * 2 presets ~= 16 MB)
    // See also: kMaxPresetHistorySize
    // See also: kGpuWarpMapCacheSize in gpu_warp.js
    const kPresetKeepInMemoryRadius = 3;
    let i1 = Math.max(most_active_preset_index - kPresetKeepInMemoryRadius, 0);
    let i2 = Math.min(most_active_preset_index + kPresetKeepInMemoryRadius + 1, g_presets.length);
    for (let i = 0; i < i1; i++) {
      g_presets[i].ReleaseWarpMaps();
    }
    if (!g_bkg_preset_rebuilding) {
      for (let i = i1; i < i2; i++) {
        if (!g_presets[i].warp_maps_ready) {
          rebuild_preset_in_background(g_presets[i]);
          break;
        }
      }
    }
    for (let i = i2; i < g_presets.length; i++) {
      g_presets[i].ReleaseWarpMaps();
    }



    // Apply user requests to randomize certain things.
    const src_preset = g_presets[most_active_preset_index];
    if (randomize_motion) {
      // Randomize it for the most active current preset.
      src_preset.RandomizeMotion();
      // If any others are fading in/out, replicate it to them, too.
      for (let i = 0; i < active_preset_indices.length; i++) {
        if (active_preset_indices[i] != most_active_preset_index) {
          let dst_preset = g_presets[active_preset_indices[i]];
          dst_preset.warps = structuredClone(src_preset.warps);
          dst_preset.motion_params = structuredClone(src_preset.motion_params);
          dst_preset.warp_maps_ready = src_preset.warp_maps_ready;
          dst_preset.rebuild_id = -1;  // Invalidate any background warp map rebuilds in progress.
          for (const warp of dst_preset.warps) {
            Object.setPrototypeOf(warp, Warp.prototype);
          }
        }
      }
		}
    if (randomize_wave) {     
      // Randomize it for the most active current preset.
      src_preset.RandomizeWaveParams();
      // If any others are fading in/out, replicate it to them, too.
      for (let i = 0; i < active_preset_indices.length; i++) {
        if (active_preset_indices[i] != most_active_preset_index) {
          let dst_preset = g_presets[active_preset_indices[i]];
          dst_preset.wave_params = structuredClone(src_preset.wave_params);
        }
      }
    }
    if (randomize_dots) {     
      // Randomize it for the most active current preset.
      src_preset.RandomizeDotParams();
      // If any others are fading in/out, replicate it to them, too.
      for (let i = 0; i < active_preset_indices.length; i++) {
        if (active_preset_indices[i] != most_active_preset_index) {
          let dst_preset = g_presets[active_preset_indices[i]];
          dst_preset.dot_params = structuredClone(src_preset.dot_params);
        }
      }
    }
    if (randomize_shader) {     
      // Randomize it for the most active current preset.
      src_preset.RandomizeShaderParams();
      // If any others are fading in/out, replicate it to them, too.
      for (let i = 0; i < active_preset_indices.length; i++) {
        if (active_preset_indices[i] != most_active_preset_index) {
          let dst_preset = g_presets[active_preset_indices[i]];
          dst_preset.shader_params = structuredClone(src_preset.shader_params);
        }
      }
    }
    
    let updated_palette = false;
    if (randomize_palette) {
      // Randomize it for the most active current preset.
      src_preset.RandomizePalette();
      updated_palette = true;
    }
    if (overwrite_palette) {
      src_preset.palette_orig = structuredClone(overwrite_palette);
      src_preset.ExpandPalette();
      updated_palette = true;
    }
    if (scale_palette_brightness != 1.0) {
      if (src_preset.palette_orig != null) {
        for (let i = 0; i < src_preset.palette_orig.palette.length; i++) {
          src_preset.palette_orig.palette[i] *= scale_palette_brightness;
        }
        src_preset.ExpandPalette();
        updated_palette = true;
      }
    }
    if (updated_palette) {
      // If any others are fading in/out, replicate it to them, too.
      for (let i = 0; i < active_preset_indices.length; i++) {
        if (active_preset_indices[i] != most_active_preset_index) {
          let dst_preset = g_presets[active_preset_indices[i]];
          dst_preset.palette_orig = structuredClone(src_preset.palette_orig);
          dst_preset.palette_expanded = structuredClone(src_preset.palette_expanded);
        }
      }
    }

    // Apply any user effect toggles.
    {
  		if (toggle_grid_dots) {
        const k = (src_preset.dot_params.grid_dots > 0.1) ? 0.0 : 1.0;
        for (let z = 0; z < active_preset_indices.length; z++) {
          g_presets[active_preset_indices[z]].dot_params.grid_dots = k;
        }
  		}
  		if (toggle_fading_dots) {
        const k = (src_preset.dot_params.fading_dots > 0.1) ? 0.0 : 1.0;
        for (let z = 0; z < active_preset_indices.length; z++) {
          g_presets[active_preset_indices[z]].dot_params.fading_dots = k;
        }
  		}
  		if (g_toggle_dotty_chaser) {
        const k = (src_preset.dot_params.dotty_chaser > 0.1) ? 0.0 : 1.0;
        for (let z = 0; z < active_preset_indices.length; z++) {
          g_presets[active_preset_indices[z]].dot_params.dotty_chaser = k;
        }
  		}
  		if (toggle_random_beat_dots) {
        const k = (src_preset.dot_params.random_beat_dots > 0.1) ? 0.0 : 1.0;
        for (let z = 0; z < active_preset_indices.length; z++) {
          g_presets[active_preset_indices[z]].dot_params.random_beat_dots = k;
        }
  		}
  		if (toggle_radial_beat_dots) {
        const k = (src_preset.dot_params.radial_beat_dots > 0.1) ? 0.0 : 1.0;
        for (let z = 0; z < active_preset_indices.length; z++) {
          g_presets[active_preset_indices[z]].dot_params.radial_beat_dots = k;
        }
  		}
  		if (g_toggle_chasers) {
        let chaser_1_active = (src_preset.dot_params.chaser_1 > 0.001);
        let chaser_2_active = (src_preset.dot_params.chaser_2 > 0.001);
  			const old_chaser_count = (chaser_1_active ? 1 : 0) + (chaser_2_active ? 1 : 0);
  			const new_count = (old_chaser_count + 1) % 3;
        for (let z = 0; z < active_preset_indices.length; z++) {
    			// Chaser 1:
    			{
    				const new_value = (new_count > 0) ? 1.0 : 0.0;
            g_presets[active_preset_indices[z]].dot_params.chaser_1 = new_value;
    			}
    			// Chaser 2:
    			{
    				const new_value = (new_count > 1) ? 1.0 : 0.0;
            g_presets[active_preset_indices[z]].dot_params.chaser_2 = new_value;
    			}
        }
  		}
    }



    // Get the current [blend of] presets.    
    let blended_warps = null;
    let blended_warp_fade_strs = null;
    let follow_motions = new Float32Array(kGpuWarpSlots).fill(0.0);
    let blended_palette = null;
    let blended_bkg = null;
    let blended_shader_params = null;
    let blended_dot_params = null;
    let blended_wave_params = null;
    let blended_motion_params = null;
    if (active_preset_indices.length == 1) {
      // Pull from a single preset.
      let preset = g_presets[active_preset_indices[0]];
          
      blended_warps = preset.warps;
      blended_warp_fade_strs = new Float32Array(preset.warps.length).fill(1.0);
      for (let i = 0; i < preset.warps.length; i++) {
        follow_motions[i] = preset.warps[i].warp_map.warp_prefs.stars_follow_motion;
      }
      
      blended_palette = preset.palette_expanded;
      blended_bkg = preset.palette_orig.bkg;
      
      blended_shader_params = preset.shader_params;
      blended_dot_params = preset.dot_params;
      blended_wave_params = preset.wave_params;
      blended_motion_params = preset.motion_params;   

    } else {
      // Blend between two presets.
      let preset1 = g_presets[active_preset_indices[0]];
      let preset2 = g_presets[active_preset_indices[1]];
      let t = preset_blend_t;

      blended_warps = new Array(preset1.warps.length + preset2.warps.length);
      blended_warp_fade_strs = new Float32Array(preset1.warps.length + preset2.warps.length);
      for (let i = 0; i < preset1.warps.length; i++) {
        blended_warps[i] = preset1.warps[i];
        blended_warp_fade_strs[i] = 1.0 - t;
        follow_motions[i] = preset1.warps[i].warp_map.warp_prefs.stars_follow_motion;
      }
      for (let i = 0; i < preset2.warps.length; i++) {
        let i2 = preset1.warps.length + i;
        blended_warps[i2] = preset2.warps[i];
        blended_warp_fade_strs[i2] = t;
        follow_motions[i2] = preset2.warps[i].warp_map.warp_prefs.stars_follow_motion;
      }

      // Blend palette.
      blended_palette = new Float32Array(256 * 4);  // values in [0..1+], where 1 is SDR white.
      for (let i = 0; i < 256 * 4; i++) {
        blended_palette[i] = preset1.palette_expanded[i] * (1.0 - t) + preset2.palette_expanded[i] * t;
      }

      // Blend palette background color.
      blended_bkg = new Float32Array(3);
      for (let i = 0; i < 3; i++) {
        blended_bkg[i] = preset1.palette_orig.bkg[i] * (1.0 - t) + preset2.palette_orig.bkg[i] * t;
      }

      // Blend params.
      let weights = [ 1.0 - t, t ];
      blended_shader_params = blendStructs([preset1.shader_params, preset2.shader_params], weights);
      blended_dot_params = blendStructs([preset1.dot_params, preset2.dot_params], weights);
      blended_wave_params = blendStructs([preset1.wave_params, preset2.wave_params], weights);
      blended_motion_params = blendStructs([preset1.motion_params, preset2.motion_params], weights);

      // Perform a special blend for the flat wave angle, with wrapping.
      let angle1 = preset1.wave_params.wave_flat_angle;
      let angle2 = preset2.wave_params.wave_flat_angle;
      angle1 = WrapAngleNear(angle1, angle2);
      blended_wave_params.wave_flat_angle = angle1 * (1.0 - t) + angle2 * t;
    }



		// Update the palette and prepare the "bkg" compation color
    // -- including adapting them to the HDR headroom of the display (via ReduceHdrHeadroom).
    let palette_to_upload = new Float32Array(1024);
    let bkg_color_adapted_to_headroom = new Float32Array(3);
		{
      for (let i = 0; i < 256 * 4; i++) {
        palette_to_upload[i] = blended_palette[i];
      }

      // Update 'g_last_palette_desc' and 'g_last_pure_palette_orig'.
      if (!g_frozen_palette) {
        if (active_preset_indices.length == 1) {
          g_last_palette_desc = g_presets[active_preset_indices[0]].palette_orig.name;
          g_last_pure_palette_orig = g_presets[active_preset_indices[0]].palette_orig;
        } else if (active_preset_indices.length == 2) {
          g_last_palette_desc = `${g_presets[active_preset_indices[0]].palette_orig.name} (${((active_preset_weights[0] * 100) | 0)}%), ${g_presets[active_preset_indices[1]].palette_orig.name} (${((active_preset_weights[1] * 100) | 0)}%)`;
          g_last_pure_palette_orig = null;
        } else {
          g_last_palette_desc = `?`;
          g_last_pure_palette_orig = null;
        }
      }      

      // Handle frozen palette.
      if (!palette_locked) {
        g_frozen_palette = null;
      } else {
        if (g_frozen_palette) {
    			for (let i = 0; i < 256 * 4; i++) {
    				palette_to_upload[i] = g_frozen_palette[i];
    			}
        } else {
          g_frozen_palette = new Float32Array(1024);
          for (let i = 0; i < 256 * 4; i++) {
            g_frozen_palette[i] = palette_to_upload[i];
          }
        }
      }

			// Scale by 'brightness', clamp, and measure max value.
			// Don't let palette colors exceed this amount.
			// We limit this to ~3 because even if HDR displays support
			//   more than 3x brightness over SDR, we don't want to use
			//   that much -- it would hurt your eyes.
			const enforced_max_value = 3.0;
			let max_value = 0.0;
			for (let i = 0; i < 256; i++) {
				let r = palette_to_upload[i * 4 + 0] * brightness;
				let g = palette_to_upload[i * 4 + 1] * brightness;
				let b = palette_to_upload[i * 4 + 2] * brightness;
				r = Math.min(r, enforced_max_value);
				g = Math.min(g, enforced_max_value);
				b = Math.min(b, enforced_max_value);
				palette_to_upload[i * 4 + 0] = r;
				palette_to_upload[i * 4 + 1] = g;
				palette_to_upload[i * 4 + 2] = b;
				palette_to_upload[i * 4 + 3] = 1.0;
				max_value = Math.max(Math.max(r, g), Math.max(b, max_value));
			}				

			// Squeeze the palette down to the available HDR headroom.
      // If hdr_display_headroom is as low as 1.0, this will do a full HDR -> SDR remap.
			for (let i = 0; i < 256; i++) {
				let t = i * (1.0 / 255);
				let r = palette_to_upload[i * 4 + 0];
				let g = palette_to_upload[i * 4 + 1];
				let b = palette_to_upload[i * 4 + 2];
				//let col = HdrToSdr(t, r, g, b, max_value, experiment);
        let col = ReduceHdrHeadroom(hdr_display_headroom, t, r, g, b, max_value, experiment);
				palette_to_upload[i * 4 + 0] = col[0];
				palette_to_upload[i * 4 + 1] = col[1];
				palette_to_upload[i * 4 + 2] = col[2];
			}

      // Also adapt the blended "bkg" companion color to the available HDR headroom. 
      // And apply 'brightness'.
      let t = 0.7; //xxx - ok??
      let col = ReduceHdrHeadroom(hdr_display_headroom, t, 
                                  blended_bkg[0] * brightness, 
                                  blended_bkg[1] * brightness, 
                                  blended_bkg[2] * brightness, 
                                  max_value, experiment);
      for (let i = 0; i < 3; i++) {
        bkg_color_adapted_to_headroom[i] = col[i];
      }
      
			this.presenter.uploadPaletteRGBA8UNorm(palette_to_upload);
		}
    


    // Determine the strengths for the various warp maps, and upload them if needed.
		let final_warp_str = new Float32Array(blended_warps.length).fill(0.0);
    g_last_mode_desc = "";
		for (let i = 0; i < blended_warps.length; i++) {
			// Adjust for various factors.
      let str2 = blended_warps[i].str * blended_warp_fade_strs[i];
						
      // Breathing.
      let str3 = str2 * (1.0 + blended_warps[i].warp_map.breathing_str * 
          Math.cos(shift_time * blended_warps[i].warp_map.breathing_freq + blended_warps[i].warp_map.breathing_phase));

      let str4 = str3 * BASE_MOTION_SCALE * user_motion_scale / fps;

      final_warp_str[i] = str4;

      const mode = (`${blended_warps[i].warp_map.mode}`).padStart(2, ' ');
      g_last_mode_desc += `Warp ${i}:   mode ${mode},  str ${str2.toFixed(2)} / ${str3.toFixed(2)}`;
      if (i == dbg_warp_slot) {
        g_last_mode_desc += ` <-`;
      }
      if (i != blended_warps.length - 1) {
        g_last_mode_desc += `\n`;
      }
		}

		// Update warp maps for the GPU (if needed), which always has 'kGpuWarpSlots' slots.
		let gpu_warp_str = new Float32Array(kGpuWarpSlots).fill(0.0);
    for (let i = 0; i < Math.min(kGpuWarpSlots, blended_warps.length); i++) {
      gpu_warp_str[i] = final_warp_str[i];
      // Note that internally, this caches the ~20 most recent warp maps used.
			this.presenter.SetWarpMap(i, blended_warps[i].warp_map);
    }
    for (let i = blended_warps.length; i < kGpuWarpSlots; i++) {
      // ~dummy binding.
			this.presenter.SetWarpMap(i, blended_warps[0].warp_map);			
    }




		// Determine shift_x and shift_y, in pixels.
		let shift_x = 0.0;
		let shift_y = 0.0;
		{
			const shift_mag = 
					BASE_MOTION_SCALE * user_motion_scale * diagonal / fps * 0.000121 * SHIFT_MAGNITUDE;
			
			const speed = SHIFT_FREQ;
			const t = shift_time * 60;
			shift_x = shift_mag *
					(Math.cos(t * 0.01171 * speed + 0) * 2 +
					 Math.cos(t * 0.00111 * speed + 3) * 2);
			shift_y = shift_mag *
					(Math.cos(t * 0.00874 * speed + 1) * 1.3 +
					 Math.cos(t * 0.00351 * speed + 2) * 1.3);

      /*
			// Hacked no-motion modes:
			if (g_active_warp_maps[0].mode == 98) {
				shift_x = 0.0;
				shift_y = 0.0;
			}
			if (g_active_warp_maps[0].mode == 99) {
				shift_x = 0.0;
				shift_y = -5.0;
			}
      */
		}
				
		// Base this on real time, not motion_time, which looks funny if the user
		// cranks the transition speed way up.



		// This is the diagonal size at which we jump from 1x1 dots 
		//   to proportionally larger, anti-aliased dots.
		// Note that below this, the wave doesn't need adjustment, since
		//   extra samples are not drawn more densely -- the wave is essentially
		//   cropped (more samples end up offscreen).
  	const diag1 = GetDiag(2060, 1430);
  	//const diag2 = GetDiag(3024, 1964);  // macbook @ highest res
  	//const diag2 = GetDiag(6016, 3384);  // XDR display max res
		const wave_point_size_in_pixels = Math.max(1.0, diagonal / diag1);    // > 2.0

		let wave_ampl_scale = 1.0;
		{
			// Update g_visual_energy_att.
			const visual_energy_imm = audioFrame.visual_energy;
			const guarded_fps = Math.max(5.0, Math.min(120, fps));
			let update_rate = (visual_energy_imm > g_visual_energy_att) ? 0.7 : 0.1;
			let t = 1.0 - Math.pow(1.0 - update_rate, 1.0 / guarded_fps);
			g_visual_energy_att = g_visual_energy_att * (1.0 - t) + t * visual_energy_imm;

			if (g_visual_energy_att > kMaxVisualEnergy) {
				wave_ampl_scale = kMaxVisualEnergy / g_visual_energy_att;
			} else {
				wave_ampl_scale = 1.0;
			}
			//console.log(g_visual_energy_att.toFixed(4), " / ", wave_ampl_scale.toFixed(2));
		}

		const wave_opacity_scale = kWaveOpacityScale;//experiment ? 0.5 : 1.0;//1.0;
		
    let dots = 
    		this.generateWaveformPoints(blended_wave_params, wave_point_size_in_pixels, wave_smoothing, xy_oscilloscope_gap, wave_opacity_scale, wave_ampl_scale, align_frac, audioFrame, experiment);



		if (beat > 0.0001) {
			let beat_dots = {
				dot_rad : 0.0025 + Math.random() * 0.0025*10,
				dot_count : Math.pow(2.0, 4 + 3 * Math.random()),
				intensity : 32 + Math.random() * 128,
			};
			let intensity = beat_dots.dot_rad * beat_dots.dot_rad * beat_dots.dot_count * beat_dots.intensity * (beat + 1);
			const kTargetIntensity = (0.08 + 0.08 * Math.random()) * 1.5;
			const adj = Math.pow(kTargetIntensity / intensity, 0.25);
			beat_dots.dot_rad *= adj * adj * 0.44;
			beat_dots.dot_count *= adj * 0.5;
			beat_dots.intensity *= adj;

			const r = (beat_dots.dot_rad * diagonal) | 0;			
			const flat_shading = (Math.random() < 0.25);

			const inv_r = 1.0 / r;
			
			if (blended_dot_params.radial_beat_dots > 0.001) {	
				const v = (beat_dots.intensity * blended_dot_params.radial_beat_dots) | 0;
				const circ_rad = 0.1 + 0.2 * Math.random(); //0.2 + 0.35 * Math.random();
				const count = (beat_dots.dot_count / 2) | 0;
				for (let i = 0; i < count; i++) {
					let theta = i * Math.PI * 2 / count;
					let fcx = Math.cos(theta) * circ_rad + blended_motion_params.blended_motion_cx;//wave_flat_cx;//params.blended_center_trace.cx;
					let fcy = Math.sin(theta) * circ_rad + blended_motion_params.blended_motion_cy;//wave_flat_cy;//params.blended_center_trace.cy;
					let xform = new TransformHelper(W, H);
					const cx = xform.NormToScreenX(fcx) | 0;
					const cy = xform.NormToScreenY(fcy) | 0;
					
		      dots.push(cx, cy, r * 2, v * (1.0 / 255));
				}
			}
		
			if (blended_dot_params.random_beat_dots > 0.001) {
				const v = (beat_dots.intensity * blended_dot_params.random_beat_dots) | 0;	
				for (let i = 0; i < beat_dots.dot_count; i++) {
					let cx = (Math.random() * (W - 1)) | 0;
					let cy = (Math.random() * (H - 1)) | 0;
		      dots.push(cx, cy, r * 1.3, v * (1.0 / 255));
				}
			}
		}

    // Update even when spawning is disabled: existing bokeh dots still advect.
    this.presenter.gpu_warp.fading_dots.set_bokeh_follow_motions(follow_motions);

    // Bokeh dots (stars):
    {
      // Present-only bokeh test. Set false to stop spawning; existing dots finish.
      const camera_focal_distance = 0.5; // [0..1]; animate or replace with a caller control.
      this.presenter.set_bokeh_frame(true_time, camera_focal_distance,
        bkg_color_adapted_to_headroom, hdr_calib_params.calibrating_hdr_headroom > 0,
        blended_dot_params.stars_opacity);
      if (!frozen && blended_dot_params.stars_opacity > 0.0) {
        // Fade-in needs to be short, so that during heavy zoom, you can even see them.
        // But they can both be short, because the defocus makes them essentially invisible @ start and end.
        let adj = 1.0 / user_motion_scale;

        const alive_count = this.presenter.gpu_warp.fading_dots.get_bokeh_alive_count();
        let target_count = Math.min(kBokehDotCapacity - 1, blended_dot_params.stars_population * 512);

        // Note: This will inherently be scaled by 'adj' already.
        let average_lifetime =
          this.presenter.gpu_warp.fading_dots.get_bokeh_average_lifespan()
          ?? kBokehParticleMaxLifetime;


        // The tentative spawn count (for this frame) is computed conservatively,
        // so that at the max allowed lifetime, they come out in a steady drip.
        let spawn_count = target_count / (fps * kBokehParticleMaxLifetime);
 
        // But the particle lifetime is short, we can increase this a bit.
        //xxx - TODO: when zoomy, increase spawn rate.        
        let t = average_lifetime / kBokehParticleMaxLifetime;   // [0..1]
        spawn_count *= 1.0 / Math.max(t, 0.25);

        //xxx
        //if ((frame % 10) == 0) console.log(`alive_count ${alive_count}, average_lifetime ${average_lifetime}, spawn ${spawn_count}`); //xxx

        // When lifetimes are long (t is ~1), spawn everywhere.
        // When lifetimes are medium (t is ~0.5), spawn almost everywhere.
        // When lifetimes are short (t is low), spawn very near the center.
        let max_rad = Math.min(1.0, Math.max(0.25, Math.pow(t, 0.5)));
        
        let int_spawn_count = Math.max(0, Math.floor(spawn_count + Math.random()));

        let bokeh_dots = [];
        for (let i = 0; i < int_spawn_count; i++) {
          let x = 0;
          let y = 0;
          do {
            x = (Math.random() * 2 - 1);
            y = (Math.random() * 2 - 1);
          } while (x * x + y * y > 1.0);

          x *= kMinMaxBokehDotXY * max_rad;
          y *= kMinMaxBokehDotXY * max_rad;
          //xxx - TODO: pass in per-star 'blended_dot_params.stars_opacity' here.
          bokeh_dots.push({
              x: x,
              y: y,
              //z: 1.0 - 0.1 * Math.random(),   // Start far away, get closer over time 
              //z2: 0.0 + 0.1 * Math.random(),   // End close to camera 
              z: 0.45 + 0.1 * Math.random(),   // Start far away, get closer over time 
              z2: 0.45 + 0.1 * Math.random(),   // End close to camera 
              use_background: Math.random() < 0.25,
              palette_position: (0.3 + Math.random() * 0.7),
              fade_in:  kBokehParticleFadeIn * adj,
              hold:     kBokehParticleHoldTime * adj,        
              fade_out: kBokehParticleFadeOut * adj,
          });
        }
        this.presenter.add_bokeh_dots(bokeh_dots, true_time);
      }
    }

    // Warp-dot positions, fading, respawning and advection now stay on the GPU.
    // Only these scalar controls cross from the CPU each frame.
    this.presenter.gpu_warp.set_fading_dots({
      strength: blended_dot_params.fading_dots,
      true_time,
      point_size: wave_point_size_in_pixels,
    });

		// Chasers
		for (let k = 0; k < 3; k++) {
			let str = 1.0;
			let step = 0.003;
			let phase_offset = 0;
			let time_scale = 1.0;
			let rad_scale = 1.0;
			if (k == 0) {
				str = blended_dot_params.chaser_1;
			} else if (k == 1) {
				str = blended_dot_params.chaser_2;
				phase_offset = 371.0;
			} else {
				str = blended_dot_params.dotty_chaser;
				step = 0.06;  // ~distance between the dots
				time_scale = 0.75;//1.0;//0.1;
				phase_offset = 150.0;
				rad_scale = 1.5;
			}

			if (str > 0.001) {
				// TODO:
				// 1. make it sometimes -- and fadeable.  3 versions:
				//      a. chaser 1
				//      b. chaser 2
				//      c. dotty chaser
				// 2. morph the cos wavelengths over time (blend between 2 sets)
				// 3. vary opacity (str)
				// 3. later: draw to 2nd color plane
				const speed = 2.5 * time_scale * 0.93;
				let xform = new TransformHelper(W, H);
				for (let i = 0; i < 10; i++) {
					let fx = Math.cos((true_time + i * step + phase_offset) * 1.732 * speed + 1) * 0.15;
					let fy = Math.cos((true_time + i * step + phase_offset) * 1.332 * speed + 3) * 0.15;
					fx += Math.cos((true_time + i * step + phase_offset) * 1.592 * speed + 2) * 0.2;
					fy += Math.cos((true_time + i * step + phase_offset) * 2.188 * speed + 0) * 0.2;
					fx *= rad_scale;
					fy *= rad_scale;
					let x = xform.NormToScreenX(fx);
					let y = xform.NormToScreenY(fy);	
					dots.push(x, y, wave_point_size_in_pixels * 2, str);
				}
			}
		}
		
		// Grid
		if (blended_dot_params.grid_dots > 0.001) {
			const diagonal = Math.sqrt(W * W + H * H) | 0;
			const points_along_diagonal = 24;
			const v = (96 * blended_dot_params.grid_dots) | 0;
			
			let phase = true_time * 0.7;
			phase -= Math.floor(phase);
			
			const step = Math.max(1, (diagonal / points_along_diagonal) | 0);
			//const start_x = (step * phase) | 0;
			//const start_y = (step / 2) | 0;
			const time = true_time * 0.55;
			const fx = (Math.cos(time * 0.537 + 1) + Math.cos(time * 0.359 + 2)) * 0.5;
			const fy = (Math.cos(time * 0.611 + 3) + Math.cos(time * 0.429 + 5)) * 0.4;
			const start_x = (((fx+1) * step * 3) | 0) % step;
			const start_y = (((fy+1) * step * 3) | 0) % step;
			
			for (let y = start_y; y < H; y += step) {
				for (let x = start_x; x < W; x += step) {
		      dots.push(x, y, wave_point_size_in_pixels, v * (1.0 / 255));
				}				
			}
		}
		
		// Embed song title.
		if (embed_string != "") {
			// Rarely: rebuild/upload bitmap
      if (g_title_x <= 0) {
        let tx = (Math.random() + Math.random() + Math.random()) * (1.0 / 3.0);
        let ty = (Math.random() + Math.random() + Math.random()) * (1.0 / 3.0);
        g_title_x = W * (0.4 + 0.2 * tx);
        g_title_y = H * (0.4 + 0.2 * ty);
      }
			const text_size = Math.max(8, diagonal * TITLE_EMBED_SIZE / Math.max(36, embed_string.length));
			this.presenter.gpu_warp.set_overlay_text(
				embed_string, 
				text_size|0, 
        true_time,
				{
				  center_x: g_title_x,
				  center_y: g_title_y,
				  intensity: 1.0,
				  font_family: "Arial",
				  font_weight: "bold",
				  supersample: 1,
				  duration: TITLE_EMBED_DURATION_SECONDS,
				  fade_in_power: TITLE_EMBED_FADE_IN_POWER,
				});			
		}
		
		let intensity = this.presenter.gpu_warp.text_overlay.get_intensity(true_time);
		if (intensity >= 1.0) {
			this.presenter.gpu_warp.burn_overlay_text(true_time, g_title_x, g_title_y, 1.0);
      g_title_x = -1;
      g_title_y = -1;
		}


    // Increase 'darkening' automatically when there is a lot of zoom motion
    // and we're using a single flat waveform (...presuming that it aligns with
    // the center of zoom motion).  Note that this would work better if we actually
    // tested whether or not the waveform (wherever it is and whatever shape it
    // is) for intersection with the center of zoom motion.
    const kZoomForNormalDarkening = 0.8;
    const kZoomForExtraDarkening = 2.0;
    const kMaxExtraDarkening = 0.25;
    // We want full darkening if it's a flat, mono wave.
    // If it's circular or stereo, we will attenuate it.
    let is_mono_flat_wave = 
        blended_wave_params.wave_flat_str * 
        (1.0 - blended_wave_params.wave_flat_is_stereo); 
    g_auto_darkening = 
        Math.max(0.0, Math.min(1.0, (blended_motion_params.blended_net_zoom_motion - kZoomForNormalDarkening) / (kZoomForExtraDarkening - kZoomForNormalDarkening)  )) 
            * kMaxExtraDarkening 
            * (0.5 + 0.5 * is_mono_flat_wave);

    
		// -------------------------------------------------------------------
		// -------------------------------------------------------------------
		// -------------------------------------------------------------------
		// Run the Warp on the GPU and then draw all this stuff into it..
		// TODO: add shift_mag, shift_time.
		// -------------------------------------------------------------------
		const repacked_points = new Float32Array(dots);
		if (!frozen) {
			this.presenter.warpAndDrawWaveform(
 	        gpu_warp_str, blended_shader_params, hdr_calib_params,
					shift_x, shift_y, repacked_points, darkening + g_auto_darkening, true_time,
          bkg_color_adapted_to_headroom[0], 
          bkg_color_adapted_to_headroom[1], 
          bkg_color_adapted_to_headroom[2],
          hdr_display_headroom);
		}
		// -------------------------------------------------------------------
		// -------------------------------------------------------------------
		// -------------------------------------------------------------------

		
    // Prune active warp maps history, to free memory.
    PrunePresets();

    // Swap
    const tmp = this.front;
    this.front = this.back;
    this.back = tmp;

    g_last_misc_debug_text  = `\n\n`;
    g_last_misc_debug_text += `blended_net_motion           ${blended_motion_params.blended_net_motion.toFixed(2)          }\n`;
    g_last_misc_debug_text += `blended_net_zoom_motion      ${blended_motion_params.blended_net_zoom_motion.toFixed(2)     }\n`;
    g_last_misc_debug_text += `blended_in_or_out_motion     ${blended_motion_params.blended_in_or_out_motion.toFixed(2)    }\n`;
    g_last_misc_debug_text += `blended_net_clockwise_motion ${blended_motion_params.blended_net_clockwise_motion.toFixed(2)}\n`;
    g_last_misc_debug_text += `blended_cw_or_ccw_motion     ${blended_motion_params.blended_cw_or_ccw_motion.toFixed(2)    }\n`;
    g_last_misc_debug_text += `blended_motion_cx   ${blended_motion_params.blended_motion_cx.toFixed(2)           }\n`;
    g_last_misc_debug_text += `blended_motion_cy   ${blended_motion_params.blended_motion_cy.toFixed(2)           }\n`;
    g_last_misc_debug_text += `g_auto_darkening    ${g_auto_darkening.toFixed(2)}`;
  }

	// TODO: move to wave_draw.js
  generateWaveformPoints(params, point_size_in_pixels, wave_smoothing, xy_oscilloscope_gap,
                         wave_opacity_scale, wave_ampl_scale, align_frac, audioFrame, experiment) {
    const a = audioFrame;
    if (!a) return;

		// TODO: make this fixed size so it's not constantly reallocating.
		let buf = [];

    const W = this.iw;
    const H = this.ih;
    const wave = a.wave; // Int16Array [-16384..16384]
    const rms = a.rms;   // 0..1
		
		// The largest # of samples we'll need is
		// proportional to the diagonal of the image. 
		const diagonal = Math.sqrt(W * W + H * H) | 0;
		
		//const align_frac = experiment ? 0.55 : ALIGN_FRAC;
		
		// This will return the 75% of the wave back that
		// aligns best to the previous (aligned) wave.
		let waveform = null;
		if (params.wave_flat_str > 0 || params.wave_circ_str > 0) {
			// Align and smooth.
			let aligned = aligner.alignFromF(wave, align_frac, wave.length);
			waveform = Blur(aligned, wave_smoothing);	
		}

		if (params.wave_flat_str > 0) {
			// Flat waveform

			// Angle:
			const cosx = Math.cos(params.wave_flat_angle);
			const sinx = Math.sin(params.wave_flat_angle);

			// If we don't have enough samples to fill the screen, upsample it.
			const target_sample_count = (diagonal * WAVE_STRETCH * (align_frac/0.55) / point_size_in_pixels) | 0;
			let resampled = waveform;
			if (resampled.length != target_sample_count) {
				resampled = linearResampleF(resampled, target_sample_count);
			}
	    const N = resampled.length;

			const sep = params.wave_flat_is_stereo * params.wave_flat_stereo_sep;

			const amplitude = 2.0 * 0.7 * params.wave_flat_scale * wave_ampl_scale *
					// quickly decrease amplitude when stereo separation is active:
					(1.0 - 0.7 * Math.sqrt(sep));

			// Opacity:
	    const mag0 = Math.min(255, (Math.min(255, rms * 20000 + 16) * params.wave_flat_str)) * wave_opacity_scale * WAVE_MAG_SCALE;

			let cx = params.wave_flat_cx;
			let cy = params.wave_flat_cy;

			// Slide the wave, if needed, so that we see the full thing.
			{
				let t = cx * cosx + cy * sinx;
				cx -= cosx * t;
				cy -= sinx * t;
			}


			let xform = new TransformHelper(W, H);
			let midx = xform.NormToScreenX(cx);
			let midy = xform.NormToScreenY(cy);			
	    
	    // Just pluck the center of the aligned part:
	    const sample0 = Math.max(0, N - target_sample_count) / 2;
	    const sample_count = Math.min(target_sample_count, N);

			const second_channel_opacity = Math.max(0.0, params.wave_flat_is_stereo * 2 - 1);
	    
	    for (let pass = 0; pass < 2; pass++) {
	    	if (pass == 1 && (params.wave_flat_is_stereo < 0.0001 || params.wave_flat_stereo_sep < 0.0001)) {
	    		break;
	    	}
	    	
	    	let mag = mag0;
	    	let stereo_offset = 0.0;
	    	
	    	if (pass == 1) {
	    		// Fade the second wave in late and out early.
	    		mag = (mag * second_channel_opacity) | 0;
	    	}
    		stereo_offset = ((-0.5 + pass) * sep);
	    
	    	const amplitude2 = amplitude * 
	    			(second_channel_opacity * params.wave_flat_stereo_amplitude_scale +
	    			 (1.0 - second_channel_opacity) * 1.0);
	    
	    	mag = Math.min(mag * (1.0 / 255), 1.0);
	    
		    for (let k = 0; k < sample_count; k++) {
		      const i = (k + sample0) | 0;
		      const v = resampled[i] * amplitude2; // -1..1
		
					// The pixel coordinates (relative to the center of the screen)
					// of the un-rotated wave:
					let x0 = (k - sample_count * 0.5) * point_size_in_pixels;
					let y0 = (v + stereo_offset) * (diagonal * 0.3);
		
					// Take rotation of point (x0, y0) by 'angle'; 
					let fx = x0 * cosx - y0 * sinx;
		      let fy = x0 * sinx + y0 * cosx;
		      	      
		      let x = midx + fx;
		      let y = midy + fy;	      
		      buf.push(x, y, point_size_in_pixels, mag);
		    }
		  }
	  }

		if (params.wave_circ_str > 0) {
			// Circular waveform

			const circ_rad = params.wave_circ_rad * 1.15;		// 0.8 ... 1.5
			const circ_ampl_scale = params.wave_circ_scale * 0.85;	// 0.5 ... 1.2

			// This will return the 75% of the wave back that
			// aligns best to the previous (aligned) wave.

			// Use half as many samples for circular waves; otherwise, they draw
			// too cluttered and dense.
			const target_sample_count = (diagonal * 0.45) | 0;

			// If we don't have enough samples to fill the screen, upsample it.
			let resampled = waveform;
			if (resampled.length != target_sample_count) {
				resampled = linearResampleF(resampled, target_sample_count);
			}
	    const N = resampled.length;

			const amplitude = 2.2 * params.wave_circ_scale * params.wave_circ_rad * wave_ampl_scale;

			// Opacity:
	    let mag = Math.min(255, (Math.min(255, rms * 20000 * 0.16 + 12) * 0.6) * params.wave_circ_str) | 0; 
	    mag = Math.min(mag * (1.0 / 255), 1.0) * wave_opacity_scale;

			let xform = new TransformHelper(W, H);
			const midx = xform.NormToScreenX(params.wave_circ_cx);
			const midy = xform.NormToScreenY(params.wave_circ_cy);
	    
	    // Just pluck the center of the aligned part:
	    const sample0 = (Math.max(0, N - target_sample_count) / 2) | 0;
	    const sample_count = Math.min(target_sample_count, N);
	    const overlap = (sample_count / 8) | 0;
	    
	    for (let k = 0; k < sample_count - overlap; k++) {
	      const i = (k + sample0) | 0;
	      let v = resampled[i] * amplitude; // -1..1
	      
	      if (k < overlap) {
	      	const t = k * (1.0 / overlap);
	      	const i2 = (k + sample0 + sample_count - overlap) | 0;
	      	const v2 = resampled[i2] * amplitude;
	      	v = v2 * (1 - t) + v * (t);
	      }
	
				// The pixel coordinates (relative to the center of the screen)
				// of the un-rotated wave:
				let fx = k - sample_count * 0.5;
				let fy = v * (diagonal * 0.3);
	
	      // Make circular.
	      {
	      	let t = k * (1.0 / (sample_count - overlap));  // [0..1]
	      	let r = diagonal * (0.1 * circ_rad) + fy * (0.2 * circ_ampl_scale);  //TWEAK	      	
	      	fx = r * Math.cos(t * (3.1415927 * 2) + params.wave_circ_angle);
	      	fy = r * Math.sin(t * (3.1415927 * 2) + params.wave_circ_angle);
	      }

	      let x = midx + fx;
	      let y = midy + fy;	      
	      buf.push(x, y, point_size_in_pixels, mag);
	    }
		}
		
	  if (0) {
	  	// XY oscilloscope

			//let aligned = linearUpsampleF(wave, wave.length * 2);


// We probably need to smooth (upsample) AFTER the blur.  Perhaps multiple passes.
// Ideal:
//    1. choose the ideal gap.  Small pref to match prev frame.
//    2. try to align to previous frame.
//    3. draw it


// TODO: Does auto gap code work well?  Play with manually setting, vs. it.
	  	let gap = xy_oscilloscope_gap;
	  	if (1) {
	  		// Determine optimal gap.
// TODO: Factor out into a function.
	  		let best_gap = 4;
	  		let best_avg = 0;
	  		for (let gap = 32; gap < 192; gap += 4) {
	  			let sum = 0;
	  			let samples = 0;
	  			let prev_sum = 0;

  				let p_prev = { x : waveform[0], y : waveform[gap] };

	  			for (let j = 0; j < waveform.length - gap; j += 4) {
	  				let p = { x : waveform[j], y : waveform[j + gap] };

						// Take the cross product of this point and the previous point.
						// This should give a high value when faraway points make angular movement,
						//   and a low value when the points are close, or don't make angular
						//   movement.
						sum += Math.abs(p.x * p_prev.y - p.y * p_prev.x);  // 2D cross product ~ area

						p_prev = p;
					
						samples++;
	  			}
	  			let avg = sum / samples;
	  			
	  			if (avg > best_avg) {
	  				best_avg = avg;
	  				best_gap = gap;
	  			}
	  		}
	  		//console.log(`best_gap ${best_gap}, best_avg ${best_avg}`);
	  		gap = best_gap;
	  	} 

			// If we don't have enough samples to fill the screen, upsample it.
			//let old_len = waveform.length;
			//if (old_len.length < 2048) {
			//	waveform = linearUpsampleF(waveform, 2048);
			//	gap = (gap * waveform.length / old_len) | 0;
			//}
	    const N = waveform.length;

			// Opacity:
	    //let mag = Math.min(63, rms * 20000 + 16);
	  	
	  	const xy_amplitude = 1.0 * 0.75 * wave_ampl_scale;
	  	const max_count = 850;  //TWEAK
	  	const count = Math.min(N - gap, max_count);

			/*
			// Pass 1: determine the average radius of the points we'll draw,
			// relative to the screen.
			let avg_rad = 0.0;
			let fx = new Float32Array(count);
			let fy = new Float32Array(count);
	    for (let i = 0; i < count; i++) {
	      let ix = waveform[i];
	      let iy = waveform[i + gap];
	      const vx = ix * xy_amplitude; // -1..1
	      const vy = iy * xy_amplitude; // -1..1
	      const rad = Math.sqrt(vx * vx + vy * vy);
	      avg_rad += rad;
	      fx[i] = vx;
	      fy[i] = vy;
	    }
	    avg_rad /= count;
	    */

			// Determine the opacity.
			//console.log(`avg_rad ${avg_rad}`);
			//let mag = 16;//Math.max(1, Math.min(255, avg_rad * avg_rad * 300));

			// Pass 2: draw it.
			for (let i = 0; i < count; i++) {
	      let fx = waveform[i      ] * xy_amplitude;
	      let fy = waveform[i + gap] * xy_amplitude;
	      
	      //let rad = Math.sqrt(fx * fx + fy * fy);
	      //fx *= Math.pow(rad, -0.5) * 0.3;
	      //fy *= Math.pow(rad, -0.5) * 0.3;
	      
	      let vx = fx * (diagonal * 0.5);
	      let vy = fy * (diagonal * 0.5);
	      vx = (W * 0.5 + vx) | 0;
	      vy = (H * 0.5 + vy) | 0;
	      //const vx = (W / 2 + ix * amplitude * H * 0.5) | 0; // -1..1
	      //const vy = (H / 2 + iy * amplitude * H * 0.5) | 0; // -1..1
	      let rad_sq = (fx * fx + fy * fy);
	      let r = rad_sq;//Math.pow(rad_sq, 1.5) * 32;
	      //let mag = Math.max(1, Math.min(32, r * 4096)) | 0;
	      let mag = Math.max(8, Math.min(128, r * 8000 * WAVE_MAG_SCALE)) | 0;
	
	      //if (vx < 0 || vy < 0 || vx >= W || vy >= H) continue;	
	      //const idx = vy * W + vx;	
	      //buf[idx] = Math.min(255, buf[idx] + mag);
	      
	      buf.push(vx, vy, point_size_in_pixels, mag * (1.0 / 255));
	    }
	  }

		return buf;	  
  }   	 


}

