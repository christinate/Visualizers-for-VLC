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
    
  // 3d noise-driven velocity field
// Use this to push random values in [0..1] toward 0.5:
fn push_toward_half(t: f32) -> f32 {
  return t * (3.0 + t * (4.0 * t - 6.0));   // (inverse smoothstep)
}
fn dynamic_warp(pos: vec2f) -> vec2f {
  // Span only a few noise texels across the map for a smooth, coarse field.
  let cells = mix(3.0, 9.0, params.c01);
  let offset = vec3f(params.c02, params.c03, params.c04);
  let anim_speed = 0.4 + 0.5 * pow(params.c05, 2);
  let slice = params.true_time * anim_speed;
  //let str = pow(1.3, (params.c06 + params.c07 + params.c08 + params.c09) * (1.0 / 4.0) * 2.0 - 1.0);
  let str = pow(2.2, push_toward_half(push_toward_half(params.c06)) * 2.0 - 1.0) * 0.004;

  // 3d noise:
  let uvw = offset + vec3f(
      (pos * 0.5 + 0.5) * cells * (1.0 / params.k_static_noise_3d_size),  // XY
      slice * (1.0 / params.k_static_noise_3d_slices));                   // Z
  let velocity = textureSampleLevel(static_noise_3d, noise_sampler, uvw, 0.0).rg * 2.0 - 1.0;

  return velocity * str;
}
