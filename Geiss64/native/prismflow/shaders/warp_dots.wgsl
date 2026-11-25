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

struct WarpDotControls {
  true_time: f32,
  strength: f32,
  point_size: f32,
  reset: f32,
  resize_x: f32,
  resize_y: f32,
  random_seed: u32,
  padding: u32,
};

@group(0) @binding(0) var<storage, read_write> positions: array<vec2f>;
@group(0) @binding(1) var<storage, read_write> instances: array<vec4f>;
@group(0) @binding(2) var<uniform> controls: WarpDotControls;
@group(0) @binding(3) var<uniform> u: WarpUniforms;
@group(0) @binding(4) var warp0: texture_2d<f32>;
@group(0) @binding(5) var warp1: texture_2d<f32>;
@group(0) @binding(6) var warp2: texture_2d<f32>;
@group(0) @binding(7) var warp3: texture_2d<f32>;
@group(0) @binding(8) var warp4: texture_2d<f32>;
@group(0) @binding(9) var warp5: texture_2d<f32>;
@group(0) @binding(10) var warp6: texture_2d<f32>;
@group(0) @binding(11) var warp7: texture_2d<f32>;
@group(0) @binding(12) var samp: sampler;

fn hash_u32(value: u32) -> u32 {
  var x = value;
  x = (x ^ (x >> 16u)) * 0x7feb352du;
  x = (x ^ (x >> 15u)) * 0x846ca68bu;
  return x ^ (x >> 16u);
}

fn random_position(id: u32) -> vec2f {
  let seed = hash_u32(id ^ controls.random_seed);
  let rx = f32(hash_u32(seed ^ 0xa511e9b3u) >> 8u) * (1.0 / 16777216.0);
  let ry = f32(hash_u32(seed ^ 0x63d83595u) >> 8u) * (1.0 / 16777216.0);
  return vec2f(rx * u.index_width, ry * u.index_height);
}

@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) global_id: vec3u) {
  let id = global_id.x;
  if (id >= 1024u) { return; }

  let phase = f32(id) * 6.28318530718 / f32(1024);
  let strength = clamp(0.5 + 0.6 * cos(controls.true_time * 6.5 + phase), 0.0, 1.0)
               * controls.strength * 0.6 * 0.5;

  var position = positions[id] * vec2f(controls.resize_x, controls.resize_y);
  if (controls.reset > 0.5 || strength < 0.0001) {
    position = random_position(id);
  }
  if (strength < 0.0001) {
    // Same lifecycle as the CPU implementation: reposition while invisible.
    positions[id] = position;
    instances[id] = vec4f(position, controls.point_size, 0.0);
    return;
  }

  // Invert the backward sampling field: find the destination whose source is
  // the old dot position. Four fixed-point iterations match AdvectPoint().
  // DotRenderer treats integers as pixel centers; the warp shader uses +0.5.
  let source_pixel = position + vec2f(0.5);
  var destination_pixel = source_pixel;
  for (var iteration = 0; iteration < 4; iteration++) {
    destination_pixel = source_pixel - vec2f(u.shift_x, u.shift_y)
                      - warp_displacement_pixels(destination_pixel,
                                                 vec4f(1.0),
                                                 vec4f(1.0));
  }
  position = destination_pixel - vec2f(0.5);
  positions[id] = position; // Keep full precision for the next frame.
  // Preserve the old integer-centered drawing, without rounding the state.
  instances[id] = vec4f(trunc(position), controls.point_size, strength);
}

