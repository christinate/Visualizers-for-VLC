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

struct BokehDeathEvent {
  age: f32, // -1 means no death in this event slot this update
  ticket: u32,
};
struct BokehLifetimeHistory {
  next_ticket: atomic<u32>,
  sample_count: u32,
  processed_ticket: u32,
  write_offset: u32,
  ages: array<f32>,
};

struct BokehUpdateControls {
  frame: vec4f, // true_time, focal_distance, output width, output height
  spawn: vec4u, // spawn count, padding
  cull_bounds: vec4f, // min screen UV (xy), max screen UV (zw)
  follow_weights_0123: vec4f, // Multipliers applied in addition to u.w0..u.w3.
  follow_weights_4567: vec4f, // Multipliers applied in addition to u.w4..u.w7.
};
@group(0) @binding(0) var<storage, read_write> bokeh_dots: array<BokehDot>;
@group(0) @binding(2) var<uniform> bokeh_controls: BokehUpdateControls;
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
@group(0) @binding(13) var<storage, read> bokeh_spawns: array<BokehDot>;
@group(0) @binding(14) var<storage, read_write> bokeh_spawn_cursor: atomic<u32>;
@group(0) @binding(15) var<storage, read_write> bokeh_alive_count: atomic<u32>;
@group(0) @binding(16) var<storage, read_write> bokeh_death_events: array<BokehDeathEvent>;
@group(0) @binding(17) var<storage, read_write> bokeh_history: BokehLifetimeHistory;

fn record_bokeh_death(event_index: u32, p: BokehDot, now: f32) {
  let ticket = atomicAdd(&bokeh_history.next_ticket, 1u);
  bokeh_death_events[event_index] = BokehDeathEvent(max(0.0, now - p.color_life.z), ticket);
}

@compute @workgroup_size(64)
fn cs_bokeh(@builtin(global_invocation_id) gid: vec3u) {
  let id = gid.x;
  if (id >= 4096u) { return; }
  // Two events allow a timeout followed by an immediately culled replacement.
  bokeh_death_events[id * 2u].age = -1.0;
  bokeh_death_events[id * 2u + 1u].age = -1.0;
  var p = bokeh_dots[id];
  let now = bokeh_controls.frame.x;
  p.position.z = bokeh_depth(p, now);  
  if (p.position.w > 0.5) {    
    let end_time = p.color_life.z + p.color_life.w + p.lifetime.x + p.lifetime.y;
    if (now >= end_time) {
      // The fade ended at end_time, even if a paused/stalled frame notices later.
      record_bokeh_death(id * 2u, p, end_time);
      p.position.w = 0.0;
    } else {
      let size = vec2f(u.index_width, u.index_height);
      let source_pixel = p.position.xy * size;
      var destination_pixel = source_pixel;
      // Future depth-dependent motion belongs here; z is p.position.z.
      let motion_scale = 1.0;
      for (var iteration = 0; iteration < 4; iteration++) {
        destination_pixel = source_pixel - motion_scale * (
          vec2f(u.shift_x, u.shift_y) + warp_displacement_pixels(destination_pixel,
            bokeh_controls.follow_weights_0123, bokeh_controls.follow_weights_4567));
      }
      p.position = vec4f(destination_pixel / size, p.position.zw);
    }
  }
  if (p.position.w < 0.5 && bokeh_controls.spawn.x > 0u) {
    let spawn_index = atomicAdd(&bokeh_spawn_cursor, 1u);
    if (spawn_index < bokeh_controls.spawn.x) { p = bokeh_spawns[spawn_index]; }
  }
  if (p.position.w > 0.5) {
    // Also handles particles newly spawned during this update.
    p.position.z = bokeh_depth(p, now);

    let output_size = bokeh_controls.frame.zw;
    let center = p.position.xy * output_size;
    
    //let radius = bokeh_inner_radius(p.position.z, bokeh_controls.frame.y)
    //           + 0.5;
    let size_scale = max(output_size.x, output_size.y) * (1.0 / f32(1080));
    let radius = bokeh_inner_radius(p.position.z, bokeh_controls.frame.y)
               * size_scale + 0.5;

    // Keep dots alive until their complete soft disc clears the expanded
    // normalized-XY box, not merely the visible viewport.
    let min_pixel = bokeh_controls.cull_bounds.xy * output_size;
    let max_pixel = bokeh_controls.cull_bounds.zw * output_size;
    let nearest = clamp(center, min_pixel, max_pixel);
    if (distance(center, nearest) >= radius) {
      record_bokeh_death(id * 2u + 1u, p, now);
      p.position.w = 0.0;
    }
  }
  bokeh_dots[id] = p;
  // Count allocated living particles, including ones still fading in.
  if (p.position.w > 0.5) { atomicAdd(&bokeh_alive_count, 1u); }
}

