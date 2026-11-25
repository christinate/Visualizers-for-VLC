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

struct DynamicParams {
  c01: f32,
  c02: f32,
  c03: f32,
  c04: f32,
  c05: f32,
  c06: f32,
  c07: f32,
  c08: f32,
  c09: f32,
  c10: f32,
  c11: f32,
  c12: f32,
  c13: f32,
  c14: f32,
  c15: f32,
  c16: f32,
  true_time: f32,
  width: f32,
  height: f32,
  k_static_noise_3d_size: f32,
  k_static_noise_3d_slices: f32,
  k_static_noise_2d_size: f32,
  k_dynamic_noise_2d_size: f32,
  padding0: f32,
};
@group(0) @binding(0) var<uniform> params: DynamicParams;
@group(0) @binding(1) var static_noise_3d: texture_3d<f32>;
@group(0) @binding(2) var static_noise_2d: texture_2d<f32>;
@group(0) @binding(3) var dynamic_noise_2d: texture_2d<f32>;
@group(0) @binding(4) var noise_sampler: sampler;
@group(0) @binding(5) var audio_spectrum: texture_2d<f32>;
@group(0) @binding(6) var spectrum_sampler: sampler;

// t=0 is the first spectrum bin, t=1 the last; interpolate between bins.
// Values retain the caller's normalization/log scaling (no extra gain here).
fn sample_audio_spectrum(t: f32) -> f32 {
  let size = f32(textureDimensions(audio_spectrum).x);
  let uv = (clamp(t, 0.0, 1.0) * (size - 1.0) + 0.5) / size;
  return textureSampleLevel(audio_spectrum, spectrum_sampler, vec2f(uv, 0.5), 0.0).r;
}

@vertex fn vs_main(@builtin(vertex_index) vertex_index: u32) -> @builtin(position) vec4f {
  let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(positions[vertex_index], 0.0, 1.0);
}

@fragment fn fs_main(@builtin(position) pixel: vec4f) -> @location(0) vec2f {
  // The consuming warp shader already adds a half-texel offset. Match the
  // static map lattice: texel (i,j) represents (2*i/width-1, 2*j/height-1).
  let pos = (pixel.xy - vec2f(0.5)) * 2.0 / vec2f(params.width, params.height) - vec2f(1.0);
  return dynamic_warp(pos);
}

fn dynamic_warp(pos: vec2f) -> vec2f {
  /*
    // original prototype of ripple: (2 passes)
    let fxy = uv * 2.0 - 1.0;
    let rad = length(fxy);
    let freq = select(21.3, 15.3, i==0);
    let phase = f32(u.true_time) * select(5.182, 4.573, i==0);
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * 0.002;    
    warp_vec1 += normalize(fxy) * displacement;
  */

  let rad = length(pos);
  var ret = vec2f(0.0);
  {
    let freq = 21.3 * pow(1.2, params.c01 + params.c02 - 1);
    let speed = 5.182 * (1.0 - 0.65 * params.c03);
    let phase = params.c04 * 6.28 + f32(params.true_time) * speed;
    let str = 0.002 * (1.0 - 0.5 * pow(params.c05, 2));
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * str;
    ret += normalize(pos + 0.0001) * displacement;
  }
  {
    let freq = 15.3 * pow(1.2, params.c06 + params.c07 - 1);
    let speed = 4.573 * (1.0 - 0.65 * params.c08);
    let phase = params.c09 * 6.28 + f32(params.true_time) * speed;
    let str = 0.002 * (1.0 - 0.5 * pow(params.c10, 2));
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * str;
    ret += normalize(pos + 0.0001) * displacement;
  }

  return ret;
}
