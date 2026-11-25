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

// randomly-aligned sine waves
fn rotate(v: vec2f, theta: f32) -> vec2f {
  // theta is in radians.
  let c = cos(theta);
  let s = sin(theta);
  return vec2f(c * v.x - s * v.y, s * v.x + c * v.y);
}
fn dynamic_warp(pos: vec2f) -> vec2f {

  let rot_ang = params.c15 * 1.57;
  var pos2 = rotate(pos.xy, rot_ang);

  var ret = vec2f(0.0);

  var cycles = mix(1.0, 5.0, params.c02);
  var unjulation_speed = mix(0.3, 1.0, pow(params.c03, 2)) * 0.1;//0.25;
  var amplitude = mix(0.003, 0.012, params.c01) * 0.23;
  {
    let direction = select(-1.0, 1.0, params.c05 >= 0.5);
    let phase = 6.28318530718 * (cycles * pos2.y * 0.5
        - direction * unjulation_speed * params.true_time + params.c04);
    ret += vec2f(amplitude * sin(phase), 0.0);
  }
  {
    //let amplitude = mix(0.003, 0.018, pow(params.c06, 0.25)) * 0.3;
    //let cycles = mix(1.0, 5.0, params.c07);
    //let speed = mix(0.3, 1.4, params.c08) * 0.5;

    amplitude        *= pow(1.2, params.c06);
    unjulation_speed *= pow(1.2, params.c07);
    cycles           *= pow(1.2, params.c08);

    let direction = select(-1.0, 1.0, params.c10 >= 0.5);
    let phase = 6.28318530718 * (cycles * pos2.x * 0.5
        - direction * unjulation_speed * params.true_time + params.c09);
    ret += vec2f(0.0, amplitude * sin(phase));
  }

  ret = rotate(ret, -rot_ang);

  return ret;
}
