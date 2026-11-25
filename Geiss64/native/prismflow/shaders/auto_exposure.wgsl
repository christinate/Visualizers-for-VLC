// Extracted for Prism Flow native Dawn; shader body unchanged.
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
struct WarpUniforms {
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

