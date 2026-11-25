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

import { TransformHelper } from "./coords.js"
import { kWarpMapSize, kDynamicMotionParamCount } from "./const.js"
import { FillWormholeMotionMap } from "./motion_wormhole.js";

// Static warp map: each dest pixel samples 1% closer to center.
// Outputs:
//  base: Uint32Array absolute upper-left source index (sx + sy*W)
//  w00,w10,w01,w11: Uint8Array weights summing to 256

// The size of the random number bank generated each time
// we make a new warp map.  All "randomness" must them come
// from these values (deterministically).  If the user then
// resizes the window, the map can be rebuilt with the same
// "randomness" by just passing in the mode # and the 
// existing random numbers, instead of generating new ones.
const WarpMapRandomNumberCount = 256;  // Must be a power of 2.
const WarpMapRandomNumberMask = WarpMapRandomNumberCount - 1;

var _count = 0;
export class WarpMap {
  constructor(mode = 0, weight = 0.0, str_lo = -1.0, str_hi = 2.0) {
    this.mode = mode | 0;        // force int
    // TODO: Rename to freq
    this.weight = +weight;       // force number
    this.index = _count++;
  }
}

function AdjustCenter(cx, cy, warp_prefs) {
  warp_prefs.flat_cx_lo += cx;
  warp_prefs.flat_cx_hi += cx;
  warp_prefs.flat_cy_lo += cy;
  warp_prefs.flat_cy_hi += cy;
  warp_prefs.circ_cx_lo += cx;
  warp_prefs.circ_cx_hi += cx;
  warp_prefs.circ_cy_lo += cy;
  warp_prefs.circ_cy_hi += cy;

  warp_prefs.motion_cx = cx;
  warp_prefs.motion_cy = cy;  
}

const kSwirlFreq = 1.05;//1.25;//1.5;

// Precompute one of each warp map.
export const g_warp_maps = [
  // freq = weight ~ probability of being picked
  //       mode   freq  str_lo str_hi
  new WarpMap(0,  0.41),	// flat zoom (boring)
  new WarpMap(1,  2.43),	// power zoom
  new WarpMap(2,  0.62),	// ~zooming over terrain

  // Swirlies:
  new WarpMap(3,  1.12 * kSwirlFreq),	// LOW FREQ SINE WAVES
  new WarpMap(4,  0.61 * kSwirlFreq),	// N randomly-placed swirls; can overlap
  new WarpMap(5,  0.42 * kSwirlFreq),	// swirlie grid 2
  new WarpMap(6,  0.43 * kSwirlFreq),	// swirlie grid
  new WarpMap(7,  0.94 * kSwirlFreq),	// N randomly-placed swirls, progressively smaller; can overlap

  // Radial/angular ripples:
  new WarpMap(8,  1.49),	// angular ripples (sonic)
  new WarpMap(9,  1.23),	// radial ripples (pond splash)
  new WarpMap(10, 0.15),	// starfish
  new WarpMap(11, 1.82),	// glorious ripples

  // Eggs & spheres:
  new WarpMap(12, 0.02),	// sphere (was: egg)
  new WarpMap(13, 0.22),	// sphere 1
  new WarpMap(14, 1.79),	// sphere 2 - classic geiss look :)
  new WarpMap(15, 0.06),	// black hole (-)

  new WarpMap(16, 2.33),	// heavy rotate -- center can rotate one way, outer edge the other (and accelerated)
  new WarpMap(17, 3.28),	// 1/Z zoom
  new WarpMap(18, 1.10),	// motion-blur-esque zoom (via noisy 'scale') and rotate 
  new WarpMap(19, 0.29),	// radial zoom w/strong accel
  new WarpMap(20, 0.06),	// ROUNDED SQUARE ROTATION
  new WarpMap(21, 0.49),	// faster zoom, with medium accel as f(rad)
  new WarpMap(22, 0.12),	// FISSURE.  This mode "hogs" the motion when active, so make it less common.   
  new WarpMap(23, 0.17),	// ground plane zoom
  new WarpMap(24, 0.13),	// glitchy: super speedy chunky/glitchy zoom (radial chunks)
  new WarpMap(25, 0.08),	// glitchy: glitchy/chunky ripple zoom
  new WarpMap(26, 0.19),	// feathered inward rotation
  new WarpMap(27, 0.31),	// split zoom (in/out), mild rotation
  new WarpMap(28, 0.17),	// split zoom (in/out), mild rotation, sucked in @ sides
  new WarpMap(29, 0.17),	// flower petals / starfish
  new WarpMap(30, 0.36),	// rotation - increasing at edges  (vortex)
  new WarpMap(31, 0.35),	// horizontal U zoom
  new WarpMap(32, 0.13),	// vertical U zoom
  new WarpMap(33, 0.32),	// split zoom (in/out)
  new WarpMap(34, 0.17),	// wormhole -- keep it rare so it remains novel :)
  new WarpMap(35, 1.50),	// dynamic -- unjulating waves from center
  new WarpMap(36, 0.20),	// 5-15 hard radial bands, each with a random rotation
  new WarpMap(37, 1.33 * kSwirlFreq),	// dynamic -- low freq. 3d noise-driven velocity field
  new WarpMap(38, 0.27 * kSwirlFreq),	// dynamic -- vertical and horizontal animated sine waves, which create vortices
  new WarpMap(39, 0.000001),	// dynamic -- vertical and horizontal animated sine waves, which create vortices
  // *** Note: This list must be tight, i.e. start at 0 and increasing by 1 each time --
  // *** otherwise the good/bad mode pairs arrays below will be sized incorrectly.
  //new WarpMap(40, 0.30),	// ground effect
  //new WarpMap(41, 0.05),	// 
  //new WarpMap(42, 3.30),	// Name of the Wind
  //new WarpMap(43, 0.15),	// super horizontal streaks
  //new WarpMap(44, 0.05),	// y-dependent zoom, plus a rotate
  //new WarpMap(89, 0.05),	// crappy cubismo, half-finished
  //new WarpMap(99, 0.04),	// cubism
];

// Reverse zoom:   12,26,
// No zoom:        3,4,5,6,7,8,9,16,20,30,34
// Low zoom:       11,13,22,27,28,29,31,
// Low to medium:  0,1,2,10,17,19,21,33,
// medium:         18,23,32
// medium to high: 24,25,
// high zoom:      14,15,

const kSuperGoodModePairsList = {
  35: [16,17],    // Cosmic breathing iris
  37: [16],
};

const kGoodModePairsList = {
  3: [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 20, 22, 23, 27, 28, 29, 30, 31],
  4: [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 20, 22, 23, 27, 28, 29, 30, 31],
  5: [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 20, 22, 23, 27, 28, 29, 30, 31],
  6: [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 20, 22, 23, 27, 28, 29, 30, 31],
  7: [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 20, 22, 23, 27, 28, 29, 30, 31],
  8: [16,20,22,27,28,29,31,32],
  11: [8, 12],
  16: [3,4,5,6,7,8],
  23: [3,4,5,6,7,11],
  28: [3,4,5,6,7],
  31: [3,4,5,6,7,10,11,12],
  32: [3,4,5,6,7,10,11,12],
  37: [0,1,2,10,20,31,32,34,35],
  // Add others as needed.
};

const kBadModePairsList = {
  11: [3,4,5,6,7,8,11,18,22,24],    // Avoid swirlies, and pairing with self, and some high zooms.
  12: [3,4,5,6,7],
  15: [25],
  25: [14,17,18,19,24],    // 25 has a fairly fast zoom; avoid combining with other fast zooms.
  28: [11,25],
  35: [3,4,5,6,7],         // Avoid mixing w/swirlies.
  36: [22],
  37: [11],
  38: [36],
  // Add others as needed.
};

// Note: You don't actually need to list each mode here with itself; 
// we disallow have 2 instances of the same mode live at the same time
// (in the base 4 motions for a preset).
const kForbiddenModePairsList = {
  11: [28,38],
  14: [11,14,17,18,19,24],     // 14 has the fastest zoom; avoid combining it with other fast zooms, and esp. itself.
  34: [1,10,11,14,15,17,18,19,21,23,24,25,28,31,32,33,34],  // wormhole doesn't like anything capable of medium OR heavy zoom -- and doesn't like to pair with itself.
  35: [14,15,18,23,24,25,32,35],   // Avoid mixing with medium or high zooms; and don't have 2 active at once.
  36: [0,1,2,10,14,15,17,18,19,21,23,24,25,28,29,31,32,33,34],   // Avoid low/medium-ish and higher zooms.
  37: [37],
  38: [25],
};

// 2.0 -> too many bad combos
// 2.5 -> too boring/safe
export const kSuperGoodModePairWeightScale = 4.0;
export const kGoodModePairWeightScale = 2.15;//2.3;
export const kBadModePairWeightScale = 1.0 / kGoodModePairWeightScale;
export const kForbiddenModePairWeightScale = 0.00001;

function ConvertToFull2DArray(list) {
  // Set up the array; fill with zeroes.
  let ret = [];
  for (let i = 0; i < g_warp_maps.length; i++) {
    let scores = new Int32Array(g_warp_maps.length).fill(0);
    ret.push(scores);
  }

  // Splat each pair into the array.
  for (const [mode1, companions] of Object.entries(list)) {
    const mode = Number(mode1); // Object keys are strings.

    for (const mode2 of companions) {
      ret[mode1][mode2] = 1;
      ret[mode2][mode1] = 1;
    }
  }

  return ret;
}

export const kModePairIsSuperGood = ConvertToFull2DArray(kSuperGoodModePairsList);
export const kModePairIsGood = ConvertToFull2DArray(kGoodModePairsList);
export const kModePairIsBad = ConvertToFull2DArray(kBadModePairsList);
export const kModePairIsForbidden = ConvertToFull2DArray(kForbiddenModePairsList);





function mulberry32(seed) {
  let state = seed >>> 0;

  return function() {
    state = (state + 0x6D2B79F5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function NormalizeQuaternion(quat) {
  const qlen = Math.hypot(quat.x, quat.y, quat.z, quat.w);
  if (qlen === 0) return null; // invalid rotation
  const inv = 1.0 / qlen;
  return { 
  	x: quat.x * inv, 
  	y: quat.y * inv, 
  	z: quat.z * inv, 
  	w: quat.w * inv 
  };
}

function lerp(a, b, t) {
	return a * (1.0 - t) + b * t;
}

function smoothstep(x) {
	return x * x * (3 - 2 * x);
}

function ApplyRot2D(x, y, rot) {
	const cos_rot = Math.cos(rot);
	const sin_rot = Math.sin(rot);
	let x2 = x * cos_rot - y * sin_rot;
	let y2 = x * sin_rot + y * cos_rot;
	return { x: x2, y: y2 };
}

// 't' should be between 0 and 1, but it can go outside.
// 'lo' and 'hi' must both be > 0.
function LogInterp(lo, hi, t) {
	//return Math.exp(Math.ln(lo) + t * (Math.log(hi) - Math.log(lo)));
  return lo * Math.pow(hi / lo, t);
}

function LinearInterp(lo, hi, t) {
	return lo + (hi - lo) * t;
}

export function buildWarpMap(new_mode, seed = undefined) {

  if (seed === undefined) {
    seed = (Math.random() * 4294967296) | 0;
  }

  const random = mulberry32(seed);

  // See also: size forced in gpu_warp.js.
	const W = kWarpMapSize;
	const H = kWarpMapSize;
	
  const kFollowMotionNone = 0.0;
  const kFollowMotionLow = 0.1;
  const kFollowMotionMed = 0.5;
  const kFollowMotionHigh = 1.0;

	// Flat waveform
	//   wave_flat.str					[0 .. 1]
	//   wave_flat.angle				[0 .. 2PI]
	//   wave_flat.scale				~1
	//   wave_flat.is_stereo		[0..1]
	//   wave_flat.stereo_sep   [0..1]
	//   wave_flat.cx           [-1..1]
	//   wave_flat.cy           [-1..1]
	// Circular waveform
	//   wave_circ.str 					[0..1]
	//   wave_circ.rad					~1
	//   wave_circ.scale				~1
	//   wave_circ.angle				[0 .. 2PI]
	//   wave_circ.cx           [-1..1]
	//   wave_circ.cy           [-1..1]
  let warp_prefs = {
		// How much this motion map cares about selecting the wave data.
		// >= 0.  A higher weight will cause it to be more aggressive in
		// the weighted decision (when multiple motion maps are active at 
		// once).
    // This applies to all values in this struct EXCEPT the "stars"
    // values, which have their own 'stars_voting_weight'.
		wave_prefs_weight : 1.0,

		// These are all relative weights.  They don't have to sum to 1.
		// Note that the primary logic for the wave type (flat vs. circ)
		//   is driven by a function of the weighted average motion 
		//   metadata of the 4 active maps.  These 'prob' values basically
		//   serve as gates, to completely shut off certain wave types
		//   for certain modes, if desired -- or to amplify them.  But
    //   most of the time, the motion will drive the proper selection,
    //   and no biases are necesssary.
		//type_flat_prob : 1.0,		// Do not change.
		//type_circ_prob : 1.0,		// Do not change.
		//type_flat_plus_circ : 0.01,
		//type_xy : 0.0,
    use_motion_metadata_to_choose_wave_type : 1.0,
    // As 'use_motion_metadata_to_choose_wave_type' approaches 0,
    // we will instead use these probabilities: 
    wave_flat_prob : 1.0,
    wave_circ_prob : 0.25,
    wave_flat_plus_circ_prob : 0.01,
		
    // Try to keep flat angles between 0 and pi.
		flat_angle_lo :  0.0,//-3.141592,
		flat_angle_hi :  3.141592,
		// Since most people watch in widescreen:
		flat_angle_bias_toward_horizontal_angles : 1.7,   // 0+
		flat_scale : 1.0,
		flat_is_stereo_chance : 0.18,//0.2,//0.18,		// [0..1]
		flat_stereo_sep_lo : 0.43,//0.55,
		flat_stereo_sep_hi : 0.65,//0.85,
		flat_stereo_amplitude_scale : 1.0,
		// see also: use_motion_center_as_wave_center_on_zoomy_combos_prob
		flat_cx_lo : -0.12,
		flat_cx_hi :  0.12,
		flat_cy_lo : -0.12,
		flat_cy_hi :  0.12,

		circ_rad_lo : 0.65,//0.75,// 0.45,
		circ_rad_hi : 1.2,//1.3,//0.75
		circ_scale :  1.0,
		// see also: use_motion_center_as_wave_center_on_zoomy_combos_prob
		circ_cx_lo : -0.05,
		circ_cx_hi :  0.05,
		circ_cy_lo : -0.05,
		circ_cy_hi :  0.05,

    motion_cx : 0.0,
    motion_cy : 0.0,

		// Most of the time, we ignore the (cx, cy) for the wave and
		// just stick it at the center of motion.  You can control
		// how often that happens here.  Or, for certain modes, you
		// can fine-tune it.
		use_motion_center_as_wave_center_prob : 0.93,
		// When we do it, don't always just do it 100%; sometimes
		// do it fractionally, according to:
		//   [random in 0..1] ^ use_motion_center_as_wave_center_power
		use_motion_center_as_wave_center_power : 0.4,
		
		radial_beat_dots_prob : 0.57,//0.6,//0.54, // Note: Actual prob. will seem lower b/c it gets shut off @ low net zooms.
		random_beat_dots_prob : 0.01,		
		fading_dots_prob      : 0.07,
		grid_dots_prob        : 0.005,
		chaser_prob           : 0.37,//0.39,//0.45,		// Note: This is the prob of there being ANY chasers (qty 1 or 2).  Two control how often it is 1 vs 2, adjust kTwoChasersProb.
		dotty_chaser_prob     : 0.05,

		// Metadata computed from the resulting warp map:
		//net_motion : 0.0,
		//net_zoom_motion : 0.0,
		//in_or_out_motion : 0.0,
		//net_clockwise_motion : 0.0,
		//cw_or_ccw_motion : 0.0,
		
		//angular_motion_mag : 0.0,     // ~clockwise rotation
		//radial_motion_mag : 0.0,      // ~zoom out
		//abs_radial_motion_mag : 0.0,  // ~motion in or out
		//abs_angular_motion_mag : 0.0,  // ~rotation in any direction

    // The 'stars' (formerly bokeh particles) advect with *some* of the motion,
    // but never imprint into the warp image; they draw on top of it.
    stars_voting_weight : 1.0,  // anything
    // 1) These are mixed according to 'stars_voting_weight' for the preset,
    //    and then the blended results are used at the preset level.
    stars_prob           : 0.55,  // [0..1]
    stars_population_min : 1.0,  // [0..1]
    stars_population_max : 1.0,  // [0..1]
    stars_opacity_min    : 0.3,  // [0..1]
    stars_opacity_max    : 1.0,  // [0..1]
    stars_follow_motion_min : kFollowMotionMed,   // [0..1]
    stars_follow_motion_max : kFollowMotionHigh,  // [0..1]
    stars_follow_motion_pow : 1.0,    // Interpolation power; <1 biases toward max, >1 biases toward min.
	};

  const n = W * H;

	// Store interlaved: { dx, dy, dx, dy ... }  
  // Register additional GPU-generated modes here as they are added.
  let is_dynamic = false;
  let src_dxy = new Float32Array(n * 2);

	// TODO: Factor out this common code (which is duplicated 
	// in both motion.js and warp.js), and coordinate transform
	// helper functions.
	  
	// Compute the aspect-ratio-independent coordinates
	// of the point on the screen.  The window is fitted
	// maximally to a [-1..1] x [-1..1] square.
	const aspect = W * 1.0 / H;  // 1.6
	
	let x0 = -1.0;
	let y0 = -1.0;
	let x1 = 1.0;
	let y1 = 1.0;
	
	if (aspect > 1.0) {
	  // wide
	  y0 /= aspect;
	  y1 /= aspect;
	} else {
	  // tall
	  x0 *= aspect;
	  x1 *= aspect;
	}

	const inv_W = 1.0 / W;
	const inv_H = 1.0 / H;
	
	const x0x1 = x1 - x0;	//2
	const y0y1 = y1 - y0; //1
	const inv_x0x1 = 1.0 / x0x1;	//0.5
	const inv_y0y1 = 1.0 / y0y1;	//1

	// Generate a bank of random numbers that this warp map
	// can draw from.  Try not to use Math.random() after
	// this point in this function.
	let randoms = new Float32Array(WarpMapRandomNumberCount);
	for (let i = 0; i < WarpMapRandomNumberCount; i++) {
		randoms[i] = random();//Math.random();
	}
	let rand_idx = 0;

  let write_offset = 0;

	// TODO: Optimize (simplify) the math for computing fdy, fdx.

	if (new_mode < 0) {
		// Generate a quick dummy map with no motion.
		// (...nothing to do; default for a new array is to fill with zeroes)
	}
	else if (new_mode == 4) {
		// N randomly-placed swirls; can overlap
		//warp_prefs.type_circ_prob *= 0.1;
    warp_prefs.stars_prob *= 0.5;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;

		// Clear the motion map to zero everywhere.
		for (let dy = 0; dy < H; dy++) {
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
		    src_dxy[write_offset + 0] = 0.0;
		    src_dxy[write_offset + 1] = 0.0;	    
      }
    }			  		

		// Add a few big swirles, that can overlap.
		const N = (2 + 34 * randoms[rand_idx++]) | 0;
		for (let n = 0; n < N; n++) {
			const cx = (randoms[rand_idx++] - 0.5) * x0x1;
			const cy = (randoms[rand_idx++] - 0.5) * y0y1;
      
			const rad = 0.3 + 0.5 * randoms[rand_idx++] * Math.min(1.0, 10.0 / N);
			const str = (0.2 + 0.5 * randoms[rand_idx++]) * 
					((randoms[rand_idx++] > 0.5) ? 1 : -1) *
					0.1 * Math.pow(Math.min(1.0, (3.0 / N)), 0.7); //0.7; 
			
		  const dx0 = Math.max(0, Math.min(W, ((cx - rad) * inv_x0x1 + 0.5) * W)) | 0;
		  const dy0 = Math.max(0, Math.min(H, ((cy - rad) * inv_y0y1 + 0.5) * H)) | 0;
		  const dx1 = Math.max(0, Math.min(W, ((cx + rad) * inv_x0x1 + 0.5) * W)) | 0;
		  const dy1 = Math.max(0, Math.min(H, ((cy + rad) * inv_y0y1 + 0.5) * H)) | 0;

			for (let dy = dy0; dy < dy1; dy++) {
			  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
			  write_offset = (dy * W + dx0) * 2;
		    for (let dx = dx0; dx < dx1; dx++, write_offset += 2) {
				  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);		// -1 .. 1

					let rad_sq = ((fdx - cx) * (fdx - cx) + (fdy - cy) * (fdy - cy)) * (1.0 / (rad * rad));
					if (rad_sq < 1) {
						let r = Math.pow(rad_sq, 0.333);
						
						let sdx = fdx;
						let sdy = fdy;
						
						r = smoothstep(r);
						let zx = fdx - cx;
						let zy = fdy - cy;
						let rot = (1.0 - r) * str;//rad_sq * 0.15;
						let zx2 = (zx * Math.cos(rot) - zy * Math.sin(rot));
						let zy2 = (zx * Math.sin(rot) + zy * Math.cos(rot));
						sdx = cx + zx2;
						sdy = cy + zy2;

				    src_dxy[write_offset + 0] += (sdx - fdx);
				    src_dxy[write_offset + 1] += (sdy - fdy);
					}
				}   
      }
    }			  		
	} else if (new_mode == 5) {
		// SWIRLIE GRID 2
		//warp_prefs.type_circ_prob *= 0.1;
    warp_prefs.stars_prob *= 0.5;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;
		const N = 0.5 + randoms[rand_idx++] * 3.5;
		const offset_x = randoms[rand_idx++] * (1.0 / N);
		const offset_y = randoms[rand_idx++] * (1.0 / N);
		const str = (0.1 + 0.3 * randoms[rand_idx++] * randoms[rand_idx++]) * 
				((randoms[rand_idx++] > 0.5) ? 1 : -1) *
				0.25; //0.7; 
		const rot0 = randoms[rand_idx++] * 6.28;
		const variety = (randoms[rand_idx++] > 0.5);

		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
		  fdy += offset_y;
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);		// -1 .. 1
			  fdx += offset_x;

				// Note: If you rotate at the start like this,
				// you also have to counter-rotate the final displacement.
				let p = ApplyRot2D(fdx, fdy, rot0);
				let fx = p.x;
				let fy = p.y;

				let nx = Math.floor(fx * N);  // -N .. N
				let ny = Math.floor(fy * N);

				let k = (nx * 3 + ny * 7) | 0;
				let att = variety ? (randoms[k & WarpMapRandomNumberMask] * 2 - 1) : 1.0;

				let cx = (nx + 0.5) * (1.0 / N);		// -1 .. 1
				let cy = (ny + 0.5) * (1.0 / N);

				let rad_sq = ((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy)) * (N * N * 4);
				let r = Math.pow(rad_sq, 0.333);
				let sdx = fx;
				let sdy = fy;
				if (r < 1) {
					r = smoothstep(r);
					let zx = fx - cx;
					let zy = fy - cy;
					let rot = (1.0 - r) * str;//rad_sq * 0.15;
					let zx2 = (zx * Math.cos(rot * att) - zy * Math.sin(rot * att));
					let zy2 = (zx * Math.sin(rot * att) + zy * Math.cos(rot * att));
					sdx = cx + zx2;
					sdy = cy + zy2;
				}

				sdx -= fx;
				sdy -= fy;
				
				// Rotate back at the end -- so that the displacement
				// is in the original coordinate space.
				let p2 = ApplyRot2D(sdx, sdy, -rot0);   // back to original basis
				sdx = p2.x;
				sdy = p2.y;
				      	
		    src_dxy[write_offset + 0] = sdx;
		    src_dxy[write_offset + 1] = sdy;
      }
    }			  		
	} else if (new_mode == 6) {
		// GRID OF SWIRLIES
		//warp_prefs.type_circ_prob *= 0.1;
    warp_prefs.stars_prob *= 0.5;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;
		const N = (2 + randoms[rand_idx++] * 8);		//VAR
		const offset_x = randoms[rand_idx++] * (1.0 / N);
		const offset_y = randoms[rand_idx++] * (1.0 / N);
		const scale = 1.0;//0.99 + 0.02 * randoms[rand_idx++];	//VAR
		const rot = (0.002 + 0.028 * randoms[rand_idx++]) * ((randoms[rand_idx++] < 0.5) ? 1 : -1) * Math.min(1.0, 4.0 / N);		//VAR
		//const power = 0.5 + 2.5 * randoms[rand_idx++];
		const power = 0.25 + 0.25 * randoms[rand_idx++];
		const variety = Math.max(0.0, Math.min(1.0, randoms[rand_idx++] * 3 - 1));  // [0..1]
		
		const rot0 = randoms[rand_idx++] * 6.28;
		for (let dy = 0; dy < H; dy++) {
		  const orig_fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);

	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  const orig_fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				// Note: If you rotate at the start like this,
				// you also have to counter-rotate the final displacement.
				let p = ApplyRot2D(orig_fdx, orig_fdy, rot0);
				let fdx = p.x;
				let fdy = p.y;

				fdx += offset_x;
				fdx = (fdx * N * 0.5);
				const nx = Math.floor(fdx);
				fdx -= nx;	 // [0..1]
				fdx = fdx * 2 - 1;			 // [-1..1]

				fdy += offset_y;
				fdy = (fdy * N * 0.5);
				const ny = Math.floor(fdy);
				fdy -= ny;			// [0..1]
				fdy = fdy * 2 - 1;					// [-1..1]

				// GRADED VARIETY:
				const k = (nx * 3 + ny * 7) | 0;
	
				const tx = fdx;// - cx;
				const ty = fdy;// + cy;
				var rad = Math.sqrt(tx*tx + ty*ty);
				rad = Math.pow(rad, power);
				const rot2 = rot * Math.max(0.0, 1 - rad);
				// graded variety:
				const rot3 = rot2 * lerp(1.0, randoms[k & WarpMapRandomNumberMask] * 2 - 1, variety);
				let sdx = (fdx * Math.cos(rot3) - fdy * Math.sin(rot3)) * scale;
				let sdy = (fdx * Math.sin(rot3) + fdy * Math.cos(rot3)) * scale;
      	
				sdx -= fdx;
				sdy -= fdy;
				
				// Rotate back at the end -- so that the displacement
				// is in the original coordinate space.
				let p2 = ApplyRot2D(sdx, sdy, -rot0);   // back to original basis
				sdx = p2.x;
				sdy = p2.y;
				      	
		    src_dxy[write_offset + 0] = sdx;
		    src_dxy[write_offset + 1] = sdy;
      }
    }			  	
    			  		
	} else if (new_mode == 10) {
		// STARFISH
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
		
		let str = 0.4 + 1.4 * randoms[rand_idx++];
		if (randoms[rand_idx++] < 0.3) {
			str = str * -0.25;
		}
		
		const fins = 3 + Math.floor(randoms[rand_idx++] * 6);

    const cx = (randoms[rand_idx++] * 2 - 1) * 0.12;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.12;
    AdjustCenter(cx, cy, warp_prefs);

    for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				const tx = fdx - cx;
				const ty = fdy - cy;
				//const scale = 1.0 / Math.sqrt(tx * tx + ty * ty + 0.0000001);
				const ang = Math.atan2(tx, ty);	
				const scale = 0.987 + 0.010 * Math.cos(ang * fins);
				let sdx = fdx * scale;
				let sdy = fdy * scale;

				// Attenuate motion near the center, to avoid discontinuities.
				const rad = Math.sqrt(tx * tx + ty * ty);
				const att = Math.max(0, Math.min(1.0, (rad - 0.05) * 4));
				sdx = fdx + (sdx - fdx) * att * str;
				sdy = fdy + (sdy - fdy) * att * str;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
	/*} else if (new_mode == 9) {
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				// NOISY BLUR
				let sdx = fdx + (randoms[rand_idx++] * 2 - 1) * 0.02;
				rand_idx &= WarpMapRandomNumberMask;
				let sdy = fdy + (randoms[rand_idx++] * 2 - 1) * 0.02;
				rand_idx &= WarpMapRandomNumberMask;
		
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }*/			  
	} else if (new_mode == 9) {
		warp_prefs.circ_scale = 4.0;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;

		const cx = (randoms[rand_idx++] * 2 - 1) * 0.25;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.25;
    AdjustCenter(cx, cy, warp_prefs);

    const freq = 6 + 18 * randoms[rand_idx++];
		const phase = randoms[rand_idx++] * 6.28;
		const mag = (0.00006 + 0.00011 * randoms[rand_idx++]) / freq * 100.0 *
				((randoms[rand_idx++] < 0.5) ? 1.0 : -1.0) * 2;
		const bias = randoms[rand_idx++] * 2 - 1;
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				// RADIAL RIPPLES
				const tx = fdx - cx;
				const ty = fdy - cy;
				const rad = Math.sqrt(tx*tx + ty*ty);
				const scale = 1.0 + mag * (Math.cos(rad * freq + phase) + bias);
				let sdx = fdx * scale;
				let sdy = fdy * scale;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
	} else if (new_mode == 8) {
		warp_prefs.circ_scale = 4.0;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;

		const cx = (randoms[rand_idx++] * 2 - 1) * 0.45;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.45;
    // Don't adjust the wave centers for this one; the
    // center of the ripples can be quite far from the center,
    // and since this one doesn't zoom, the waves don't need to
    // be centered on it.
    //AdjustCenter(cx, cy, warp_prefs);
    // But DO update these two, so that radial beat dots show up centered:
    warp_prefs.motion_cx = cx;
    warp_prefs.motion_cy = cy;  
  
    const rad_freq = 5 + 19 * randoms[rand_idx++];
		const rot_str = (0.001 + 0.004 * randoms[rand_idx++] * randoms[rand_idx++]) * 
										((randoms[rand_idx++] < 0.5) ? -1.0 : 1.0) * 0.21 * 
										Math.pow(13 / rad_freq, 0.6) * 4;		// partially compensate for freq
		const rot_bias = randoms[rand_idx] * 2 - 1;
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				// SWIRL
				// TODO: If we oversize the warp maps a little bit,
				// maybe we could shift them individually??
				const tx = fdx - cx;
				const ty = fdy - cy;
				const rad = Math.sqrt(tx*tx + ty*ty);
				const rot = (Math.cos(rad * rad_freq) + rot_bias) * rot_str;
				let sdx = tx * Math.cos(rot) - ty * Math.sin(rot) + cx;
				let sdy = tx * Math.sin(rot) + ty * Math.cos(rot) + cy;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
	} else if (new_mode == 2) {
		// LANDING
  	warp_prefs.wave_prefs_weight *= 5;
  	
  	let str = 0.027 * (0.1 + 0.9 * randoms[rand_idx++]);
		let angle = randoms[rand_idx++] * 6.28;
		const cos_angle = Math.cos(angle);
		const sin_angle = Math.sin(angle);

		// Add in a very mild rotation.
		const rot = (randoms[rand_idx++] * 2 - 1) * 0.0015;
		const cos_rot = Math.cos(rot);
		const sin_rot = Math.sin(rot);
  	
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				let rx = fdx * cos_angle - fdy * sin_angle;
				let ry = fdx * sin_angle + fdy * cos_angle;
					
				let sdx = fdx;
      	let sdy = fdy;
      	//let zoom = 1.0 - (1.0 / (-fdy + 1.01)) * 0.015;	-> led to weird behavior near fdy==1
      	let zoom = 1.0 - (1.0 / (-ry + 1.4)) * str;
      	sdx *= zoom;
      	sdy *= zoom;

				rx = sdx * cos_rot - sdy * sin_rot;
				ry = sdx * sin_rot + sdy * cos_rot;
				sdx = rx;
				sdy = ry;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
  } 
  /*else if (new_mode == 7) {
		// EGG - removed because it's a subset of 13 (sphere)
		motion_mag_scale_lo = 0.5;
		motion_mag_scale_hi = 1.5;
  	warp_prefs.wave_prefs_weight *= 10;  // Very dominant effect.
  	//warp_prefs.type_circ_prob = warp_prefs.type_flat_prob;
		warp_prefs.circ_rad_lo = 0.5;
		warp_prefs.circ_rad_hi = 1.25;

		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				let sdx = fdx;
      	let sdy = fdy;
      	let rad = Math.sqrt(fdx*fdx + fdy*fdy);
      	let scale = 0.97 + 0.1*rad;
      	scale = Math.pow(scale, 4);
      	sdx *= scale;
      	sdy *= scale;
      	
      	sdx = fdx + (sdx - fdx) * 0.5;
      	sdy = fdy + (sdy - fdy) * 0.5;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
  }*/ 
  else if (new_mode == 15) {
		// BLACK HOLE
  	warp_prefs.wave_prefs_weight *= 10;  // Very dominant effect.
  	//warp_prefs.type_circ_prob = warp_prefs.type_flat_prob;
		warp_prefs.circ_rad_lo = 0.9;
		warp_prefs.circ_rad_hi = 1.5;

		// 0.5 -> just a strong perspective zoom
		// 1.0 -> black hole
		// 1.5 -> black hole, but showing hints of sphere
		// 2.0 -> 
		let str = (0.75 + 1.5 * randoms[rand_idx++]);

		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				let sdx = fdx;
      	let sdy = fdy;
      	let rad = Math.sqrt(fdx*fdx + fdy*fdy);
      	let scale = 0.97 + 0.1*rad;
      	scale = Math.pow(scale, 4);
      	sdx *= scale;
      	sdy *= scale;
      	
      	sdx = fdx + (sdx - fdx) * -str;
      	sdy = fdy + (sdy - fdy) * -str;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
  } 
  /*else if (new_mode == 99) {
		motion_mag_scale_lo = 0.0;
		motion_mag_scale_hi = 1.0;
		const count = ((8 + 8 * randoms[rand_idx++]) | 0);		
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				// CUBISM 1
				const str = 0.05;
				let tx = (((fdx + 1) * (count * 0.5) + 0.5) | 0) * (1.0 / (count * 0.5)) - 1;
      	let ty = (((fdy + 1) * (count * 0.5) + 0.5) | 0) * (1.0 / (count * 0.5)) - 1;
      	tx = tx * str + fdx * (1.0 - str);
      	ty = ty * str + fdy * (1.0 - str);      	
      	let sdx = tx;
      	let sdy = ty;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  
  }*/
  else if (new_mode == 0) {
		// SIMPLE ZOOM
  	warp_prefs.wave_prefs_weight *= 10;
		//warp_prefs.type_circ_prob *= 10;
		//warp_prefs.type_flat_plus_circ *= 1.4;

		let zoom = 0.004 + 0.016 * randoms[rand_idx++];
		if (randoms[rand_idx++] < 0.1) {
			zoom *= -0.25;
		}

		// Add in a very mild rotation.
		const rot = (randoms[rand_idx++] * 2 - 1) * 0.0015;
		const cos_rot = Math.cos(rot);
		const sin_rot = Math.sin(rot);

	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
      	let sdx = fdx + (0.0 - fdx) * zoom;
      	let sdy = fdy + (0.0 - fdy) * zoom;

				let rx = sdx * cos_rot - sdy * sin_rot;
				let ry = sdx * sin_rot + sdy * cos_rot;
				sdx = rx;
				sdy = ry;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
  } else if (new_mode == 1) {
		
		// POWER ZOOM
  	warp_prefs.wave_prefs_weight *= 10;
		//warp_prefs.type_circ_prob *= 10;
		//warp_prefs.type_flat_plus_circ *= 1.4;

		let zoom = (0.4 + 2.7 * randoms[rand_idx++]) * 0.03 * 1.8;
		if (randoms[rand_idx++] < 0.01) {
			zoom *= -0.4;
		}

		// Add in a very mild rotation.
		const rot = (randoms[rand_idx++] * 2 - 1) * 0.0015;
		const cos_rot = Math.cos(rot);
		const sin_rot = Math.sin(rot);

	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
  	
				let r = (fdx*fdx + fdy*fdy);
      	let sdx = fdx + (0.0 - fdx) * r * zoom;
      	let sdy = fdy + (0.0 - fdy) * r * zoom;				

				let rx = sdx * cos_rot - sdy * sin_rot;
				let ry = sdx * sin_rot + sdy * cos_rot;
				sdx = rx;
				sdy = ry;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
		

		/*
		// LOG ZOOM
  	warp_prefs.wave_prefs_weight *= 10;
		//warp_prefs.type_circ_prob *= 10;
		//warp_prefs.type_flat_plus_circ *= 1.4;

		// Note: using zoom>1 here is cheating.
		let zoom = 0.01 + 1.98 * randoms[rand_idx++] * randoms[rand_idx++] * randoms[rand_idx++];

		// Add in a very mild rotation.
		const rot = (randoms[rand_idx++] * 2 - 1) * 0.0015;
		const cos_rot = Math.cos(rot);
		const sin_rot = Math.sin(rot);

	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
  	
				let r = Math.sqrt(fdx*fdx + fdy*fdy) * 0.5;  // [0..1]
				// Use a log curve (of the radius) to determine the look-at radius; this ensures
				// the curve can never ramp up too quickly, leading to inversions (which look like a sphere).
				let r2 = Math.log(r + 1);
				var scale = (r2 + 0.001) / (r + 0.001);
				scale = scale * zoom + (1 - zoom) * 1.0;
      	let sdx = fdx * scale;
      	let sdy = fdy * scale;

				let rx = sdx * cos_rot - sdy * sin_rot;
				let ry = sdx * sin_rot + sdy * cos_rot;
				sdx = rx;
				sdy = ry;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
		*/
  } else if (new_mode == 16) {
		// ROTATE
  	warp_prefs.wave_prefs_weight *= 0.01;
		//warp_prefs.type_circ_prob *= 0.1;
    warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
  	
    // rotation speed at center:
		var rot1 = 0.002 + 0.008 * randoms[rand_idx++];		
		if (randoms[rand_idx++] < 0.5) rot1 *= -1;

    // rotation speed at edge:
		//var rot2 = rot1;	 	
		//if (randoms[rand_idx++] < 0.8) {
		//	rot2 = rot1 * randoms[rand_idx++] * 2;
		//}
		var rot2 = (0.002 + 0.008 * randoms[rand_idx++]) * 2;		
		if (randoms[rand_idx++] < 0.5) {
      rot2 *= -1;
    }
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
  	
				const rad = Math.sqrt(fdx * fdx + fdy * fdy);
				const rot = rot1 + (rot2 - rot1) * rad;
				let sdx = fdx * Math.cos(rot) - fdy * Math.sin(rot);
				let sdy = fdx * Math.sin(rot) + fdy * Math.cos(rot);

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);
      }
    }
  } else if (new_mode == 17) {
		// 1/Z ZOOM
  	warp_prefs.wave_prefs_weight *= 10;

		const speed_min = 0.015;
		const speed_max = 0.16;
		const speed_pow = 2.0;  // >1 = bias toward speed_min; [0..1] = bias toward speed_max.
		const t = Math.pow(randoms[rand_idx++], 3.0);
		//const speed = LogInterp(speed_min, speed_max, randoms[rand_idx++]);
		const speed = LinearInterp(speed_min, speed_max, Math.pow(randoms[rand_idx++], speed_pow));

		// Add in a very mild rotation.
		const rot = (randoms[rand_idx++] * 2 - 1) * 0.0015;
		const cos_rot = Math.cos(rot);
		const sin_rot = Math.sin(rot);

	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				// RAD = 1/Z  ->  Z = 1 / RAD  	
				let orig_rad = Math.sqrt(fdx*fdx + fdy*fdy + 0.00001);
  			let inv_rad = 1.0 / orig_rad;
  			let nx = fdx * inv_rad; 
  			let ny = fdy * inv_rad;  			
  			
    		let dist = inv_rad;
    		dist += speed;
    		let rad = 1.0 / dist;
    		let sdx = fdx + nx * (rad - orig_rad);
    		let sdy = fdy + ny * (rad - orig_rad);

				let rx = sdx * cos_rot - sdy * sin_rot;
				let ry = sdx * sin_rot + sdy * cos_rot;
				sdx = rx;
				sdy = ry;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
  } else if (new_mode == 12) {
		// SPHERE
  	warp_prefs.wave_prefs_weight *= 10;
    warp_prefs.stars_follow_motion_min = kFollowMotionLow;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
  	
  	const power = LinearInterp(1.03, 1.10, randoms[rand_idx++]);
  	// <1 == ~crop of a supersized sphere
  	//  1 == sphere fitted to screen
  	// >1 == ~egg
  	const t = (randoms[rand_idx++] + randoms[rand_idx++]) * 0.5;
  	const scale = LogInterp(1.5, 4.0, t);
  	const inv_scale = 1.0 / scale;
  	const str = 0.1 + 0.4 * randoms[rand_idx++];

	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				let orig_rad = Math.sqrt(fdx*fdx + fdy*fdy + 0.00001);
  			let rad = Math.pow(orig_rad * scale, power) * inv_scale;
  			let inv_rad = 1.0 / orig_rad;
  			let nx = fdx * inv_rad; 
  			let ny = fdy * inv_rad;  			
  			
    		let sdx = fdx + nx * (rad - orig_rad) * str;
    		let sdy = fdy + ny * (rad - orig_rad) * str;


	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
  } else if (new_mode == 20) {
		// ROUNDED SQUARE ROTATION
  	warp_prefs.wave_prefs_weight *= 10;
    warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
  	
	  const speed = LinearInterp(0.003, 0.01, randoms[rand_idx++]) *
	                ((randoms[rand_idx++] > 0.5) ? 1.0 : -1.0);
  	
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				const rad = Math.sqrt(fdx * fdx + fdy * fdy);

				let nx = fdy;
				let ny = -fdx;
				let nx_sign = (nx < 0) ? -1.0 : 1.0;
				let ny_sign = (ny < 0) ? -1.0 : 1.0;
				nx *= nx * nx_sign;
				ny *= ny * ny_sign;
				const norm_scale = 1.0 / Math.sqrt(nx * nx + ny * ny + 0.00001);
				nx *= norm_scale;
				ny *= norm_scale;
				
    		let sdx = fdx + nx * speed * rad;
    		let sdy = fdy + ny * speed * rad;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
	} else if (new_mode == 30) {
		// VORTEX
		const cx = (randoms[rand_idx++] * 2 - 1) * 0.08;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.08;
    AdjustCenter(cx, cy, warp_prefs);

    warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;

    const str = LinearInterp(0.003, 0.010, randoms[rand_idx++]) * 
							  ((randoms[rand_idx++] > 0.5) ? 1 : -1);
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				const tx = fdx - cx;
				const ty = fdy - cy;
								
				const rad = Math.sqrt(tx * tx + ty * ty);
				const rot = rad * str;

				const rotated = ApplyRot2D(tx, ty, rot);
				
				let sdx = cx + rotated.x;
				let sdy = cy + rotated.y;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  		
	} else if (new_mode == 34) {
		// Wormhole
    warp_prefs.motion_cx = 0;
    warp_prefs.motion_cy = 0;
    warp_prefs.fading_dots_prob *= 1.8;
    //warp_prefs.type_circ_prob *= 0.01;
  	warp_prefs.wave_prefs_weight *= 10.0;

    let r1 = 0.2 + 0.4 * randoms[rand_idx++]; // Radius of the vertical tube
    let r2 = 0.25 + 0.45 * randoms[rand_idx++]; // Rounding radius of the quarter-circle fillet
    let rot = (0.3 + 0.7 * Math.pow(randoms[rand_idx++], 0.5)) * 
              ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    let inward_speed = 0.03 + randoms[rand_idx++] * 0.3; //0.08;
    let fall_accel = 0.05 + 0.42 * randoms[rand_idx++]; //0.35;
    let elev_degrees = 10 + 20 * randoms[rand_idx++] + 20 * randoms[rand_idx++]; // 90 = from above, 0 = side-on

    /*
    console.log(`-----------------`);
    console.log(`r1 ${r1.toFixed(3)}`);
    console.log(`r2 ${r2.toFixed(3)}`);
    console.log(`rot ${rot.toFixed(3)}`);
    console.log(`inward_speed ${inward_speed.toFixed(3)}`);
    console.log(`fall_accel ${fall_accel.toFixed(3)}`);
    console.log(`elev_degrees ${elev_degrees.toFixed(1)}`);
    */
    const stats = FillWormholeMotionMap(src_dxy, W, H, {
      r1: r1,
      r2: r2,
      rotation: rot,          // radians/s at the rim; negative reverses swirl
      inward_speed: inward_speed,     // surface units/s at the rim
      fall_acceleration: fall_accel,
      elevation_degrees: elev_degrees,
    });
	} else if (new_mode == 35) {
    is_dynamic = true;   // For the motion for dynamic maps, see gpu_dynamic_warp.js.

    warp_prefs.fading_dots_prob = 0.8;

  	warp_prefs.wave_prefs_weight = 10;

    // This will activate the 3 probabilities below.
    warp_prefs.use_motion_metadata_to_choose_wave_type = 0.0;

		warp_prefs.wave_flat_prob = 0.0;
		warp_prefs.wave_circ_prob = 1.0;
    warp_prefs.wave_flat_plus_circ_prob = 0.0;
	} else if (new_mode == 36) {
    const str = 0.2 + 0.8 * randoms[rand_idx++];
    const rad_scale = 4.0 + randoms[rand_idx++] * 10.0;

		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				const rad = Math.sqrt(fdx * fdx + fdy * fdy);
        const band = Math.floor(rad * rad_scale) | 0;
				const rot = (randoms[16 + band] * 2 - 1) * 0.02 * str;

				const rotated = ApplyRot2D(fdx, fdy, rot);
				
				let sdx = rotated.x;
				let sdy = rotated.y;
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  		
	} else if (new_mode == 37) {
    is_dynamic = true;   // For the motion for dynamic maps, see gpu_dynamic_warp.js.
    warp_prefs.stars_follow_motion_min = kFollowMotionLow;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;
	} else if (new_mode == 38) {
    is_dynamic = true;   // For the motion for dynamic maps, see gpu_dynamic_warp.js.
	} else if (new_mode == 39) {
    is_dynamic = true;   // For the motion for dynamic maps, see gpu_dynamic_warp.js.
  }
	/*} else if (new_mode == 15) {
		// radial & angular swirls, vel normalized, with center attenuation to avoid discontinuity
		const cx = (randoms[rand_idx++] * 2 - 1) * 0.08;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.08;
    AdjustCenter(cx, cy, warp_prefs);
		
    const fins = (3 + randoms[rand_idx++] * 11) | 0;
		const ripples = randoms[rand_idx++] * 45 + 15;
		
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				const tx = fdx - cx;
				const ty = fdy - cy;
				
				//const zx = 0.01 * Math.cos(tx * 17 - ty * 22 + 1);
				//const zy = 0.01 * Math.cos(tx * 13 + ty * 7  + 2);
				
				const rad = Math.sqrt(tx * tx + ty * ty + 0.00001);
				const ang = Math.atan2(ty, tx);	
				
				const att = Math.max(0, Math.min(1.0, (rad - 0.05) * 4));
				
				const rad2 = rad + Math.cos(ang * fins) * 0.005;
				const rot = 0.003 * (Math.cos(rad * ripples) + 1);
				
				const rotated = ApplyRot2D(tx, ty, rot);
				
				const scale = rad2 / rad;
				let sdx = rotated.x * scale;
				let sdy = rotated.y * scale;

				//sdx = LinearInterp(fdx - cx, sdx, att);
				//sdy = LinearInterp(fdy - cy, sdy, att);

      	sdx += cx;
      	sdy += cy;
      	
      	sdx -= fdx;
      	sdy -= fdy;
      	
      	const t = 1.0 / Math.sqrt(sdx * sdx + sdy * sdy + 0.00001);
      	sdx *= att * t * 0.002;
      	sdy *= att * t * 0.002;

      	sdx += fdx;
      	sdy += fdy;
      	
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
	}
	else if (new_mode == 15) {
		// LF WAVES
		const rot1 = randoms[rand_idx++];
		const rot2 = randoms[rand_idx++];
		const rot3 = randoms[rand_idx++];
		const rot4 = randoms[rand_idx++];
		
		for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				let x = fdx;
				let y = fdy;

				x += Math.cos(fdx *  0.17 + fdy *  0.29 + 3) * 0.002;
				y += Math.cos(fdx * -0.08 + fdy *  0.17 + 2) * 0.002;
				x += Math.cos(fdx * -0.05 + fdy *  0.29 + 4) * 0.002;
				y += Math.cos(fdx *  0.21 + fdy * -0.12 + 1) * 0.002;

				let sdx = x;
				let sdy = y;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  		
  }*/
  else if (new_mode == 22) {
		// FISSURE - aka TWO-PLANE ZOOM
  	warp_prefs.wave_prefs_weight *= 10;
    warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;

		const theta = randoms[rand_idx++] * 3.14;//6.28;
	  const cos_theta = Math.cos(theta);
	  const sin_theta = Math.sin(theta);
	  const cos_minus_theta = Math.cos(-theta);
	  const sin_minus_theta = Math.sin(-theta);
	  const dz = (0.015 + 0.015 * randoms[rand_idx++]) * 1.0;
	  const plane_y = 1.01 + 0.5 * randoms[rand_idx++];
	  
	  // (cx, cy) serves as the center for both the fissure
	  //   and the waveform.
	  // Only vary the center position perpendicular
	  //   to the fissure direction; this will keep the wave
	  //   more centered.
	  const wave_offset = (randoms[rand_idx++] * 2 - 1) * 0.6;
	  const cx = wave_offset * Math.cos(-theta + Math.PI / 2);
	  const cy = wave_offset * Math.sin(-theta + Math.PI / 2);

	  //AdjustCenter(cx, cy, warp_prefs);
    // Do this manually - most of the time:
  	if (randoms[rand_idx++] < 0.99) {
			warp_prefs.flat_angle_lo = -theta + (3.1415927 * 2); 
			warp_prefs.flat_angle_hi = -theta + (3.1415927 * 2);
			warp_prefs.flat_cx_lo = cx;
			warp_prefs.flat_cx_hi = cx;
			warp_prefs.flat_cy_lo = cy;
			warp_prefs.flat_cy_hi = cy;
			warp_prefs.flat_angle_bias_toward_horizontal_angles = 0.0;
			warp_prefs.flat_stereo_sep_lo *= 0.5;//0.3;
			warp_prefs.flat_stereo_sep_hi *= 0.7;//0.75;
			warp_prefs.flat_stereo_amplitude_scale *= 0.6;
			warp_prefs.circ_cx_lo = cx;
			warp_prefs.circ_cx_hi = cx;
			warp_prefs.circ_cy_lo = cy;
			warp_prefs.circ_cy_hi = cy;
		}
    // But DO update these two, so that radial beat dots show up centered:
    warp_prefs.motion_cx = cx;
    warp_prefs.motion_cy = cy;
    
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
			
				// Rotate around the Z axis.
				let rrx = fdx - cx;
				let rry = fdy - cy;
				let rx = rrx * cos_theta - rry * sin_theta;
				let ry = rrx * sin_theta + rry * cos_theta;
				
				// Camera at ws (cx,cy,cz) = (0,0,0)
				// Virtual screen plane spanning ws ([-1..1], [-1..1], 1)
				// Infinite XZ planes at Y = +/- plane_y
			  
			  // Compute the WS view ray direction and normalize it.
			  let sx = rx;
			  let sy = ry;
			  let sz = 1.0;
			  let vx = sx;
			  let vy = sy;
			  let vz = sz;
			  let v_norm_scale = 1.0 / Math.sqrt(vx * vx + vy * vy + vz * vz);
			  vx *= v_norm_scale;
			  vy *= v_norm_scale;
			  vz *= v_norm_scale;
			  
			  // Compute WS intersection of the ray with y == plane_y
			  // cy + vy * t = plane_y
			  // cy + vy * t = plane_y
			  const which_plane_y = plane_y * ((ry < 0) ? -1 : 1);
			  let t = which_plane_y / vy;
			  
			  // Compute the WS point of intersection.
			  let ix = vx * t;
			  let iy = vy * t;
			  let iz = vz * t;

				// Move the WS point of intersection.
				iz += dz;				

				// Compute the new ray direction.
			  vx = ix;
			  vy = iy;
			  vz = iz;
			  v_norm_scale = 1.0 / Math.sqrt(vx * vx + vy * vy + vz * vz);
			  vx *= v_norm_scale;
			  vy *= v_norm_scale;
			  vz *= v_norm_scale;
				
			  
			  // Re-project that WS point back onto the WS screen plane.
			  // cz + vz * t = 1
			  t = 1 / vz;
			  
			  // Compute the final WS point of intersection.
			  let fx = vx * t;
			  let fy = vy * t;

				// Reverse-rotate the point back, so the displacement makes sense.			  
				let sdx = fx * cos_minus_theta - fy * sin_minus_theta;
				let sdy = fx * sin_minus_theta + fy * cos_minus_theta;

				sdx += cx;
				sdy += cy;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
  }  
  else if (new_mode == 3) {
		// LOW FREQ SINE WAVES
		
    warp_prefs.stars_prob *= 0.5;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;

		const N = 15;

		const str = (0.4 + 0.6 * randoms[rand_idx++]) * 0.75;
  	
  	let theta = new Float32Array(N);
  	let cos_theta = new Float32Array(N);
  	let sin_theta = new Float32Array(N);
  	let freq = new Float32Array(N);
  	let amp = new Float32Array(N);

		const base_theta = randoms[rand_idx++];
		let theta_sum = 0.0;
		for (let i = 0; i < N; i++) {
	  	theta[i] = (base_theta + i/N) * 6.28;
	  	cos_theta[i] = Math.cos(theta[i]);
	  	sin_theta[i] = Math.sin(theta[i]);
	  	amp[i] = randoms[rand_idx++] * str * 0.002 * 0.5 * 1.3;
	  	freq[i] = randoms[rand_idx++] * 12 * 2;

	  	theta_sum += theta[i];
		}
  	
  	const cos_undo = Math.cos(-theta_sum);
  	const sin_undo = Math.sin(-theta_sum);

		// We generate it first at reduced resolution, then upsample it.
		const SCALE = 4;
		const W2 = (W / SCALE + 1) | 0;
		const H2 = (H / SCALE + 1) | 0;
		let temp_x = new Float32Array(W2 * H2);
		let temp_y = new Float32Array(W2 * H2);
  	
	  for (let dy = 0; dy < H2; dy++) {
		  let fdy = (dy * (2.0 / H2) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W2; dx++) {
			  let fdx = (dx * (2.0 / W2) - 1.0) * (x0x1 * 0.5);	

				let x = fdx;
				let y = fdy;
				
				for (let i = 0; i < N; i++) {
					let x2 = x * cos_theta[i] - y * sin_theta[i];
		      let y2 = x * sin_theta[i] + y * cos_theta[i];
		      x = x2;
		      y = y2;
	
		      x += Math.cos(y * freq[i]) * amp[i];					
				}
				
				let sdx = x * cos_undo - y * sin_undo;
	      let sdy = x * sin_undo + y * cos_undo;

		    temp_x[dy * W2 + dx] = sdx - fdx;
		    temp_y[dy * W2 + dx] = sdy - fdy;
      }
    }

		// Now upsample.
		// Note that if we had SIMD, it would be faster 
		// to upsample first in X, then in Y.
		write_offset = 0;
	  for (let dy = 0; dy < H; dy++) {
	  	const fsy = dy * (H2 - 1) / H;
	  	const sy = Math.floor(fsy);
	  	const fy = fsy - sy;
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
		  	const fsx = dx * (W2 - 1) / W;
		  	const sx = Math.floor(fsx);
		  	const fx = fsx - sx;
        
        const o1 = (sy + 0) * W2 + (sx + 0);
        const o2 = (sy + 0) * W2 + (sx + 1);
        const o3 = (sy + 1) * W2 + (sx + 0);
        const o4 = (sy + 1) * W2 + (sx + 1);
        
				const w1 = (1 - fy) * (1 - fx);
				const w2 = (1 - fy) * (    fx);
				const w3 = (    fy) * (1 - fx);
				const w4 = (    fy) * (    fx);

				const vx = temp_x[o1] * w1 +
									 temp_x[o2] * w2 +
				           temp_x[o3] * w3 +
									 temp_x[o4] * w4;
				const vy = temp_y[o1] * w1 +
									 temp_y[o2] * w2 +
				           temp_y[o3] * w3 +
									 temp_y[o4] * w4;

				//if (!Number.isFinite(vx) || !Number.isFinite(vy)) {
				//  console.error("NaN/Inf in upsample", { dx, dy, sx, sy, fx, fy, o1, o2, o3, o4 });
				//  break;
				//}
					    	
		    src_dxy[write_offset + 0] = vx;
		    src_dxy[write_offset + 1] = vy;
		  }
		}
	}
	else if (new_mode == 7) {
		// N randomly-placed swirls; can overlap;
		// like mode 4, but with way more swirls, starting large and
		// getting progressively smaller.

		//warp_prefs.type_circ_prob *= 0.1;
    warp_prefs.stars_prob *= 0.5;
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;
		
		// Clear the motion map to zero everywhere.
		for (let dy = 0; dy < H; dy++) {
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
		    src_dxy[write_offset + 0] = 0.0;
		    src_dxy[write_offset + 1] = 0.0;	    
      }
    }			  		

		// Add a few big swirles, that can overlap.
		const N = (2 + 240 * randoms[rand_idx++]) | 0;
		const str_big = 0.03;
		const str_lil = 0.03 + 0.05 * randoms[rand_idx++];
		for (let n = 0; n < N; n++) {
			const t = n / (N - 1);  // [0..1] [big..lil]
			
			const cx = (randoms[rand_idx++] - 0.5) * x0x1;
			const cy = (randoms[rand_idx++] - 0.5) * y0y1;
			const rad = 0.8 - 0.75 * Math.pow(t,0.25);//0.3 + 0.5 * randoms[rand_idx++] * Math.min(1.0, 10.0 / N);
			const str = (0.2 + 0.3 * randoms[rand_idx++]) * 
					((randoms[rand_idx++] > 0.5) ? 1 : -1) *
					(str_big + (str_lil - str_big) * t);
			
		  const dx0 = Math.max(0, Math.min(W, ((cx - rad) * inv_x0x1 + 0.5) * W)) | 0;
		  const dy0 = Math.max(0, Math.min(H, ((cy - rad) * inv_y0y1 + 0.5) * H)) | 0;
		  const dx1 = Math.max(0, Math.min(W, ((cx + rad) * inv_x0x1 + 0.5) * W)) | 0;
		  const dy1 = Math.max(0, Math.min(H, ((cy + rad) * inv_y0y1 + 0.5) * H)) | 0;

			for (let dy = dy0; dy < dy1; dy++) {
			  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
			  write_offset = (dy * W + dx0) * 2;
		    for (let dx = dx0; dx < dx1; dx++, write_offset += 2) {
				  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);		// -1 .. 1

					let rad_sq = ((fdx - cx) * (fdx - cx) + (fdy - cy) * (fdy - cy)) * (1.0 / (rad * rad));
					if (rad_sq < 1) {
						let r = Math.pow(rad_sq, 0.333);
						
						let sdx = fdx;
						let sdy = fdy;
						
						r = smoothstep(r);
						let zx = fdx - cx;
						let zy = fdy - cy;
						let rot = (1.0 - r) * str;//rad_sq * 0.15;
						let zx2 = (zx * Math.cos(rot) - zy * Math.sin(rot));
						let zy2 = (zx * Math.sin(rot) + zy * Math.cos(rot));
						sdx = cx + zx2;
						sdy = cy + zy2;

				    src_dxy[write_offset + 0] += (sdx - fdx);
				    src_dxy[write_offset + 1] += (sdy - fdy);
					}
				}   
      }

			if (rand_idx > randoms.length - 8) {
				rand_idx = (randoms[rand_idx] * 43) | 0;
			}  
    }			  		
	}
  // -------------- OLD GEISS MODES BELOW THIS LINE --------------
  else if (new_mode == 18) {
    /*
    // This was the old Geiss mode 7.  Use this as reference for porting code!
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    // Note: Had to move 'scale' toward 1 here because nowadays we run at 120 fps, vs. 30 before.
    const f1 = 0.98 + 0.01 * randoms[rand_idx++];
    // Note: This became (640/2) or 320, but then we reduced that to 80, another 4x, to account for higher fps.
    const f2 = (0.0006 + 0.0005 * randoms[rand_idx++]) * 80;
    // Note: Had to decrease 'turn' by ~75% to account for higher fps, too.
    const turn = 0.15 * (0.01 + 0.01 * randoms[rand_idx++]) * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);

	  for (let dy = 0; dy < H; dy++) {
		  const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2*dx2 + dy2*dy2) * f2;
        // Note: We can get rid of 'protective_factor', since displacements are now unlimited.
        const scale = f1 - r + randoms[rand_idx++] * 0.01;
        rand_idx %= (randoms.length - 3);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
        // Note: we can now always get rid of 'damping'.
        // Note: we can now always get rid of the wrapping.

        // Write relative displacements for X and Y.
        src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
    */

    // Port of mode 7 from Geiss.  It's just f(rad) zoom & rotation, plus some noise.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const f1 = 0.98 + 0.02 * randoms[rand_idx++];
    const f2 = (0.0006 + 0.0005 * randoms[rand_idx++]) * 80;
    const turn = 0.15 * (0.01 + 0.01 * randoms[rand_idx++]) * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);

	  for (let dy = 0; dy < H; dy++) {
		  const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2*dx2 + dy2*dy2) * f2;
        const scale = f1 - r + randoms[rand_idx++] * 0.01;
        rand_idx %= (randoms.length - 3);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);

        src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
  }
  else if (new_mode == 44) {
    // Geiss mode 3.  Y-dependent zoom plus rotation. The old randomized initial scale was overwritten per pixel, so it can be discarded. Its vertical coefficient uses 480 / 2 = 240.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.01 + 0.015 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        // Original: 0.95 - normalizedPixelY * 0.0005.
        // Convert Y by 240, then reduce motion by 4.
        const scale = 0.9875 - dy2 * 0.03;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  /*else if (new_mode == 89) {
    // crappy cubismo, half-finished 
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const squares = 5 + 10 * randoms[rand_idx++];    
    const phase_x = randoms[rand_idx++];    
    const phase_y = randoms[rand_idx++];    
    const push = 0.02 + 0.10 * randoms[rand_idx++];    
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        
        const nx = (fdx * 0.5 + 0.5) * squares + phase_x;
        const ny = (fdy * 0.5 + 0.5) * squares + phase_y;
        const nx2 = Math.floor(nx + 0.5);
        const ny2 = Math.floor(ny + 0.5);
        
        //const push = 0.005 + 0.3 * randoms[(ny * squares + nx) % WarpMapRandomNumberCount];    
        const dx2 = (nx2 - nx) * push * (1.0 / squares);
        const dy2 = (ny2 - ny) * push * (1.0 / squares);
          
        src_dxy[write_offset + 0] = dx2;
        src_dxy[write_offset + 1] = dy2;
      }
    }
  }      */
  else if (new_mode == 13) {
    // Geiss mode 4 - sphere-like zoom plus rotation. Outward motion inside the circular boundary, inward outside it.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2);
    
        // Original: 0.9 + pixelRadius * 0.0025 * 0.14.
        // Radius conversion and FPS adjustment: 320 / 4 = 80.
        const scale = 0.975 + r * 0.028;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 21) {
    // Geiss mode 5 - perspective zoom plus rotation. The original has two variants controlled by effect[NUCLIDE]. Set sqrtRadius to true for the old == -1 branch, or false for the other branch.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);
    
    // Reduce both components of the zoom motion by 4.
    const f1 = (0.05 + 0.05 * randoms[rand_idx++]
      + 0.07 * randoms[rand_idx++]) * 0.25;
    const f2 = 0.9975 - 0.0025 * randoms[rand_idx++]
      - 0.005 * randoms[rand_idx++];
    const sqrtRadius = true;
    const turn = 0.15 * (0.01 + 0.03 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        // Original radius normalization: pixelRadius / 200.
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2) * 1.6;
        const perspective = sqrtRadius ? Math.sqrt(r) : r * 1.7;
        const scale = f2 - f1 * perspective;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 11) {
    // Geiss mode 8 - beautiful rippled outward zoom plus rotation. The radius conversion stays inside the sine’s argument; only the zoom amplitude and baseline motion are reduced for FPS, preserving the ring spacing.
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 4.0;

    const cx = -0.06 + 0.12 * randoms[rand_idx++];
    const cy = -0.06 + 0.12 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const f1 = 1.4 + 0.2 * randoms[rand_idx++] + 2.7 * Math.pow(randoms[rand_idx++], 6);
    const turn = 0.0075 * randoms[rand_idx++]
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1) * 0.5;
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    const damping = 1.0;//0.5;

    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2) * 320;
    
        // Original: 0.85 + 0.1 * sin(sqrt(r) * f1).
        //const scale = 0.9625 + 0.025 * Math.sin(Math.sqrt(r) * f1);
        //const scale = 0.975 + 0.02 * Math.sin(Math.sqrt(r) * f1);
        const scale = 0.98 + 0.017 * Math.sin(Math.sqrt(r) * f1);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = (sdx - fdx) * damping;
        src_dxy[write_offset + 1] = (sdy - fdy) * damping;
      }
    }
  }
  else if (new_mode == 40) {
    // Geiss mode 9 - checkerboard blend of radial zoom and uniform zoom, with shared rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);
    
    // The alternate pixels retain the original independently chosen scale.
    const scale2 = 0.95 + 0.0625 * randoms[rand_idx++];
    const f1 = 0.995 + 0.0025 * randoms[rand_idx++];
    const f2 = (0.0009 + 0.0012 * randoms[rand_idx++]) * 80;
    const turn = 0.15 * (0.01 + 0.03 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2) * f2;
        const scale = ((dx & 1) === (dy & 1)) ? f1 - r : scale2;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 41) {
    // Geiss mode 10 - horizontal contraction that varies with Y, plus vertical contraction toward the top. The old y / FXH becomes (fdy + 1) / 2, so the effect stays anchored to the full coordinate space rather than the crop.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = 0.0;
    AdjustCenter(cx, cy, warp_prefs);

    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
    
        // Original X scale: 1.03 + 0.03 * (y / FXH).
        const scaleX = 1.0075 + 0.00375 * (fdy + 1);
        const sdx = cx + dx2 * scaleX;
    
        // Original Y mapping: y * 1.04, anchored at the top edge.
        const sdy = -1 + (fdy + 1) * 1.01;
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 26) {
    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 0.333;

    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const baseScale = 1.008 + 0.008 * randoms[rand_idx++];
    const scale1 = 1 + (baseScale * 0.99 - 1) * 0.25;
    //const scale2 = 1 + (baseScale * 1.01 - 1) * 0.25;
    const baseTurn = 0.15 * (0.04 + 0.14 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const turn1 = baseTurn * -0.6;
    //const turn2 = baseTurn * 0.1;
    const c1 = Math.cos(turn1);
    const s1 = Math.sin(turn1);
    //const c2 = Math.cos(turn2);
    //const s2 = Math.sin(turn2);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        //const first = ((dx & 1) === (dy & 1));
        const scale = scale1;//first ? scale1 : scale2;
        const c = c1;//first ? c1 : c2;
        const s = s1;//first ? s1 : s2;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 43) {
    // Geiss mode 12 - nonlinear horizontal splitter; no vertical motion or rotation. Here, the 4× FPS reduction is applied directly to the displacement because this isn’t a scale-and-rotation transform.
    // Mode 12 also reduced the old interpolation weight sum by 2%, adding extra fade. That belongs in the feedback/brightness handling if we want to preserve it; it doesn’t affect the motion map.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = 0;
    AdjustCenter(cx, cy, warp_prefs);

    const sqrtConversion = 1 / Math.sqrt(320);
    const centerThreshold = 0.5 / 320;
    const offset = 0.9 / 320;

    warp_prefs.flat_angle_lo = 3.1415927 * 0.5 - 0.1; 
    warp_prefs.flat_angle_hi = 3.1415927 * 0.5 + 0.1;
		warp_prefs.flat_is_stereo_chance = 0.0;
  
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
    
        const sourceX = dx2 < -centerThreshold
          ? cx - Math.sqrt(-dx2) * sqrtConversion + offset
          : dx2 > centerThreshold
            ? cx + Math.sqrt(dx2) * sqrtConversion - offset
            : cx;
    
        src_dxy[write_offset + 0] = (sourceX - fdx) * 0.25;
        src_dxy[write_offset + 1] = 0;
      }
    }
  }
  else if (new_mode == 27) {
    // Geiss mode 13 - black-hole zoom plus rotation. Inward near the center, outward farther away; the radial term grows as rad^1.5.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;

    const f1 = 0.92 + 0.16 * randoms[rand_idx++];
    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2) * 320;
    
        // Original: 1 + (0.04 - r * sqrt(r) * 0.00025 * 0.14) * f1.
        const scale = 1 + (0.01 - r * Math.sqrt(r) * 0.00000875) * f1;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 28) {
    // Geiss mode 14 - horizontal bands of inward/outward zoom, plus rotation. The original adds a tiny per-pixel random variation to the cosine’s denominator; this preserves it using 480 as the reference height.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        //const jitter = randoms[rand_idx++];
        //rand_idx %= (randoms.length - 3);
        //const phase = dy2 * 6 / (1 + jitter / 480);
        //const phase = dy2 * 6;    // ...6 was too much for a square window
        const phase = dy2 * 4.5;
        const scale = 0.975 + 0.05 * Math.cos(phase);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 29) {
    // Geiss mode 15 - angular zoom modulation producing 2–6 petals, plus rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const petals = 2 + Math.floor(5 * randoms[rand_idx++]);
    const f2 = 0.988 + 0.01 * randoms[rand_idx++];
    const f3 = (0.0125 + 0.0125 * randoms[rand_idx++]) * 0.25;
    const turn = 0.15 * (0.04 * randoms[rand_idx++]
      + 0.045 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1) * 0.5;
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        const angle = Math.atan2(dy2, dx2);
        const scale = f2 + f3 * Math.sin(angle * petals);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 14) {
    // Geiss mode 16 - quadratic radial zoom plus rotation. Its original lower scale limit of -1.5 becomes 0.375 after bringing the scale toward 1 by 4×.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    warp_prefs.stars_follow_motion_min = kFollowMotionNone;
    warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
    warp_prefs.stars_follow_motion_pow = 3.0;

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r2 = dx2 * dx2 + dy2 * dy2;
    
        // Radial coefficient: 320² * 0.00025 * 0.09 / 4 = 0.576.
        const scale = Math.max(0.375, 1.0125 - r2 * 0.576);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 31) {
    // Geiss mode 17 - horizontal tunnel: outward zoom strengthens with Y², plus rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        // Original: 0.97 - Y² * 0.40.
        const scale = 0.9925 - dy2 * dy2 * 0.10;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 32) {
    // Geiss mode 18 - vertical tunnel: outward zoom strengthens with X², plus rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        // Original: 0.97 - X² * 0.40.
        const scale = 0.9925 - dx2 * dx2 * 0.10;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 33) {
    // Geiss mode 19 - black-hole/vortex zoom: inward near the center, outward farther away, plus rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2);
    
        // Original: 1.04 - rad * 0.25.
        const scale = 1.01 - r * 0.0625;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 23) {
    // Geiss mode 20 -  curved Y-dependent perspective zoom, plus rotation.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        // Original: 1.15 - sqrt(Y + 1.4) * 0.20.
        const scale = 1.0375 - Math.sqrt(dy2 + 1.4) * 0.05;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  /*else if (new_mode == XXX) {
    // Geiss mode 21 - blocky zoom-speed regions based on stepped abs(X) and abs(Y).
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
    
        const stepsX = Math.floor(Math.abs(dx2) * 10);
        const stepsY = Math.floor(Math.abs(dy2) * 10);
        const scale = 0.9875 - (stepsX + stepsY) * 0.0075;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }*/
  else if (new_mode == 24) {
    // Geiss mode 22 - concentric rings whose outward zoom speed increases in steps.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2);
    
        const ring = Math.floor(r * 10);
        const scale = 0.9875 - ring * 0.01;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 25) {
    // Geiss mode 23 - concentric rings cycling through four outward zoom speeds.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2);
    
        const ring = Math.floor(r * 20) % 4;
        const scale = 0.9875 - ring * 0.03;
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
  else if (new_mode == 19) {
    // Geiss mode 25 - smooth perspective zoom with reciprocal radial scaling. This reduces the original scale’s deviation from 1 by 4×, preserving its radial profile.
    const cx = -0.1 + 0.2 * randoms[rand_idx++];
    const cy = -0.1 + 0.2 * randoms[rand_idx++];
    AdjustCenter(cx, cy, warp_prefs);

    const turn = 0.15 * (0.007 + 0.02 * randoms[rand_idx++])
      * ((randoms[rand_idx++] < 0.5) ? 1 : -1);
    const c = Math.cos(turn);
    const s = Math.sin(turn);
    
    for (let dy = 0; dy < H; dy++) {
      const fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
      for (let dx = 0; dx < W; dx++, write_offset += 2) {
        const fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);
        const dx2 = fdx - cx;
        const dy2 = fdy - cy;
        const r = Math.sqrt(dx2 * dx2 + dy2 * dy2);
    
        // Original: 3 / (3 + rad).
        const scale = 1 - 0.25 * r / (3 + r);
        const sdx = cx + scale * (dx2 * c - dy2 * s);
        const sdy = cy + scale * (dx2 * s + dy2 * c);
    
        src_dxy[write_offset + 0] = sdx - fdx;
        src_dxy[write_offset + 1] = sdy - fdy;
      }
    }
  }
	else if (new_mode == 98 || new_mode == 99) {
		warp_prefs.wave_prefs_weight = 0.00001;
		
		warp_prefs.flat_angle_lo = 0.0;
		warp_prefs.flat_angle_hi = 0.0;
		warp_prefs.flat_is_stereo_chance = 0.0;
		warp_prefs.flat_cx_lo = -0.1,
		warp_prefs.flat_cx_hi =  0.1,
		warp_prefs.flat_cy_lo = -0.2,
		warp_prefs.flat_cy_hi =  0.2,

		warp_prefs.use_motion_center_as_wave_center_prob = 0.0;
	}
  /*else if (new_mode == 42) {
		// Name of the Wind
		  	
  	//warp_prefs.wave_prefs_weight *= 0.1;

		const N = 3;

		const str = 0.4 + 0.6 * randoms[rand_idx++];  // strength of the LF waves

		const wind = 
				(0.5 + 0.5 * randoms[rand_idx++]) * 
				((randoms[rand_idx++] < 0.5) ? 1.0 : -1.0) * 
				0.012;
  	
  	let theta = new Float32Array(N);
  	let cos_theta = new Float32Array(N);
  	let sin_theta = new Float32Array(N);
  	let freq = new Float32Array(N);
  	let amp = new Float32Array(N);

		let theta_sum = 0.0;
		for (let i = 0; i < N; i++) {
	  	theta[i] = randoms[rand_idx++] * 6.28;
	  	cos_theta[i] = Math.cos(theta[i]);
	  	sin_theta[i] = Math.sin(theta[i]);
	  	amp[i] = randoms[rand_idx++] * str * 0.002;
	  	freq[i] = randoms[rand_idx++] * 12;

	  	theta_sum += theta[i];
		}
  	
  	const cos_undo = Math.cos(-theta_sum);
  	const sin_undo = Math.sin(-theta_sum);
  	
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				let x = fdx;
				let y = fdy;
				
				for (let i = 0; i < N; i++) {
					let x2 = x * cos_theta[i] - y * sin_theta[i];
		      let y2 = x * sin_theta[i] + y * cos_theta[i];
		      x = x2;
		      y = y2;
	
		      x += Math.cos(y * freq[i]) * amp[i];					
				}
				
				let sdx = x * cos_undo - y * sin_undo;
	      let sdy = x * sin_undo + y * cos_undo;
	      
	      // Add wind:
	      sdx += wind;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
	}
  else if (new_mode == 17) {
  	// JUPITER STRIPES
  	
  	//warp_prefs.wave_prefs_weight *= 0.1;
\
		const wind = 
				(0.5 + 0.5 * randoms[rand_idx++]) * 
				((randoms[rand_idx++] < 0.5) ? 1.0 : -1.0) * 
				0.01;

		const phase = randoms[rand_idx++] * 6.28;
		const freq = 5.0 + 9.0 * randoms[rand_idx++];
		const amp = 0.3 + 0.7 * randoms[rand_idx++];
		const dir = (randoms[rand_idx++] < 0.5) ? -1.0 : 1.0;
  	
	  for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	

				let t = 0.5 + 0.5 * Math.cos(fdy * freq + phase);
				t *= t;
				t *= t;
				t *= amp;

				let sdx = fdx + t * dir * wind;
				let sdy = fdy;

	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }
	}*/
	else if (new_mode == 17) {
		// VORTEX
		const cx = (randoms[rand_idx++] * 2 - 1) * 0.08;
		const cy = (randoms[rand_idx++] * 2 - 1) * 0.08;
    AdjustCenter(cx, cy, warp_prefs);

    for (let dy = 0; dy < H; dy++) {
		  let fdy = (dy * (2.0 / H) - 1.0) * (y0y1 * 0.5);
	    for (let dx = 0; dx < W; dx++, write_offset += 2) {
			  let fdx = (dx * (2.0 / W) - 1.0) * (x0x1 * 0.5);	
	
				const tx = fdx - cx;
				const ty = fdy - cy;

				const rad1 = Math.sqrt(tx * tx + ty * ty);
				
				const inv_rad1 = 1.0 / (rad1 + 0.0001);
				const nx = tx * inv_rad1;
				const ny = ty * inv_rad1;
				
				const dist1 = inv_rad1;
				const dist2 = dist1 + 0.01;
				const rad2 = 1.0 / dist2 - 0.0001;

				let tx2 = rad2 * nx;
				let ty2 = rad2 * ny;				
			
				//const rot = 0.02;

				//const rotated = ApplyRot2D(tx, ty, rot);
				
				let sdx = cx + tx2;
				let sdy = cy + ty2;
      	
	      
		    src_dxy[write_offset + 0] = (sdx - fdx);
		    src_dxy[write_offset + 1] = (sdy - fdy);	    
      }
    }			  		
  }
	
	//----------------------------------------------------------------------
	//----------------------------------------------------------------------
	//----------------------------------------------------------------------

  let dynamic_motion_params = null;
  if (is_dynamic) {
    src_dxy = null;

    dynamic_motion_params = new Float32Array(kDynamicMotionParamCount);
    for (let i = 0; i < kDynamicMotionParamCount; i++) {
      dynamic_motion_params[i] = randoms[rand_idx++];
    }
  }


  if (rand_idx > WarpMapRandomNumberCount) {
  	throw new Error("Too many random numbers used when generating warp map for mode ", new_mode);
  }

  /*
  // Populate motion metadata by sampling some of the motion.
	{
		// Reminder that W,H here are both kWarpMapSize.
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
		warp_prefs.net_motion           = net_motion / sample_count;
		warp_prefs.net_zoom_motion      = net_zoom_motion / sample_count;
		warp_prefs.in_or_out_motion     = in_or_out_motion / sample_count;
		warp_prefs.net_clockwise_motion = net_clockwise_motion / sample_count;
		warp_prefs.cw_or_ccw_motion     = cw_or_ccw_motion / sample_count;
	}
  */
	
  let breathing_str = randoms[rand_idx++] * 0.4;//0.5;//0.6;  //TWEAK
  let breathing_phase = randoms[rand_idx++] * 6.28;
  //let breathing_freq = 0.7 * Math.pow(1.65, randoms[rand_idx++] * 2 - 1);  //TWEAK
  //let breathing_freq = 0.6 * Math.pow(1.92, randoms[rand_idx++] * 2 - 1);  //TWEAK
  let breathing_freq = 0.5 * Math.pow(1.92, randoms[rand_idx++] * 2 - 1);  //TWEAK

  return { mode : new_mode, 
				   // TODO: Remove W,H return params?
  				 W : W,
  				 H : H,
  				 warp_prefs : warp_prefs,
           // For static maps, this will be non-null.
  	       src_dxy,
           // For dynamic maps, this will be non-null.
           dynamic_motion_params : dynamic_motion_params,
           seed : seed,
           breathing_str   : breathing_str,
           breathing_phase : breathing_phase,
           breathing_freq  : breathing_freq };
}
