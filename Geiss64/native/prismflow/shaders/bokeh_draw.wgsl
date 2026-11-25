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

struct BokehDot {
  position: vec4f, // screen UV, z, alive
  color_life: vec4f, // palette position, use background, birth time, fade-in
  lifetime: vec4f, // hold, fade-out, original z, target z
};
fn bokeh_depth(p: BokehDot, now: f32) -> f32 {
  let duration = p.color_life.w + p.lifetime.x + p.lifetime.y;
  let t = clamp(
    (now - p.color_life.z) / max(duration, 0.000001),
    0.0, 1.0
  );
  return mix(p.lifetime.z, p.lifetime.w, t);
}
fn bokeh_envelope(p: BokehDot, now: f32) -> f32 {
  let age = max(0.0, now - p.color_life.z);
  let fade_in = p.color_life.w;
  let hold_end = fade_in + p.lifetime.x;
  if (age < fade_in) { return age / max(fade_in, 0.000001); }
  if (age < hold_end) { return 1.0; }
  return clamp(1.0 - (age - hold_end) / max(p.lifetime.y, 0.000001), 0.0, 1.0);
}
fn bokeh_inner_radius(z: f32, focus: f32) -> f32 {
  //return 0.5 + 24 * abs(z - focus);

  let camera_z = f32(-1);
  let distance = max(z - camera_z, 0.0001);
  let focal_distance = max(focus - camera_z, 0.0001);
  let perspective_scale = focal_distance / distance;

  return 0.5
       + 24 * abs(z - focus) * perspective_scale;  
}

struct BokehDrawControls {
  frame: vec4f, // output width, output height, focal distance, true_time
  background: vec4f,   // .rgb = background color; .w = particle draw opacity
};
@group(0) @binding(0) var<storage, read> bokeh_dots: array<BokehDot>;
@group(0) @binding(1) var<uniform> bokeh_draw: BokehDrawControls;
@group(0) @binding(2) var<uniform> bokeh_palette: array<vec4f, 256>;
struct BokehVertex {
  @builtin(position) position: vec4f,
  @location(0) offset: vec2f,
  @location(1) @interpolate(flat) inner_radius: f32,
  @location(2) @interpolate(flat) color: vec3f,
};
@vertex
fn vs_bokeh(@builtin(vertex_index) vid: u32, @builtin(instance_index) id: u32) -> BokehVertex {
  var out: BokehVertex;
  let p = bokeh_dots[id];
  let envelope = bokeh_envelope(p, bokeh_draw.frame.w) * p.position.w;
  if (envelope <= 0.0) {
    out.position = vec4f(2.0, 2.0, 0.0, 1.0);
    out.offset = vec2f(0.0);
    out.inner_radius = 0.0;
    out.color = vec3f(0.0);
    return out;
  }
  let corners = array<vec2f, 4>(vec2f(-1.0,-1.0), vec2f(1.0,-1.0),
                               vec2f(-1.0,1.0), vec2f(1.0,1.0));
  let z = bokeh_depth(p, bokeh_draw.frame.w);

  //let r = bokeh_inner_radius(z, bokeh_draw.frame.z);
  let size_scale = max(bokeh_draw.frame.x, bokeh_draw.frame.y) * (1.0 / f32(1080));
  let r = bokeh_inner_radius(z, bokeh_draw.frame.z) * size_scale;

  // Half-pixel padding lets edge pixels integrate their covered area.
  let offset = corners[vid] * (r + 0.5 + 0.5);
  let pixel = p.position.xy * bokeh_draw.frame.xy + offset;
  out.position = vec4f(pixel / bokeh_draw.frame.xy * vec2f(2.0,-2.0)
                      + vec2f(-1.0,1.0), 0.0, 1.0);
  out.offset = offset;
  out.inner_radius = r;
  let palette_pos = clamp(p.color_life.x, 0.0, 1.0) * 255.0;
  let index = u32(palette_pos);
  var color = mix(bokeh_palette[index].rgb, bokeh_palette[min(index + 1u, 255u)].rgb,
                  fract(palette_pos));
  if (p.color_life.y > 0.5) { color = bokeh_draw.background.rgb; }
  // Integral of circular coverage with a smoothstep edge:
  // pi * (r*r + r*edge + 0.3*edge*edge). Pi cancels in the ratio.
  let edge = 0.5;
  let min_r = 0.5 * size_scale;
  let focused_area = min_r*min_r + min_r*edge + 0.3*edge*edge;
  let area = r*r + r*edge + 0.3*edge*edge;
  out.color = 
      color * (0.9359999999999999 * envelope * focused_area / area) * 
      bokeh_draw.background.w;  // ~opacity
  return out;
}
@fragment
fn fs_bokeh(in: BokehVertex) -> @location(0) vec4f {
  let radius = length(in.offset);
  let outer_radius = in.inner_radius + 0.5;
  // Only boundary pixels need subpixel integration. It prevents focused dots
  // flickering as they move between pixel centers and better preserves flux.
  if (radius >= outer_radius + 0.707107) { discard; }
  var coverage = 1.0;
  if (radius > in.inner_radius - 0.707107) {
    coverage = 0.0;
    for (var y = 0; y < 4; y++) {
      for (var x = 0; x < 4; x++) {
        let sample_offset = (vec2f(f32(x), f32(y)) + vec2f(0.5)) * 0.25 - vec2f(0.5);
        coverage += 1.0 - smoothstep(in.inner_radius, outer_radius,
                                    length(in.offset + sample_offset));
      }
    }
    coverage *= 1.0 / 16.0;
  }
  return vec4f(in.color * coverage, 0.0);
}

