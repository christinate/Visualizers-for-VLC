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

struct TextUniforms {
  left_px   : f32,
  top_px    : f32,
  width_px  : f32,
  height_px : f32,

  dst_w     : f32,
  dst_h     : f32,
  intensity : f32,
  _pad0     : f32,
};

@group(0) @binding(0) var text_tex  : texture_2d<f32>;
@group(0) @binding(1) var text_samp : sampler;
@group(0) @binding(2) var<uniform> u : TextUniforms;

struct VSOut {
  @builtin(position) pos : vec4f,
  @location(0) uv        : vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VSOut {
  var corner = array<vec2f, 6>(
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
    vec2f(0.0, 1.0),
    vec2f(0.0, 1.0),
    vec2f(1.0, 0.0),
    vec2f(1.0, 1.0)
  );

  let c = corner[vid];

  let px = u.left_px + c.x * u.width_px;
  let py = u.top_px  + c.y * u.height_px;

  let ndc_x = (px / u.dst_w) * 2.0 - 1.0;
  let ndc_y = 1.0 - (py / u.dst_h) * 2.0;

  var out : VSOut;
  out.pos = vec4f(ndc_x, ndc_y, 0.0, 1.0);
  out.uv = c;
  return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4f {
  let a = textureSampleLevel(text_tex, text_samp, in.uv, 0.0).a;
  let intensity = clamp(a * u.intensity, 0.0, 1.0);
  return vec4f(intensity);
}

