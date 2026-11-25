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

// gpu_warp.js
// Four independent normalized channels in ping-pong rgba16float textures.
// Warp maps: rg16float (FP16), sampled bilinear.

import { kWarpMapSize, kNoiseTexSize, kGpuWarpSlots } from "./const.js"
import { GpuFadingDots } from "./gpu_advected_dots.js";
import { GpuDynamicWarp } from "./gpu_dynamic_warp.js";
import { TransformHelper } from "./coords.js"
import { DotRenderer } from "./dot_renderer.js";
import { create_noise_textures, get_noise_index_for_frame } from "./noise_textures.js";
import { GpuTextOverlay } from "./gpu_text_overlay.js";

// See also: kMaxPresetHistorySize
// See also: kPresetKeepInMemoryRadius
const kGpuWarpMapCacheSize = 20;  // Each one is kWarpMapSize * kWarpMapSize * 2 * 2 bytes (~1 MB)

const g_verbose = false;

// Shared uniforms.
// KIV: Make sure the size of 'this.uniform_buf' below is large enough to hold all of these.
const kUniforms = 
`struct WarpUniforms {
  // strengths for each warp map (in pixels per pixel, if warp maps are in pixels)
  //-----------
  w0 : f32,
  w1 : f32,
  w2 : f32,
  w3 : f32,
  //-----------
  w4 : f32,
  w5 : f32,
  w6 : f32,
  w7 : f32,
  //-----------

  // Optional global shift in pixels (can leave 0)
  shift_x : f32,
  shift_y : f32,

  // dimensions (float for convenience)
  warp_width  : f32,
  warp_height : f32,
  //-----------
  index_width  : f32,
  index_height : f32,
  index_width_inv  : f32,
  index_height_inv : f32,
  //-----------
  x0x1 : f32,
  y0y1 : f32,
  inv_x0x1 : f32,
  inv_y0y1 : f32,
  //-----------
	noise_offset_x : f32,
	noise_offset_y : f32,
  darkening : f32,		// [-1..1]
  true_time : f32,
  //-----------
  // From ShaderParams / shader_params:
  //-----------
  rx_diff_grey_scott_str : f32,
  rx_diff_grey_scott_str2 : f32,
  diffusion_colorization : f32,
  rx_diff_barkley_str : f32,
  //------------
  frequency_injector_str : f32,
  frequency_injector_freq_scale : f32,
  painterly_str : f32,
  sharpen_str : f32,
  //------------
  noise_str : f32,
  oversample : f32,
  oversample_inv : f32,
  layer2_motion_scale : f32,
  //------------
};
`;

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
// warp_pass.wgsl
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
// Shared by image warping and point advection. Pixel coordinates here are
// fragment coordinates (the first pixel center is 0.5, 0.5).
const kWarpFieldWGSL = `
fn warp_displacement_pixels(pixel: vec2f,
                            weights0123: vec4f,
                            weights4567: vec4f) -> vec2f {
  var uv = pixel / vec2f(u.index_width - 1.0, u.index_height - 1.0);
  uv = (uv - vec2f(0.5)) * vec2f(u.x0x1, u.y0y1);
  uv = uv * 0.5 + vec2f(0.5);
  uv += vec2f(0.5 / u.warp_width, 0.5 / u.warp_height);
  let wv0 = textureSampleLevel(warp0, samp, uv, 0.0).xy;
  let wv1 = textureSampleLevel(warp1, samp, uv, 0.0).xy;
  let wv2 = textureSampleLevel(warp2, samp, uv, 0.0).xy;
  let wv3 = textureSampleLevel(warp3, samp, uv, 0.0).xy;
  let wv4 = textureSampleLevel(warp4, samp, uv, 0.0).xy;
  let wv5 = textureSampleLevel(warp5, samp, uv, 0.0).xy;
  let wv6 = textureSampleLevel(warp6, samp, uv, 0.0).xy;
  let wv7 = textureSampleLevel(warp7, samp, uv, 0.0).xy;
  let displacement = 
      wv0 * u.w0 * weights0123.x + 
      wv1 * u.w1 * weights0123.y + 
      wv2 * u.w2 * weights0123.z + 
      wv3 * u.w3 * weights0123.w + 
      wv4 * u.w4 * weights4567.x + 
      wv5 * u.w5 * weights4567.y + 
      wv6 * u.w6 * weights4567.z + 
      wv7 * u.w7 * weights4567.w;
  // Preserve the existing 0.5 motion scale in both consumers.
  return displacement * vec2f(u.inv_x0x1 * 0.5 * u.index_width,
                              u.inv_y0y1 * 0.5 * u.index_height);
}
`;

export const kWarpWGSL = kUniforms + kWarpFieldWGSL + 
`
struct ExposureOut {
  decay : f32,
  _pad0 : f32,
  _pad1 : f32,
  _pad2 : f32,
};

@group(0) @binding(0) var old_tex : texture_2d<f32>;        // rgba16float view (sampleType float)
@group(0) @binding(1) var warp0   : texture_2d<f32>;        // rg16float
@group(0) @binding(2) var warp1   : texture_2d<f32>;
@group(0) @binding(3) var warp2   : texture_2d<f32>;
@group(0) @binding(4) var warp3   : texture_2d<f32>;
@group(0) @binding(5) var warp4   : texture_2d<f32>;
@group(0) @binding(6) var warp5   : texture_2d<f32>;
@group(0) @binding(7) var warp6   : texture_2d<f32>;
@group(0) @binding(8) var warp7   : texture_2d<f32>;
@group(0) @binding(9) var samp    : sampler;               // filtering sampler
@group(0) @binding(10) var<uniform> u : WarpUniforms;
@group(0) @binding(11) var noise_tex : texture_2d<f32>;      // r8unorm, loaded via textureLoad
@group(0) @binding(12) var<storage, read> exposure : ExposureOut;
@group(0) @binding(13) var index_samp : sampler; // linear, repeat on both axes

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
  var pos = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(-1.0,  3.0),
    vec2f( 3.0, -1.0)
  );
  let p = pos[vid];
  return vec4f(p, 0.0, 1.0);
}

// p.xy : the output coordinates in [0..index_width - 1, 0 .. index_height - 1]
@fragment
fn fs_main(@builtin(position) p: vec4f) -> @location(0) vec4f {

  let inv_size = vec2f(u.index_width_inv, u.index_height_inv);
  let base_pos = p.xy + vec2f(u.shift_x, u.shift_y);
  let weights0123 = vec4f(u.w0, u.w1, u.w2, u.w3);
  let weights4567 = vec4f(u.w4, u.w5, u.w6, u.w7);
  var warp_vec1 = warp_displacement_pixels(p.xy, vec4f(1.0), vec4f(1.0));



  // Painterly:
  if (u.painterly_str > 0.0) {
    let sample0 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
    let vel_scale = 1.0 + ((sample0.r - sample0.b) * 0.2 - 0.05) * (u.painterly_str * 3);
    warp_vec1 *= vel_scale;

    //xxx - this alternative was pretty cool:
    //warp_vec1 += warp_vec1.yx * vec2f(-1.0, 1.0) * (vel_scale - 1);

    //xxx - this alternative was pretty cool:
    //let t = vel_scale - 1;
    //warp_vec1 = warp_vec1.xy * (1.0 - t) + warp_vec1.yx * vec2f(-1.0, 1.0) * t;

    //xxx - this alternative was pretty cool:
    //let t = (vel_scale - 1) * 2;
    //warp_vec1 = warp_vec1.xy * (1.0 - t) - warp_vec1.xy * t;
  }

  // Do 4-channel noise lookup.
  let nx = (u32(p.x * u.oversample_inv) + u32(u.noise_offset_x)) & (256u - 1u);
  let ny = (u32(p.y * u.oversample_inv) + u32(u.noise_offset_y)) & (256u - 1u);
  let n01 = textureLoad(noise_tex, vec2u(nx, ny), 0);  // [0,1]
  let noise = n01 * 2.0 - 1.0;                               // [-1,1]


  // If the displacement is significantly less than 1 pixel, add noise, so that you
  // at least get a tiny bit of softening -- we don't want those regions to remain
  // oddly crisp.
  const min_warp_displacement_pixel_frac = 7;   // 5 ~ 1/5th of a pixel
  let warp_noise = 1.0 - saturate(length(warp_vec1) * min_warp_displacement_pixel_frac);
  warp_vec1 += noise.zw * warp_noise * 0.1;



  var sample1 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
  //var sample3 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
  
  // Apply a little extra softening, over time, on some channels.
  let kDist = 1.33 * u.oversample;    // [1 .. 1.5]   ~ determines the wavelength (size/spacing) of the stripes.  Use 1.33 for either.
  let softer = (     
      // Use 4 queens pattern:
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f( 1.5, -0.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f(-1.5,  0.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f( 0.5,  1.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f(-0.5, -1.5) * kDist)) * inv_size, 0.0)) * 0.25;

  // For Barkley diffusion: weaker blur -> smaller AND slower spirals
  // NOTE: Softening on 4th channel is ignored right now -- overridden below...
  const kRgbSoften = vec4f(0.0, 0.05, 1.0, 0.0);// 0.7;//0.05;//0.7;        // [0..1]    Use 0.05 for Barkley (~G).  Use 0.7 for grey-scott (~B).
  var values = sample1 * (vec4f(1.0,1.0,1.0,1.0) - kRgbSoften) + kRgbSoften * softer;
  
  // Alpha channel has no softening, and instead, warps at a different speed.
  var sample2 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 * u.layer2_motion_scale) * inv_size, 0.0);
  values.w = sample2.w;




  // Soft frequency injector:
  if (u.frequency_injector_str > 0.0) {
    // Note: Here we scale the adjustment intensity by 1/~oversample, to help reduce
    // the overzealous (contrasty) impact @ higher oversamples (if not adjusted).
    values.xy += sin((values.x - values.y) * (3.8 * u.frequency_injector_freq_scale)) * (0.0085 * u.frequency_injector_str * (1 * 0.15 + 0.85 * u.oversample_inv));
    values.xyz = saturate(values.xyz);
  }      
  


  
  // Add some grey-scott reaction diffusion.
  if (u.rx_diff_grey_scott_str > 0.0)
  {
    // G: your weighted Gaussian samples of the old G channel.
    let R = values.r;
    let G = values.b;
    
    const feed = 0.035;
    const kill = 0.059;//0.06;//0.05;//0.05;//0.062;    // 0.06 ~ little dots; 0.05 ~ stripes
    let dt = 1.0 * u.rx_diff_grey_scott_str;
    
    // Calculate from the old values before updating either channel.
    let reaction = G * R * R;
    
    let next_R = R + dt * (
        reaction - (feed + kill) * R
    );
    
    let next_G = G + dt * (
        -reaction + feed * (1.0 - G)
    );
    
    values.r = saturate(next_R);
    values.b = saturate(next_G);
  }


  
  // Add Barkley reaction-diffusion.  Note that this wants the G channel blur (above) to be much slower/milder,
  // otherwise the spirals are huge and move fast.
  /*if (u.rx_diff_barkley_str > 0.0) {
    let R = values.g;  // excitation
    let G = values.r;  // recovery
    
    const a = 0.75;
    const b = 0.02;
    const epsilon = 0.02;
    let dt = 0.03;//0.04 * u.rx_diff_barkley_str;//0.01;   // smaller timestep -> smaller AND slower spirals
    
    let threshold = (G + b) * (1.0 / a);
    let dR = R * (1.0 - R) * (R - threshold) * (1.0 / epsilon);
    let dG = R - G;
    
    values.g = saturate(R + dt * dR);
    values.r = saturate(G + dt * dG);    
  }*/







	// Add noise.
  // Tiny unfiltered noise (textureLoad). Wrap by & (size-1) since size is 256.
  // p.xy are pixel coords in the output render target space.
  // Note that we divide by the oversample factor so that the visual noise
  //   is at the frequency of the display, rather than the canvas.
  //
  // Sprinkle: pick a *tiny* scale. Tune later.
  // Multiplicative noise:  (affects all brightnesses equally)
  //let kNoiseStrength = 1.0 + u.noise_str * 0.03; //1.015;
	//values *= pow(kNoiseStrength, noise);
  // Additive noise:  (nice because it affects deep shadows only)
  // Total noise scale reference:
  //   0 ~ smooth.  0.005 ~ barely visible.  0.01 ~ nice, subtle noise grain.  0.03 ~ very strong, bold noise grain.
  //xxx - TODO: Experiment with using noise.rgba.
  values += noise.r * u.noise_str * 0.022;


	let decay = exposure.decay;
	values *= decay;	
  
  return clamp(values, vec4f(0.0), vec4f(1.0));
}`;

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

export const kAutoExposureWGSL = kUniforms + 
`
@group(0) @binding(0) var old_tex : texture_2d<f32>;
@group(0) @binding(1) var<uniform> u : WarpUniforms;

struct ExposureOut {
  decay : f32,
  _pad0 : f32,
  _pad1 : f32,
  _pad2 : f32,
};
@group(0) @binding(2) var<storage, read_write> out_exposure : ExposureOut;

// Simple integer hash (good enough for sampling coords)
fn hash_u32(x: u32) -> u32 {
  var v = x;
  v ^= v >> 16u;
  v *= 0x7feb352du;
  v ^= v >> 15u;
  v *= 0x846ca68bu;
  v ^= v >> 16u;
  return v;
}

const kWG: u32 = 256u;
const kSamplesPerThread: u32 = 16u; // 256*16 = 4096 samples total

var<workgroup> wg_sum : array<f32, 256>;

@compute @workgroup_size(256)
fn cs_main(@builtin(local_invocation_id) lid: vec3u) {
  let tid = lid.x;

  let w = u32(u.index_width);
  let h = u32(u.index_height);

  // Seed from tid + per-frame noise offsets (you update these each frame)
  // (note: u.noise_offset_x/y are floats; convert safely)
  let sx_seed = u32(u.noise_offset_x);
  let sy_seed = u32(u.noise_offset_y);
  var seed = hash_u32(tid ^ (sx_seed * 1315423911u) ^ (sy_seed * 2654435761u));

  var sum: f32 = 0.0;

  // Strided random samples
  for (var i: u32 = 0u; i < kSamplesPerThread; i++) {
    seed = hash_u32(seed + 0x9e3779b9u);

    // Derive coords from seed
    let x = (seed      ) % w;
    let y = (seed >> 16) % h;

    let c = textureLoad(old_tex, vec2u(x, y), 0);
    let values = c.rg;
    //TODO: Revisit whether c1 and c2 should be added vs averaged.
    //      If you change this, be sure to also change it in webgpu_present.js.
    sum += (values.x + values.y) * 0.5;
  }

  wg_sum[tid] = sum;
  workgroupBarrier();

  // Reduce within workgroup
  var stride = 128u;
  while (stride > 0u) {
    if (tid < stride) {
      wg_sum[tid] += wg_sum[tid + stride];
    }
    workgroupBarrier();
    stride >>= 1u;
  }

  // Thread 0 writes final average and converts to decay
  if (tid == 0u) {
    let total_samples = f32(kWG * kSamplesPerThread);
    let avg = wg_sum[0] / total_samples; // normalized R/G average

		// DECAY TUNING:
		
    let avg8 = avg * 255.0;    		// [0..255]
		// Tune this first, to the decay you want when the image is already dark.
		const base_decay = 0.997;//1.0;
		const fast_decay = 0.990;
		let exp_base = select(base_decay, fast_decay, u.darkening >= 0);
		let adj_base_decay = base_decay * pow(exp_base, u.darkening);
					
		let min_level_for_extra_decay = 30 * (1 - u.darkening);//1.2;  	// [0..255]
		// Lower divisor -> quicker decay to black *of very bright stuff* over time
		const extra_decay_strength = 0.000075;//0.000155
		let prev_avg_clamped = max(avg8 - min_level_for_extra_decay, 0.0);
		let decay = max(0.0, adj_base_decay - (prev_avg_clamped * extra_decay_strength));        
    
    out_exposure.decay = decay;
  }
}
`;

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

// Fast path (native Float16Array) when available; fallback to JS bit-pack.
// Returns Uint16Array of float16 bit patterns (len = input.length).
const _f32_scratch = new Float32Array(1);
const _u32_scratch = new Uint32Array(_f32_scratch.buffer);

// Scalar fallback: no allocations, reused scratch views.
function float32_to_float16_bits_fast_fallback(val) {
  _f32_scratch[0] = val;
  const x = _u32_scratch[0];

  const sign = (x >>> 16) & 0x8000;
  let mant = x & 0x007fffff;
  let exp  = (x >>> 23) & 0xff;

  // NaN/Inf
  if (exp === 0xff) {
    if (mant !== 0) return sign | 0x7e00; // canonical NaN
    return sign | 0x7c00;                 // Inf
  }

  // Re-bias exponent from f32 (127) to f16 (15)
  exp = exp - 127 + 15;

  // Subnormal / underflow
  if (exp <= 0) {
    if (exp < -10) return sign; // too small => +/-0
    mant = (mant | 0x00800000) >>> (1 - exp);
    // round to nearest
    if (mant & 0x00001000) mant += 0x00002000;
    return sign | (mant >>> 13);
  }

  // Overflow => Inf
  if (exp >= 0x1f) return sign | 0x7c00;

  // Normalized: round mantissa
  if (mant & 0x00001000) {
    mant += 0x00002000;
    if (mant & 0x00800000) {
      mant = 0;
      exp += 1;
      if (exp >= 0x1f) return sign | 0x7c00;
    }
  }

  return sign | (exp << 10) | (mant >>> 13);
}

export function pack_f32_to_rg16f(f32_xy_interleaved) {
  const n = f32_xy_interleaved.length;

  // Fast path: Chrome / browsers that have Float16Array
  if (typeof Float16Array !== "undefined") {
    const f16 = new Float16Array(n);
    f16.set(f32_xy_interleaved);
    // Copy out the bits so caller gets an owning Uint16Array
    return new Uint16Array(f16.buffer, f16.byteOffset, n).slice();
  }

  // Fallback: Safari
  console.log("WARNING: Native Float16Array not supported by browser; float16 uploads will be slow and might cause brief pauses.");
  const out = new Uint16Array(n);
  for (let i = 0; i < n; i++) {
    out[i] = float32_to_float16_bits_fast_fallback(f32_xy_interleaved[i]);
  }
  return out;
}

export class GPUWarp {
  constructor(device, width, height, shader_code_wgsl) {
    this.device = device;
    this.W = width;
    this.H = height;

    this.shader_code = shader_code_wgsl;

    this.index_tex = [null, null];
    this.index_view = [null, null];
    this.ping = 0; // old = ping, new = ping^1

    this.sampler = null;
    this.index_sampler = null;
    this.uniform_buf = null;
    this.bind_group = null;
    this.pipeline = null;
    
		this.dots = null;    

		this.noise = null;          // { textures, views, ... }
		this.bind_groups = null;    // 2D: [2][kNoiseTexCount]
		this.frame_index = 0;  

		this.exposure_buf = null;
		
		this.autoexp_pipeline = null;
		this.autoexp_bind_groups = [null, null]; // one per old_i (ping)

		this.text_overlay = null;
    this.pending_text_burn = false;
    this.pending_text_burn_center_x = 0;
    this.pending_text_burn_center_y = 0;
    this.pending_text_burn_intensity = 1.0;

    //this.warp_tex = new Array(kGpuWarpSlots).fill(null);
    //this.warp_view = new Array(kGpuWarpSlots).fill(null);
    //this.warp_map = new Array(kGpuWarpSlots).fill(null);

    this.warp_cache = Array.from(
      { length: kGpuWarpMapCacheSize },
      () => ({
        texture: null,
        view: null,
        key: null,
        last_used: 0,
      })
    );
    
    this.warp_slot_cache = new Array(kGpuWarpSlots).fill(-1);
    this.warp_view = new Array(kGpuWarpSlots).fill(null);
    this.warp_cache_clock = 0;
    this.warp_bind_groups_dirty = false;
    
    this.dynamic_warp = new GpuDynamicWarp(this.device);
    this.true_time = 0;
    this.fading_dots = null;
    // Static arrays are weakly keyed; dynamic params live in their bounded cache entries.
    this.warp_map_keys = new WeakMap();    
  }
	
	resize(iw, ih) {
		if (!this.pipeline) {
		  // Not initialized yet; we'll build pipelines/bindgroups in init().
		  return;
		}
		
	  iw |= 0;
	  ih |= 0;
	  if (iw <= 0 || ih <= 0) {
	    throw new Error(`GPUWarp.resize invalid size: ${iw}x${ih}`);
	  }
	
	  // No-op if unchanged
	  if (this.W === iw && this.H === ih && this.index_tex[0] && this.index_tex[1]) {
	    return;
	  }
	
	  // 1) Destroy old index textures (warp textures stay as-is)
	  for (let k = 0; k < 2; k++) {
	    if (this.index_tex[k]) {
	      try { this.index_tex[k].destroy(); } catch (_) {}
	      this.index_tex[k] = null;
	      this.index_view[k] = null;
	    }
	  }
	
	  // 2) Update dims + reset ping so "old" is tex0 again
	  this.W = iw;
	  this.H = ih;
	  this.ping = 0;
	
	  // 3) Recreate index ping-pong textures/views
	  const d = this.device;
	  for (let k = 0; k < 2; k++) {
	    this.index_tex[k] = d.createTexture({
	      size: { width: this.W, height: this.H },
		      format: "rgba16float",
	      usage:
	        GPUTextureUsage.TEXTURE_BINDING |
	        GPUTextureUsage.RENDER_ATTACHMENT |
	        GPUTextureUsage.COPY_DST |
	        GPUTextureUsage.COPY_SRC,
	    });
	    this.index_view[k] = this.index_tex[k].createView();
	  }
	
		this.rebuild_autoexp_bind_groups();
	  this.rebuild_bind_groups_all();

	
	  // 6) Fill with fresh noise, like init-time behavior
	  // (Uses upload_index_u8 internally.)
	  // TODO: Remove this.
	  this.fill_index_u8_with_noise();	  
	}
	
  init() {
    const d = this.device;

    // Index ping-pong textures: rgba16float, renderable + sampleable.
    for (let k = 0; k < 2; k++) {
      this.index_tex[k] = d.createTexture({
        size: { width: this.W, height: this.H },
        format: "rgba16float",
        usage: GPUTextureUsage.TEXTURE_BINDING |
               GPUTextureUsage.RENDER_ATTACHMENT |
               GPUTextureUsage.COPY_DST |
               GPUTextureUsage.COPY_SRC,
      });
      this.index_view[k] = this.index_tex[k].createView();
    }

    // Warp textures: rg16float, sampleable + uploadable + renderable.
    for (const entry of this.warp_cache) {
      entry.texture = d.createTexture({
        size: { width: kWarpMapSize, height: kWarpMapSize },
        format: "rg16float",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST |
               GPUTextureUsage.RENDER_ATTACHMENT,
      });
      entry.view = entry.texture.createView();
    }
    
    // Every shader binding needs a valid view, including unused slots.
    this.warp_view.fill(this.warp_cache[0].view);
    // Filtering sampler for warp maps (bilinear).
    this.sampler = d.createSampler({
      addressModeU: "clamp-to-edge",
      addressModeV: "clamp-to-edge",
      magFilter: "linear",
      minFilter: "linear",
      mipmapFilter: "nearest",
    });

    // Feedback sampling wraps; warp-map sampling keeps clamp-to-edge.
    this.index_sampler = d.createSampler({
      addressModeU: "repeat",
      addressModeV: "repeat",
      magFilter: "linear",
      minFilter: "linear",
      mipmapFilter: "nearest",
    });

    // Uniforms (16-byte aligned). We'll use 12 floats = 48 bytes, but align to 64.
    this.uniform_buf = d.createBuffer({
      size: 256,//80,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const module = d.createShaderModule({ code: this.shader_code });

    this.pipeline = d.createRenderPipeline({
      layout: "auto",
      vertex: { module, entryPoint: "vs_main" },
      fragment: {
        module,
        entryPoint: "fs_main",
        // No blending: all four components are independent channel data.
        targets: [{ format: "rgba16float", writeMask: GPUColorWrite.ALL }],
      },
      primitive: { topology: "triangle-list" },
    });



		// 16 bytes (4 floats) to keep it aligned and match ExposureOut
		this.exposure_buf = d.createBuffer({
		  size: 16,
		  usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
		});
		
		const ae_module = d.createShaderModule({ code: kAutoExposureWGSL });
		this.autoexp_pipeline = d.createComputePipeline({
		  layout: "auto",
		  compute: { module: ae_module, entryPoint: "cs_main" },
		});
		
		this.rebuild_autoexp_bind_groups();

		
		// Set up dot renderer
		this.dots = new DotRenderer(d, "rgba16float");
		this.dots.init(32768);
    this.fading_dots = new GpuFadingDots(d, kUniforms + kWarpFieldWGSL);

		
		// Create noise textures (one-time random fill inside)
		this.noise = create_noise_textures(d); // <-- adjust signature if needed (see note below)

    this.text_overlay = new GpuTextOverlay(d, "rgba16float");
    this.text_overlay.init();
    		
		this.rebuild_bind_groups_all();	
	}
	
	rebuild_autoexp_bind_groups() {
	  if (!this.autoexp_pipeline) return;          // not initialized yet
	  if (!this.exposure_buf) return;
	  if (!this.index_view[0] || !this.index_view[1]) return;
	
	  const layout0 = this.autoexp_pipeline.getBindGroupLayout(0);
	
	  for (let old_i = 0; old_i < 2; old_i++) {
	    this.autoexp_bind_groups[old_i] = this.device.createBindGroup({
	      layout: layout0,
	      entries: [
	        { binding: 0, resource: this.index_view[old_i] },
	        { binding: 1, resource: { buffer: this.uniform_buf } },
	        { binding: 2, resource: { buffer: this.exposure_buf } },
	      ],
	    });
	  }
	}
		
	rebuild_bind_groups_all() {
	  const layout0 = this.pipeline.getBindGroupLayout(0);
	
	  const tex_count = this.noise.views.length; // should be kNoiseTexCount
	  this.bind_groups = [new Array(tex_count), new Array(tex_count)];
	
	  for (let old_i = 0; old_i < 2; old_i++) {
	    for (let ni = 0; ni < tex_count; ni++) {
	      this.bind_groups[old_i][ni] = this.device.createBindGroup({
	        layout: layout0,
	        entries: [
	          { binding: 0, resource: this.index_view[old_i] },
	          { binding: 1, resource: this.warp_view[0] },
	          { binding: 2, resource: this.warp_view[1] },
	          { binding: 3, resource: this.warp_view[2] },
	          { binding: 4, resource: this.warp_view[3] },
	          { binding: 5, resource: this.warp_view[4] },
	          { binding: 6, resource: this.warp_view[5] },
	          { binding: 7, resource: this.warp_view[6] },
	          { binding: 8, resource: this.warp_view[7] },
	          { binding: 9, resource: this.sampler },
	          { binding: 10, resource: { buffer: this.uniform_buf } },
	          { binding: 11, resource: this.noise.views[ni] },
          	{ binding: 12, resource: { buffer: this.exposure_buf } },
	          { binding: 13, resource: this.index_sampler },
	        ],
	      });
	    }
	  }
	}

  // Keep the existing one-byte-per-pixel initialization API. Seed every channel.
  upload_index_u8(index_u8) {
    const count = this.W * this.H;
    if (!index_u8 || index_u8.length < count) {
      throw new Error("upload_index_u8: index_u8 length < W*H");
    }
    const values = new Float32Array(count * 4);
    for (let i = 0; i < count; i++) {
      const value = index_u8[i] / 255.0;
      values.fill(value, i * 4, i * 4 + 4);
    }
    // queue.writeTexture accepts tightly packed rows (no 256-byte alignment needed).
    const half_bits = pack_f32_to_rg16f(values);
    for (const tex of this.index_tex) {
      this.device.queue.writeTexture(
        { texture: tex }, half_bits,
        { bytesPerRow: this.W * 8, rowsPerImage: this.H },
        { width: this.W, height: this.H }
      );
    }
  }

  fill_index_u8_with_noise() {
  	let noise = new Uint8Array(this.W * this.H);
  	for (let i = 0; i < this.W * this.H; i++) {
  		noise[i] = (Math.random() * 40) | 0;
  	}
  	this.upload_index_u8(noise);
  }

  // Bind a full map descriptor, or a legacy Float32Array XYXY...
  // Static maps upload on cache miss; dynamic entries render before each step.
  upload_warp_map(slot, warp_map) {
    const dynamic_params = warp_map?.dynamic_motion_params ?? null;
    const warp_f32_xy = warp_map?.src_dxy ?? (ArrayBuffer.isView(warp_map) ? warp_map : null);
    const source = dynamic_params ?? warp_f32_xy;
    if (source == null) throw new Error("Cannot bind a pruned warp map");
    if (!Number.isInteger(slot) || slot < 0 || slot >= kGpuWarpSlots) {
      throw new RangeError(`Invalid warp slot: ${slot}`);
    }
  
    let key = this.warp_map_keys.get(source);
    if (key === undefined) {
      key = {}; // Unique identity token.
      this.warp_map_keys.set(source, key);
    }
  
    let cache_i = this.warp_cache.findIndex(e => e.key === key);
  
    if (cache_i === -1) {
      // Prefer an empty entry.
      cache_i = this.warp_cache.findIndex(e => e.key === null);
  
      if (cache_i === -1) {
        // Otherwise evict the oldest entry not assigned to a shader slot.
        let oldest = Infinity;
  
        for (let i = 0; i < this.warp_cache.length; i++) {
          if (this.warp_slot_cache.includes(i)) continue;
  
          if (this.warp_cache[i].last_used < oldest) {
            oldest = this.warp_cache[i].last_used;
            cache_i = i;
          }
        }
      }
  
      if (cache_i === -1) {
        throw new Error("Warp cache has no evictable entry");
      }
  
      const entry = this.warp_cache[cache_i];
      // Cache resources belong to instances, never to a mode number.
      entry.dynamic_state?.uniform_buf.destroy();
      entry.dynamic_state = null;
      if (dynamic_params != null) {
        entry.dynamic_state = this.dynamic_warp.create_state(warp_map.mode, dynamic_params);
      } else {
        const packed_u16 = pack_f32_to_rg16f(warp_f32_xy);
  
        if (g_verbose) {
          console.log(`Uploading new warp map to the GPU.`);
        }
        this.device.queue.writeTexture(
          { texture: entry.texture },
          packed_u16,
          {
            bytesPerRow: kWarpMapSize * 4,
            rowsPerImage: kWarpMapSize,
          },
          { width: kWarpMapSize, height: kWarpMapSize }
        );
  
      }
      entry.key = key;
    }
  
    const entry = this.warp_cache[cache_i];
    entry.last_used = ++this.warp_cache_clock;
  
    this.warp_slot_cache[slot] = cache_i;
  
    if (this.warp_view[slot] !== entry.view) {
      this.warp_view[slot] = entry.view;
      this.warp_bind_groups_dirty = true;
    }
  }
 
  set_fading_dots(params) {
    this.fading_dots.set_params(params);
  }

  // Update uniform parameters.
  set_params({
    w0 = 1, w1 = 0, w2 = 0, w3 = 0,
    w4 = 0, w5 = 0, w6 = 0, w7 = 0,
    shift_x = 0, shift_y = 0,
    darkening = 0.0,
    true_time = 0.0,
    shader_params = null,
    oversample = 1,
    warp_width = kWarpMapSize,
    warp_height = kWarpMapSize,
    image_width = this.W,
    image_height = this.H,
  } = {}) {
    this.true_time = true_time;
		let xform = new TransformHelper(image_width, image_height);
  	
  	const image_width_inv = 1.0 / image_width;
  	const image_height_inv = 1.0 / image_height;
  	
  	const noise_offset_x = (Math.random() * (kNoiseTexSize - 1)) | 0;
  	const noise_offset_y = (Math.random() * (kNoiseTexSize - 1)) | 0;
  	
  	// Uniforms:
    const data = new Float32Array([
      w0, w1, w2, w3,
      //------------
      w4, w5, w6, w7,
      //------------
      shift_x, 
      shift_y,
      warp_width, 
      warp_height,
      //------------
      image_width, 
      image_height,
      image_width_inv, 
      image_height_inv,
      //------------
      xform.x0x1,
      xform.y0y1,
      xform.inv_x0x1,
      xform.inv_y0y1,
      //------------
      noise_offset_x,
      noise_offset_y,
      darkening,
      true_time,
      //------------
      shader_params.rx_diff_grey_scott_str,
      shader_params.rx_diff_grey_scott_str2,
      shader_params.diffusion_colorization,
      shader_params.rx_diff_barkley_str,
      //------------
      shader_params.frequency_injector_str,
      shader_params.frequency_injector_freq_scale,
      shader_params.painterly_str,
      shader_params.sharpen_str,
      //------------
      shader_params.noise_str,
      oversample,
      1.0 / oversample,
      shader_params.layer2_motion_scale,
      //------------
      // NOTE: If you don't have a multiple of 4 values here, add more for padding.
    ]);
    this.device.queue.writeBuffer(this.uniform_buf, 0, data);
  }

  // Run one warp step: old -> new, then swap.
  // Draws the waveform after the warp.  Should be an interleaved
  //   Float32Array() of the form [x,y,a, x,y,a ...] 
  //   with XY as pixel coordinates.
  step(waveform) {
    const d = this.device;
    const old_i = this.ping;
    const new_i = this.ping ^ 1;

    if (this.warp_bind_groups_dirty) {
      this.rebuild_bind_groups_all();
      this.warp_bind_groups_dirty = false;
    }

		//console.log(float32_to_float16_bits(0.0).toString(16)); // should be "0"
		//console.log(float32_to_float16_bits(1.0).toString(16)); // should be "3c00"
		//console.log(float32_to_float16_bits(-1.0).toString(16)); // should be "bc00"

    const encoder = d.createCommandEncoder();
    // A dummy slot can reference the same instance; render that instance once.
    for (const cache_i of new Set(this.warp_slot_cache)) {
      if (cache_i < 0) continue;
      const entry = this.warp_cache[cache_i];
      if (entry.dynamic_state) {
        this.dynamic_warp.encode(encoder, entry.view, entry.dynamic_state, this.true_time);
      }
    }
    
    // Compute writes point positions and draw instances; rendering reads them
    // later in this command buffer. No readback or CPU/GPU wait is involved.
    this.fading_dots.encode_update(encoder, this.warp_view, this.sampler,
      this.uniform_buf, this.W, this.H);

    // --- Autoexposure compute pass (old_i -> exposure_buf)
		{
		  const cpass = encoder.beginComputePass();
		  cpass.setPipeline(this.autoexp_pipeline);
		  cpass.setBindGroup(0, this.autoexp_bind_groups[old_i]);
		  cpass.dispatchWorkgroups(1);
		  cpass.end();
		}
		
    const pass = encoder.beginRenderPass({
      colorAttachments: [{
        view: this.index_view[new_i],
        clearValue: { r: 0, g: 0, b: 0, a: 0 },
        loadOp: "load",   // or "clear" if you prefer
        storeOp: "store",
      }],
    });

		const noise_i = get_noise_index_for_frame(this.frame_index++);

    pass.setPipeline(this.pipeline);
		pass.setBindGroup(0, this.bind_groups[old_i][noise_i]);
    pass.draw(3);
    pass.end();


	  // 2) Dots: draw into the warped buffer (new_i)
    this.dots.set_uniforms(this.W, this.H);
	  if (waveform && waveform.length) {
	    this.dots.upload_points(waveform);
	    this.dots.draw(encoder, this.index_view[new_i]);
	  }

    this.fading_dots.draw(encoder, this.index_view[new_i], this.dots);

    // 3) Optional one-frame burn into the warped buffer (new_i)
    if (this.pending_text_burn && this.text_overlay) {
      this.text_overlay.draw(
        encoder,
        this.index_view[new_i],
        this.W,
        this.H,
        this.pending_text_burn_center_x,
        this.pending_text_burn_center_y,
        this.pending_text_burn_intensity
      );

      // After the burn frame, stop presenter from showing it and clear the text.
      this.text_overlay.clear_text();
      this.pending_text_burn = false;
    }
    
    d.queue.submit([encoder.finish()]);

    // Swap
    this.ping = new_i;
  }

  // Get current (latest) index texture view for presenting.
  get_current_index_view() {
    return this.index_view[this.ping];
  }

  // If your presenter wants the texture itself:
  get_current_index_texture() {
    return this.index_tex[this.ping];
  }


	/*
	// Intended usage pattern for text overlay stuff:
	
	// Rarely: rebuild/upload bitmap
	gpu_warp.set_overlay_text("HELLO WORLD", 96, {
	  font_family: "Arial",
	  font_weight: "bold",
	  supersample: 2,
	  padding_px: 4,
	});
	
	// Whenever you want it stamped on the next frame:
	gpu_warp.queue_overlay_text(gpu_warp.W * 0.5, gpu_warp.H * 0.5, 0.75);
	
	// Then your normal render/update path:
	gpu_warp.step(waveform);
	*/
  set_overlay_text(text, font_px, true_time, {
    font_family = "sans-serif",
    font_weight = "bold",
    supersample = 2,
    padding_px = 4,
    center_x = this.W * 0.5,
    center_y = this.H * 0.5,
    intensity = 1.0,
    duration = 2.0,
    fade_in_power = 0.5,
  } = {}) {
    if (!this.text_overlay) return;

    this.text_overlay.set_text(text, font_px, {
      font_family,
      font_weight,
      supersample,
      padding_px,
    });

		const time_now = true_time;//performance.now() * 0.001;
		const t0 = time_now;
		const t1 = time_now + duration;
    this.text_overlay.show_overlay(center_x, center_y, intensity, t0, t1, fade_in_power);
  }

  clear_overlay_text() {
    if (!this.text_overlay) return;
    this.text_overlay.clear_text();
    this.pending_text_burn = false;
  }

  hide_overlay_text() {
    if (!this.text_overlay) return;
    this.text_overlay.hide_overlay();
  }

  show_overlay_text(center_x, center_y, intensity = 1.0) {
    if (!this.text_overlay) return;
    this.text_overlay.show_overlay(center_x, center_y, intensity);
  }

  burn_overlay_text(true_time, center_x = null, center_y = null, intensity = null) {
    if (!this.text_overlay) return;

    const rect = this.text_overlay.get_presenter_rect(true_time);
    if (!rect.enabled) return;

    this.pending_text_burn = true;
    this.pending_text_burn_center_x = (center_x ?? (rect.left_px + rect.width_px * 0.5));
    this.pending_text_burn_center_y = (center_y ?? (rect.top_px  + rect.height_px * 0.5));
    this.pending_text_burn_intensity = (intensity ?? rect.intensity);
  }

  get_overlay_texture_view() {
    return this.text_overlay ? this.text_overlay.get_presenter_texture_view() : null;
  }

  get_overlay_sampler() {
    return this.text_overlay ? this.text_overlay.get_sampler() : null;
  }

  get_overlay_rect(true_time) {
    if (!this.text_overlay) {
      return {
        enabled: 0,
        left_px: 0,
        top_px: 0,
        width_px: 0,
        height_px: 0,
        intensity: 0,
      };
    }
    return this.text_overlay.get_presenter_rect(true_time);
  }
}