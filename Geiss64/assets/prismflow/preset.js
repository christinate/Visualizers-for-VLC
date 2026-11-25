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

import { kMaxActiveWarps, kWarpMapSize } from "./const.js"
import { g_warp_maps, buildWarpMap, 
         kModePairIsSuperGood, kModePairIsGood, kModePairIsBad, kModePairIsForbidden,
         kSuperGoodModePairWeightScale, kGoodModePairWeightScale, kBadModePairWeightScale, kForbiddenModePairWeightScale } from "./motion.js";
import { GenerateRandomPalette, ExpandPalette } from "./palette.js"

// When chasers are selected, how often [0..1] should it be 2 instead of 1?
const kTwoChasersProb = 0.25;

// A threshold for what is considered strong zoom in/out motion.
const kZoomThreshScale = 1.8;

function RandBetween(lo, hi) {
	return lo + (hi - lo) * Math.random();
}

function smoothstep(u) {
  return u * u * (3 - 2 * u);
}

// Bias 'theta' toward the nearest multiple of Pi, using smoothstep.
// 'iters' can be non-integer but must be between 0 and 10.
function BiasTowardHorizontal(theta, iters = 1.0) {
	iters = Math.max(0.0, Math.min(10.0, iters));
	const t = theta / Math.PI;
	const base = Math.floor(t);
	let dt = t - base;
	
	while (iters > 0.0) {
		const dt2 = smoothstep(dt);
		const write_frac = Math.min(iters, 1.0);
		dt = dt2 * write_frac + (1.0 - write_frac) * dt;		
		iters -= 1.0;				
	}

	return (base + dt) * Math.PI;
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function log_lerp(a, b, t) {
  if (a <= 0 || b <= 0) {
    throw new RangeError(`log_lerp requires positive, nonzero endpoints: a=${a}, b=${b}`);
  }
  return a * Math.pow(b / a, t);
}


// Note: The weights do not need to sum to 1.
export function blendStructs(prefs, weights) {
  if (prefs.length !== weights.length) {
    throw new Error(`blendStructs: prefs.length (${prefs.length}) != weights.length (${weights.length})`);
  }
  if (prefs.length === 0) {
    throw new Error("blendStructs: empty prefs array");
  }

  const wsum = weights.reduce((a, b) => a + b, 0);
  if (wsum === 0) {
    throw new Error("blendStructs: weight sum is 0");
  }

  const out = {};

  // Union of keys across all pref objects
  const keys = new Set();
  for (const p of prefs) {
    for (const k of Object.keys(p)) keys.add(k);
  }

  for (const k of keys) {
    let acc = 0;
    let sawNumber = false;

    for (let i = 0; i < prefs.length; i++) {
      const v = prefs[i][k];
      if (typeof v === "number") {
        acc += weights[i] * v;
        sawNumber = true;
      }
    }

    // Numeric keys get blended; non-numeric keys copied from prefs[0] (policy choice).
    out[k] = sawNumber ? (acc / wsum) : prefs[0][k];
  }

  return out;
}

export class Warp {
  // The motion will be determined by 'mode'.  The weight of that motion will be attenuated by 'str'.
  constructor(mode = 0, str = 1.0, warp_map = null, rand_seed = 0) {
    this.mode = mode;
    this.rand_seed = rand_seed;  //xxx - use it!
    this.str = str;
    this.warp_map = warp_map;    // type: ActiveWarpMap2 -- but remove the timing stuff (t0 to t3)
  }

  GetMeanStr() {
    return this.str;//Math.sqrt(this.str_min * this.str_max);
  }
}

// NOTE: These 'params' structs are broken up into the groups 
// that they will be locked/unlocked in:
// motion, waveform, other drawn effects, shader effects (and palette).
// Key plan:
//   l = lock all
//   m = motion
//   w = waveform
//   d ?= dots
//   s ?= shader
export class MotionParams {
  constructor() {
    // Motion metadata - these are updated after the warp maps are generated,
    // and are always a blend of the metadata from the 4 blended warp maps.
    this.blended_net_motion = 0.0;
    this.blended_net_zoom_motion = 0.0;
    this.blended_in_or_out_motion = 0.0;
    this.blended_net_clockwise_motion = 0.0;
    this.blended_cw_or_ccw_motion = 0.0;
    this.blended_motion_cx = 0.0;
    this.blended_motion_cy = 0.0;
  }
}

// Waveform-drawing metadata.
export class WaveParams {
  constructor() {
    this.wave_flat_str   = 0.0;		  // [0..1]
  	this.wave_flat_angle = 0.0;	  	// [0..2PI]
  	this.wave_flat_scale = 1.0;      // ~1
  	this.wave_flat_is_stereo = 0;		// [0..1]
  	this.wave_flat_stereo_sep = 0.6;  // [0..1]
  	this.wave_flat_stereo_amplitude_scale = 1.0;  // ~1
  	this.wave_flat_cx    = 0.0;		// [1..1]
  	this.wave_flat_cy    = 0.0;		// [1..1]
  	
  	this.wave_circ_str   = 0.0;		// [0..1]
  	this.wave_circ_rad   = 1.0;		// ~1
  	this.wave_circ_scale = 1.0;    // ~1
  	this.wave_circ_angle = 0.0;    // [0 .. 2PI]
  	this.wave_circ_cx    = 0.0;		// [1..1]
  	this.wave_circ_cy    = 0.0;		// [1..1]
  }
}

// Other visual effects that are drawn into the image.
export class DotParams {
  constructor() {
    // Non-wave visual effects - mostly dots ('d')
  	this.radial_beat_dots = 0.0;		// Note: further reduced dynamically when motion is not zoomy.
  	this.random_beat_dots = 0.0;		
  	this.fading_dots      = 0.0;   // aka starfield.
  	this.grid_dots        = 0.0;
  	this.chaser_1         = 0.0;   // These always orbit around the center of the image (regardless of motion center).
  	this.chaser_2         = 0.0;   // These always orbit around the center of the image (regardless of motion center).
  	this.dotty_chaser     = 0.0;   // These always orbit around the center of the image (regardless of motion center).
  }
}

// Shader variables.
export class ShaderParams {
  constructor() {
    // Paint settings that affect the warp shader ('s')
    this.rx_diff_grey_scott_str  = 0.0;
    this.rx_diff_grey_scott_str2 = 0.0;
    /*
    this.diffusion_colorization_1 = 0.0;
    this.diffusion_colorization_2 = 0.0;
    this.diffusion_colorization_3 = 0.0;
    this.diffusion_colorization_4 = 0.0;
    this.diffusion_colorization_5 = 0.0;
    this.diffusion_colorization_6 = 0.0;
    this.diffusion_colorization_7 = 0.0;
    this.diffusion_colorization_8 = 0.0;
    */
    this.rx_diff_barkley_str    = 0.0;
    this.frequency_injector_str = 0.0;
    this.frequency_injector_freq_scale = 0.0;
    this.painterly_str          = 0.0;
    this.sharpen_str            = 0.0;
    this.noise_str              = 0.0;
    this.layer2_warp_scale      = 0.0;
    this.layer1_density         = 0.0;
    this.layer2_brightness      = 0.0;

    // TODO: Add decay / exposure aggressiveness?
  }
}

export class Preset {
  constructor(global_preset_number, t1, t2, override_mode = -1) {
    // Creates four independent Warp objects.
    this.global_preset_number = global_preset_number;

    this.warps = Array.from(
      { length: kMaxActiveWarps },
      () => new Warp()
    );
    this.warp_maps_ready = false;
    this.rebuild_id = -1;

    this.palette_orig = null;
    this.palette_expanded = null;

    this.motion_params = new MotionParams();
    this.shader_params = new ShaderParams();
    this.dot_params = new DotParams();
    this.wave_params = new WaveParams();

    this.t1 = t1;   // The time at which it starts to fade in.  (weight at t1 = 0%)
    this.t2 = t2;   // The time at which it is fully faded in.  (weight at t2 = 100%)
    // The fade-in and fade-out times will be determined by these times, plus 't1' of the following preset.
  
    // This must be called first, because the other parameters will be
    // influenced by the motions chosen.
    this.RandomizeMotion(override_mode);

    this.RandomizeWaveParams();
    this.RandomizeDotParams();
    this.RandomizeShaderParams();

    this.RandomizePalette();
  }

  RandomizePalette() {
    // The original palette - which is a { palette, name } pair, where the palette is usually ~8 RGB colors or less.
    // Note that this might sometimes be null -- like when you blend 2 presets together.
    this.palette_orig = GenerateRandomPalette();

    this.ExpandPalette();
  }

  ExpandPalette() {
    // This is unnamed, and is just an array of 1024 RGBA values in [0..1].
    this.palette_expanded = ExpandPalette(this.palette_orig.palette);
  }

  // Call this when the warp maps were nuked to save memory, but you want to re-generate them to use the preset again.
  // Note that if calling this from a background thread, then "this" is a COPY of the main-thread preset.
  RebuildWarpMaps(calling_from_background_thread = false) {
    //console.log(`### RebuildWarpMaps`);

    if (this.warp_maps_ready) {
      console.log("ERROR in RebuildWarpMaps: warp_maps_ready was already true.");
      return;
    }

    // This path is normally called by the background "rebuild" worker thread.
    for (let i = 0; i < kMaxActiveWarps; i++) {
      if (this.warps[i].warp_map != null && 
          this.warps[i].warp_map.src_dxy == null &&
          this.warps[i].warp_map.dynamic_motion_params == null) {
        //console.log(`+ rebuilding warp map - seed ${this.warps[i].warp_map.seed}`);//xxx
        this.warps[i].warp_map = buildWarpMap(this.warps[i].mode, this.warps[i].warp_map.seed);
      }
    }
    //console.log(`# done rebuilding warp maps for preset #${this.global_preset_number}, rebuild_id ${this.rebuild_id}`);
    this.warp_maps_ready = true;

    if (!calling_from_background_thread) {
      this.rebuild_id = -1;  // Invalidate any already-launched background warp map rebuilds.
    }
  }

  ReleaseWarpMaps() {
    // Keep tiny dynamic descriptors; only static pixel arrays need pruning.
    for (let i = 0; i < kMaxActiveWarps; i++) {
      if (this.warps[i].warp_map != null && 
          this.warps[i].warp_map.src_dxy != null) {
        //console.log(`- clearing warp map`);//xxx
        this.warps[i].warp_map.src_dxy = null;
      }
    }
    this.warp_maps_ready = this.warps.every(w => w.warp_map != null &&
      (w.warp_map.src_dxy != null || w.warp_map.dynamic_motion_params != null));
  }

  SetWarpSlotDebug(dbg_warp_slot, mode) {
    // Reset this, so that if there was a warp map rebuild happening in the background, 
    // it will be ignored when it comes back.
    this.rebuild_id = -1;

    this.warps[dbg_warp_slot].mode = mode;
    this.warps[dbg_warp_slot].warp_map = buildWarpMap(mode);

    this.UpdateStarsFollowMotion(false);
    this.ExtractMotionMetadata();

    this.warp_maps_ready = true;
  }

  RandomizeMotion(override_mode = -1) {
    // Note: Don't use simple assignment; other references to the object will become stale.
    Object.assign(this.motion_params, new MotionParams());

    // Reset this, so that if there was a warp map rebuild happening in the background, 
    // it will be ignored when it comes back.
    this.rebuild_id = -1;

    // Pick the warp modes.
    if (override_mode >= 0) {
      this.warps[0].mode = override_mode;
      this.warps[0].str = 1.0;
      for (let i = 1; i < kMaxActiveWarps; i++) {
        this.warps[i].mode = 0;
        this.warps[i].str = 0.0;
      }
    } else {
      let other_modes = new Array();
      for (let i = 0; i < kMaxActiveWarps; i++) {
        let mode = PickRandomWarpMap(other_modes);
        other_modes.push(mode);

        this.warps[i].mode = mode;
        this.warps[i].str = 1.0;
      }
    }

    /*    
    // One-time rebalancing code for g_warp_maps[].weight, when going from old system
    // (indep. sel) vs new system (probs * good/bad/forbidden matrix).
    let orig_weights = structuredClone(g_warp_maps);
    let old_total = 0.0;
    for (let i = 0; i < g_warp_maps.length; i++) {
      old_total += g_warp_maps[i].weight;
    }
    for (let k = 0; k < 100; k++) {
      // Using the current probs, measure how often each mode is chosen.
      let counts = new Float32Array(g_warp_maps.length).fill(0.0);
      let total = 0.0;
      for (let z = 0; z < 10000; z++) {
        let other_modes = new Array();
        for (let i = 0; i < kMaxActiveWarps; i++) {
          let mode = PickRandomWarpMap(other_modes);
          counts[mode] += 1.0;
          total += 1.0;
          other_modes.push(mode);
        }
      }
      for (let i = 0; i < counts.length; i++) {
        let target_prob = orig_weights[i].weight / old_total;
        let new_prob = counts[i] / total;

        if (k == 99) {
          console.log(`${i}  target ${target_prob.toFixed(4)}  actual ${new_prob.toFixed(4)}  weight ${g_warp_maps[i].weight.toFixed(2)}`);
        } else {
          g_warp_maps[i].weight *= target_prob / new_prob;
        }
      }
    }
    */

    // Generate the 4 motion maps.
    // ---------------------------------------------
    // This should create a random preset and enqueue it for bkg-generation.
    // The blended motion metadata analysis (below) should also be moved in there, at the end.
    // When it's time to use the motion maps, we'll block if needed.
    for (let i = 0; i < kMaxActiveWarps; i++) {
      let mode = this.warps[i].mode;
      //console.log(`Generating warp map[${i}], mode ${mode}.`);
      this.warps[i].warp_map = buildWarpMap(mode);
    }

    this.UpdateStarsFollowMotion(true);
    this.ExtractMotionMetadata();

    this.warp_maps_ready = true;
  } // end RandomizeMotion

  ExtractMotionMetadata() {
    // Get the 4 *motion* weights, using the (geometric) mean of str_min and str_max.
    const weights = this.warps.map(w => w.GetMeanStr());

    // Extract Motion Metadata
    // --------------------------------------------------
    // Measure the 7 motion metadata values from the *combined* 4 weighted warps,
    // by sampling some of the motion.

    // Reminder that W,H here are both kWarpMapSize.
    const W = kWarpMapSize;
    const H = kWarpMapSize;
    
    // Produce a single, weight-blended motion map.
    const src_dxy = new Float32Array(W * H * 2);
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        const offset = (y * W + x) * 2;
        let dx = 0.0;
        let dy = 0.0;
        for (let i = 0; i < kMaxActiveWarps; i++) {
          // Metadata describes only static motion; dynamic fields vary with time.
          const src = this.warps[i].warp_map.src_dxy;
          if (src == null) continue;
          dx += src[offset + 0] * weights[i];
          dy += src[offset + 1] * weights[i];
        }
        src_dxy[offset] = dx;
        src_dxy[offset + 1] = dy;
      }
    }

    // Measure.
    const diagonal = Math.sqrt(W * W + H * H);
    const sample_count = 256;
    const scale = 100.0 / diagonal;
    const cx = W / 2;
    const cy = H / 2;
    let net_motion = 0.0;
    let net_zoom_motion = 0.0;
    let in_or_out_motion = 0.0;
    let net_clockwise_motion = 0.0;
    let cw_or_ccw_motion = 0.0;
    for (let k = 0; k < sample_count; k++) {
      // Pick a random pixel on the (square) warp map.
      // Bias samples toward the center (pyramid-shaped distribution).
      const fx = (Math.random() + Math.random()) * 0.5;
      const fy = (Math.random() + Math.random()) * 0.5;
      const dx = (fx * (W - 1)) | 0;
      const dy = (fy * (H - 1)) | 0;
      const read_offset = (dy * W + dx) * 2;
      const sx = dx + src_dxy[read_offset + 0] * (0.5 * W);
      const sy = dy + src_dxy[read_offset + 1] * (0.5 * H);
      
      // r: vector from center of screen, to the destination point,
      // *normalized to unit length*.  We can dot against this
      // vector to determine how radial motion is.
      let rx = dx - cx;
      let ry = dy - cy;
      let r_norm_scale = 1.0 / Math.sqrt(rx * rx + ry * ry + 0.00001);
      rx *= r_norm_scale;
      ry *= r_norm_scale;
  
      // t: tangent vector.  Also of unit length.
      // We can dot against this vector to determine how angular 
      // the motion is.
      const tx = -ry;
      const ty = rx;
      
      // v: vector from the source point, to the destination point -
      // i.e. the direction of motion, at this point on the screen.
      const vx = (dx - sx) * scale;
      const vy = (dy - sy) * scale;

      // This dot product (v dot r) gives us the amount of radial motion.
      const outward   = rx * vx + ry * vy;
  
      // This dot product (v dot t) gives us the amount of clockwise (angular) motion.
      const clockwise = tx * vx + ty * vy;
  
      net_motion           += Math.sqrt(vx * vx + vy * vy);
      net_zoom_motion      += outward;
      in_or_out_motion     += Math.abs(outward);
      net_clockwise_motion += clockwise;
      cw_or_ccw_motion     += Math.abs(clockwise);
    }
    this.motion_params.blended_net_motion           = net_motion / sample_count;
    this.motion_params.blended_net_zoom_motion      = net_zoom_motion / sample_count;
    this.motion_params.blended_in_or_out_motion     = in_or_out_motion / sample_count;
    this.motion_params.blended_net_clockwise_motion = net_clockwise_motion / sample_count;
    this.motion_params.blended_cw_or_ccw_motion     = cw_or_ccw_motion / sample_count;

    // Also compute weight-blended motion_cx|cy, the self-reported
    // center of motion of the 4 warp maps.
    let motion_cx_sum = 0.0;
    let motion_cy_sum = 0.0;
    let weight_sum = 0.0;
    for (let i = 0; i < kMaxActiveWarps; i++) {
      motion_cx_sum += this.warps[i].warp_map.warp_prefs.motion_cx * weights[i];
      motion_cy_sum += this.warps[i].warp_map.warp_prefs.motion_cy * weights[i];
      weight_sum += weights[i];
    }
    this.motion_params.blended_motion_cx = motion_cx_sum / weight_sum;
    this.motion_params.blended_motion_cy = motion_cy_sum / weight_sum;
  }

  GetBlendedPrefs(apply_wave_prefs_weight) {
    let weights = this.warps.map(w => w.GetMeanStr());

    if (apply_wave_prefs_weight) {
      for (let i = 0; i < this.warps.length; i++) {
        weights[i] *= this.warps[i].warp_map.warp_prefs.wave_prefs_weight;
        //console.log(`${i}: mode ${this.warps[i].warp_map.mode}, wave_prefs_weight ${this.warps[i].warp_map.warp_prefs.wave_prefs_weight}, type_circ_prob ${this.warps[i].warp_map.warp_prefs.type_circ_prob}`); //xxx
      }
    }

    // Blend the "warp_prefs" structs for each warp map together,
    // using the relative weights.
    let prefs = new Array(kMaxActiveWarps);
    for (let i = 0; i < kMaxActiveWarps; i++) {
      prefs[i] = this.warps[i].warp_map.warp_prefs;
    }
    const p = blendStructs(prefs, weights);

    // Manually blend the "stars" values.
    {
      let w_sum = 0.0;
      for (let i = 0; i < kMaxActiveWarps; i++) {
        w_sum += this.warps[i].warp_map.warp_prefs.stars_voting_weight;
      }
      p.stars_prob       = 0.0;
      p.stars_population_min = 0.0;
      p.stars_population_max = 0.0;
      p.stars_opacity_min    = 0.0;
      p.stars_opacity_max    = 0.0;
      // note: skip 'stars_follow_motion' here -- they are used directly, per warp map.
      for (let i = 0; i < kMaxActiveWarps; i++) {
        let w = this.warps[i].warp_map.warp_prefs.stars_voting_weight;
        p.stars_prob           += this.warps[i].warp_map.warp_prefs.stars_prob           * (w / w_sum);
        p.stars_population_min += this.warps[i].warp_map.warp_prefs.stars_population_min * (w / w_sum);
        p.stars_population_max += this.warps[i].warp_map.warp_prefs.stars_population_max * (w / w_sum);
        p.stars_opacity_min    += this.warps[i].warp_map.warp_prefs.stars_opacity_min    * (w / w_sum);
        p.stars_opacity_max    += this.warps[i].warp_map.warp_prefs.stars_opacity_max    * (w / w_sum);
        // note: skip 'stars_follow_motion' here -- they are used directly, per warp map.
      }
    }

    //xxx
    //console.log(`  warp_prefs.type_flat_prob = ${p.type_flat_prob}`);
    //console.log(`  warp_prefs.type_circ_prob = ${p.type_circ_prob}`);
    //console.log(`  warp_prefs.type_flat_plus_circ = ${p.type_flat_plus_circ}`);

    return p;
  }

  RandomizeWaveParams() {
    // Note: Don't use simple assignment; other references to the object will become stale.
    Object.assign(this.wave_params, new WaveParams());

    // Pick waveform(s) and effects to draw.

    let p = this.GetBlendedPrefs(true);

    // Compute the *weighted sum* (not weighted average) 
    //   of the motion metadata for the kMaxActiveWarps active warp maps,
    //   with their current weights, to get the net motion
    //   at the desired (~lookahead) time.
    // Then use the net motion metadata to decide whether we 
    //   use a flat or circular wave.
    let flat_w = 1.0;   // Probability of selecting a flat wave
    let circ_w = 0.25;  // Probability of selecting a circular wave
    let both_w = 0.01;  // Probability of selecting BOTH at the same time
    {
      // Crush the circular wave probability to 0 unless we have
      // a lot of in or out motion.
      {
        const lo = kZoomThreshScale * 0.2;
        const hi = kZoomThreshScale * 0.35;
        let t = Math.max(0.0, Math.min(1.0, (this.motion_params.blended_in_or_out_motion	- lo) / (hi - lo)));	
        circ_w *= t;
        both_w *= t;
      }

      // If very zoomy, (randomly) decrease the stereo separation.
      // TODO: Also decrease the amplitude -- but only for the stereo case.
      {
        const lo = kZoomThreshScale * 0.25;
        const hi = kZoomThreshScale * 0.35;
        let t = Math.max(0.0, Math.min(1.0, (this.motion_params.blended_net_zoom_motion	- lo) / (hi - lo)));	
        t = 1 - 0.5 * t * Math.random();
        p.flat_stereo_sep_lo *= t;
        p.flat_stereo_sep_hi *= t;
      }

      // If VERY zoomy, (randomly) decrease the circle radius.
      {
        const lo = kZoomThreshScale * 0.3;
        const hi = kZoomThreshScale * 0.5;
        let t = Math.max(0.0, Math.min(1.0, (this.motion_params.blended_net_zoom_motion	- lo) / (hi - lo)));	
        t = 1 - 0.5 * t * Math.random();
        p.circ_rad_lo *= t;
        p.circ_rad_hi *= t;
      }
      
      //warp_prefs.type_circ_prob = 0.01 + warp_prefs.type_flat_prob * very_zoomy;
    }

    //console.log(`${p.use_motion_metadata_to_choose_wave_type}`); //xxx
    if (p.use_motion_metadata_to_choose_wave_type < 1.0) {
      let t = p.use_motion_metadata_to_choose_wave_type;
      flat_w = flat_w * t + (1.0 - t) * p.wave_flat_prob;
      circ_w = circ_w * t + (1.0 - t) * p.wave_circ_prob;
      both_w = both_w * t + (1.0 - t) * p.wave_flat_plus_circ_prob;
    }

    flat_w = Math.max(flat_w, 0.0000001);
    circ_w = Math.max(circ_w, 0.0000001);
    both_w = Math.max(both_w, 0.0000001);

    // Choose the wave type: flat, circular, or both -
    // and set the strength values accordingly.
    let t = Math.random() * (flat_w + circ_w + both_w);
    if (t < flat_w) {
      this.wave_params.wave_flat_str = 1.0;
    } else if (t < flat_w + circ_w) {
      this.wave_params.wave_circ_str = 1.0;
    } else {
      this.wave_params.wave_flat_str = 1.0;
      this.wave_params.wave_circ_str = 1.0;
    }
    
    if (this.wave_params.wave_flat_str > 0.0001) {
      this.wave_params.wave_flat_angle = BiasTowardHorizontal(
          RandBetween(p.flat_angle_lo, p.flat_angle_hi), 
          p.flat_angle_bias_toward_horizontal_angles);
      this.wave_params.wave_flat_scale = p.flat_scale;
      this.wave_params.wave_flat_is_stereo = (Math.random() < p.flat_is_stereo_chance) ? 1 : 0;
      this.wave_params.wave_flat_stereo_sep = RandBetween(p.flat_stereo_sep_lo, p.flat_stereo_sep_hi);
      this.wave_params.wave_flat_stereo_amplitude_scale = p.flat_stereo_amplitude_scale;
      // Bias cx,cy toward (0,0) by averaging two samples.
      this.wave_params.wave_flat_cx = 
          (RandBetween(p.flat_cx_lo, p.flat_cx_hi) + RandBetween(p.flat_cx_lo, p.flat_cx_hi)) * 0.5;
      this.wave_params.wave_flat_cy = 
          (RandBetween(p.flat_cy_lo, p.flat_cy_hi) + RandBetween(p.flat_cy_lo, p.flat_cy_hi)) * 0.5;
      //console.log(`wave_flat_angle lo..hi = ${p.flat_angle_lo} .. ${p.flat_angle_hi}`);
      //console.log(`wave_flat_angle = ${this.wave_params.wave_flat_angle}`);
      //console.log(`wave cx [${p.flat_cx_lo.toFixed(3)} .. ${p.flat_cx_hi.toFixed(3)}] cy [${p.flat_cy_lo.toFixed(3)} .. ${p.flat_cy_hi.toFixed(3)}]`);
    }

    if (this.wave_params.wave_circ_str > 0.0001) {
      this.wave_params.wave_circ_rad = RandBetween(p.circ_rad_lo, p.circ_rad_hi);
      this.wave_params.wave_circ_scale = p.circ_scale;
      this.wave_params.wave_circ_angle = Math.random() * 6.28;		
      // Bias cx,cy toward (0,0) by averaging two samples.
      this.wave_params.wave_circ_cx = 
          (RandBetween(p.circ_cx_lo, p.circ_cx_hi) + RandBetween(p.circ_cx_lo, p.circ_cx_hi)) * 0.5;
      this.wave_params.wave_circ_cy = 
          (RandBetween(p.circ_cy_lo, p.circ_cy_hi) + RandBetween(p.circ_cy_lo, p.circ_cy_hi)) * 0.5;
    }
  }  // end RandomizeWaveParams

  RandomizeDotParams() {
    // Note: Don't use simple assignment; other references to the object will become stale.
    Object.assign(this.dot_params, new DotParams());

    let p = this.GetBlendedPrefs(true);//false);

    // Disable the radial beat dots when in/out motion is very low.
    {
      const lo = kZoomThreshScale * 0.15;
      const hi = kZoomThreshScale * 0.50;
      let t = Math.max(0.0, Math.min(1.0, (this.motion_params.blended_in_or_out_motion - lo) / (hi - lo)));	
      p.radial_beat_dots_prob *= t;
    }

    this.dot_params.radial_beat_dots = (Math.random() < p.radial_beat_dots_prob) ? 1.0 : 0.0;

    this.dot_params.random_beat_dots = (Math.random() < p.random_beat_dots_prob) ? 1.0 : 0.0; 

    this.dot_params.fading_dots = (Math.random() < p.fading_dots_prob) ? 1.0 : 0.0;     

    this.dot_params.grid_dots = (Math.random() < p.grid_dots_prob) ? 1.0 : 0.0;

    const use_chasers = (Math.random() < p.chaser_prob);
    this.dot_params.chaser_1 = use_chasers ? 1.0 : 0.0;
    this.dot_params.chaser_2 = (use_chasers && Math.random() < kTwoChasersProb) ? 1.0 : 0.0;

    this.dot_params.dotty_chaser = (Math.random() < p.dotty_chaser_prob) ? 1.0 : 0.0;

    let use_stars = (Math.random() < p.stars_prob);
    if (use_stars) {
      this.dot_params.stars_opacity = lerp(p.stars_opacity_min, p.stars_opacity_max, Math.random());
    } else {
      this.dot_params.stars_opacity = 0.0;
    }
    this.dot_params.stars_population = log_lerp(p.stars_population_min, p.stars_population_max, Math.random());

    this.UpdateStarsFollowMotion(true);
  }  // end RandomizeDotParams

  RandomizeShaderParams() {
    // Note: Don't use simple assignment; other references to the object will become stale.
    Object.assign(this.shader_params, new ShaderParams());

    const zero_thresh = 0.92;
    const full_thresh = 0.98;

    //let p = this.GetBlendedPrefs(xxx);
    const kRxDiffProb = 0.065;//0.10;//0.15;
    const kFreqInjectorProb = 0.12;//0.15;
    //const kDiffusionColorizationProb = 0.09;  // Note: There are ~4 instances of this, so real instance rate will be 4x higher.
    this.shader_params.rx_diff_grey_scott_str  = (Math.random() >= kRxDiffProb) ? 0.0 : (0.2 + 0.4 * (Math.random() + Math.random() + Math.random()) * 0.3333333);
    this.shader_params.rx_diff_grey_scott_str2 = 0.0;//Math.max(0.0, Math.min(1.0, (Math.random() - zero_thresh) / (full_thresh - zero_thresh)));
    this.shader_params.rx_diff_barkley_str     = 0.0;//Math.random();//xxx Math.max(0.0, Math.min(1.0, (Math.random() - zero_thresh) / (full_thresh - zero_thresh)));
    /*
    this.shader_params.diffusion_colorization_1 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_2 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_3 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_4 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_5 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_6 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_7 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    this.shader_params.diffusion_colorization_8 = 0;//(Math.random() >= kDiffusionColorizationProb) ? 0.0 : Math.random();
    */
    this.shader_params.frequency_injector_str = (Math.random() >= kFreqInjectorProb) ? 0.0 : ((Math.random() + Math.random()) * 0.5);
    this.shader_params.frequency_injector_freq_scale = 0.9 + 0.2 * Math.random();
    this.shader_params.sharpen_str            = 0.0;//Math.random();//xxx Math.max(0.0, Math.min(1.0, (Math.random() - zero_thresh) / (full_thresh - zero_thresh)));
    const kNoisePower = 4.0;//4.5;  
    const kNoiseMix = 0.5;//0.2;
    this.shader_params.noise_str              = 
        // Higher powers bias this toward the low end of the distribution.
        // Going from a 100/0 to a 50/50 mix of two of them biases it toward the mean of the distribution.
        Math.pow(Math.random(), kNoisePower) * kNoiseMix + 
        Math.pow(Math.random(), kNoisePower) * (1.0 - kNoiseMix);



    let kPainterlyDualLayerSpecialComboProb = 0.32;//0.25;//0.3;
    if (Math.random() < kPainterlyDualLayerSpecialComboProb) {
      /*  (X,Y)                        painterly_str * 1000
       600       700       800       900       1000     1100       1200      1300
                                                |
                                                |   7
             5         7         9        7     |              5 (painterly too strong over here)
                                                |
        |---------|---------|---------8---------|---------7---------|---------| 900
                                                |
                                              7 |
             3         7         9              |
                                                |     7
        |---------|---------|---------|--8------|---------|----6----|---------| 800  layer2_motion_scale * 1000
                                                |
                                                |                             5
             ?         4         5             7|         ?
                                                |
        |---------|---------|---------|---------|---------|---------|---------| 700
                                                |
                                                |
                                 1              |
                                                |
        |---------|---------|---------|---------|---------|---------|---------| 600
       600       700       800       900       1000     1100       1200      1300
      */      
      this.shader_params.painterly_str       = 0.5 + 0.5 * Math.pow(Math.random() * 0.5 + Math.random() * 0.5, 0.5); // ideal: 0.855; range: [0.5 ... 1.0]
      this.shader_params.layer2_motion_scale = 0.78 + 0.09 * Math.random() + 0.09 * Math.random();  // ideal: 0.84 - 0.95
      this.shader_params.layer2_brightness   = Math.pow(Math.random(), 0.2);
      this.shader_params.layer1_density = 1.05 + 0.3 * Math.pow(Math.random(), 4.0);
    } else {
      const kPainterlyProb = 0.10;//0.12;//0.23;
      const kDualLayerProb = 0.30;//0.73;
      this.shader_params.painterly_str = (Math.random() >= kPainterlyProb) ? 0.0 : 
          Math.random();//xxx Math.max(0.0, Math.min(1.0, (Math.random() - zero_thresh) / (full_thresh - zero_thresh)));
      this.shader_params.layer2_motion_scale = 
          //0.5 + Math.pow(Math.random(), 0.6) * 0.45;
          (Math.random() < 0.8)
              ? 0.8 + 0.2 * Math.random()
              : 0.5 + 0.5 * Math.random();
      if (Math.random() < kDualLayerProb) {
        // This looks best at 0.75 ... 1.0, but we want it to sometimes be dim/subtle, too.
        this.shader_params.layer2_brightness = Math.pow(Math.random(), 0.4);  
      } else {
        this.shader_params.layer2_brightness = 0.0;
      }

      // layer1_density:
      //    0   -> layer 2 is 100% added all the time (...way too bright)
      //    1   -> layer 2 fades in as layer 1 goes [1.000 .. 0]
      //    1.2 -> MINIMUM you should use -- otherwise crisp waveforms in layer 1 get softened.  Nice balance of layer 1 + 2.
      //    1.5 -> layer 2 fades in as layer 1 goes [0.667 .. 0]
      //    2   -> layer 2 fades in as layer 1 goes [0.500 .. 0]
      //    3   -> layer 2 fades in as layer 1 goes [0.333 .. 0] ... you see very little of layer 2
      this.shader_params.layer1_density = 1.2 + 1.3 * Math.pow(Math.random(), 3.0);     // ~1 or higher; [1.2 .. 2.5] is good range; ~1.5 is ideal.  
    }    

  }  // end RandomizeShaderParams

  UpdateStarsFollowMotion(force_all) {
    for (let i = 0; i < kMaxActiveWarps; i++) {
      if (force_all ||
          this.warps[i].warp_map.warp_prefs.stars_follow_motion === undefined) {
        let power = this.warps[i].warp_map.warp_prefs.stars_follow_motion_pow; 
        let t = Math.random();
        t = Math.max(0.0, Math.min(1.0, t * 1.2 - 0.1));
        t = Math.pow(t, power);
        let follow_motion = 
            lerp(this.warps[i].warp_map.warp_prefs.stars_follow_motion_min,
                 this.warps[i].warp_map.warp_prefs.stars_follow_motion_max,
                 t);
        //console.log(`warp ${i} follow_motion ${follow_motion.toFixed(2)}`); //xxx
        this.warps[i].warp_map.warp_prefs.stars_follow_motion = follow_motion;
      }
    }
  }  // end UpdateStarsFollowMotion

} // end class Preset

export function PickRandomWarpMap(other_modes) {	
	// Adjust weights to reflect other_modes[],
	// and get the weight sum.
	other_modes = other_modes || [];		// Make the param optional.
	let adjusted_weights = new Float32Array(g_warp_maps.length);
	let weight_sum = 0.0;
	for (let i = 0; i < g_warp_maps.length; i++) {
		let weight = g_warp_maps[i].weight;
		for (let j = 0; j < other_modes.length; j++) {
			
      // Use this if you want to completely avoid the same mode being used
      //   in two different warp slots at the same time.
      // The risk is just too high of causing double zoom, double rotation,
      //   etc.
      if (g_warp_maps[i].mode == other_modes[j]) {
				weight *= 0.00001;     // TODO: Experiment with skipping this (allow duplicate modes).
			}

      // Use this if you want to adjust the probability of 
      // the next mode choice by what pairs well/poorly with the
      // already-chosen modes.
      let mode1 = i;
      let mode2 = other_modes[j];
      if (kModePairIsSuperGood[mode1][mode2]) {
        weight *= kSuperGoodModePairWeightScale;
      }
      if (kModePairIsGood[mode1][mode2]) {
        weight *= kGoodModePairWeightScale;
      }
      if (kModePairIsBad[mode1][mode2]) {
        weight *= kBadModePairWeightScale;
      }
      if (kModePairIsForbidden[mode1][mode2]) {
        weight *= kForbiddenModePairWeightScale;
      }
		}
		adjusted_weights[i] = weight;		
		weight_sum += weight;
	}

	let target = Math.random() * weight_sum;
	let sum = 0.0;
	for (let i = 0; i < g_warp_maps.length; i++) {
		sum += adjusted_weights[i];
		if (sum >= target) {
			return g_warp_maps[i].mode;
		}
	}	
	return g_warp_maps[g_warp_maps.length - 1].mode;
}
