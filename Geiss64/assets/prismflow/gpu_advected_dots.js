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

import { TransformHelper } from "./coords.js";

// Bokeh dimensions are OUTPUT pixels, independent of simulation resolution.
export const kBokehMinInnerRadius = 0.5; // One-pixel-diameter focused core.
export const kBokehApertureScale = 24.0 * 2 * 0.5; // Radius added for |z - focus| == 1.
export const kBokehEdgeFalloff = 0.5;
export const kBokehHdrBrightness = Math.pow(48.0 / kBokehApertureScale, 2.0) * 12.0 * 0.15 * 0.13; // Focused peak multiplier of palette RGB.
export const kBokehDotCapacity = 4096;
// Bounds in the same normalized XY coordinates accepted by add_bokeh_dots().
// Import this constant in engine.js to use the same range when spawning.
export const kMinMaxBokehDotXY = 1.0;
const kBokehDotStride = 48;
const kBokehUpdateUniformBytes = 80; // Five vec4 fields, including follow weights.
const kBokehFollowWeightsOffset = 12; // Float offset after frame, spawn, cull bounds.
// Last N deaths, including both off-screen exits and lifetime expirations.
//const kBokehLifetimeWindowSize = kBokehDotCapacity;
const kBokehLifetimeWindowSize = Math.max(16, Math.floor(kBokehDotCapacity / 4));
const kBokehDeathEventCount = kBokehDotCapacity * 2; // Old death + newborn exit per slot.
const kBokehStatsWorkgroupSize = 256;
const kBokehReferenceScreenSize = 1080;

// Read back at around 30 times per second, while the cached count is queried.
export const kBokehAliveCountReadbackIntervalMs = 30;//200;

// Depth: Dots have z values in [0..1].  Plus we have:
export const kBokehCameraZ = -1.0; // Keep below 0, outside the particle range.
export const kCameraFocalDistance = 0.5;

// GPU-resident warp dots. No point positions are uploaded or read back.
// Shared warp-field WGSL is supplied by GPUWarp; dynamic mode formulas live
// only in their map-generation shaders.
const kWarpDotCount = 1024;
const kWorkgroupSize = 64;

const kWarpDotsWGSL = `
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

@compute @workgroup_size(${kWorkgroupSize})
fn cs_main(@builtin(global_invocation_id) global_id: vec3u) {
  let id = global_id.x;
  if (id >= ${kWarpDotCount}u) { return; }

  let phase = f32(id) * 6.28318530718 / f32(${kWarpDotCount});
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
`;

class GpuWarpDots {
  constructor(device, warp_field_wgsl) {
    this.device = device;
    this.strength = 0;
    this.true_time = 0;
    this.point_size = 1;
    this.initialized = false;
    this.previous_width = 0;
    this.previous_height = 0;
    this.frame = 0;
    this.seed = (Math.random() * 4294967296) >>> 0;
    this.data = new ArrayBuffer(32);
    this.data_f32 = new Float32Array(this.data);
    this.data_u32 = new Uint32Array(this.data);
    this.bind_group = null;
    this.bound_views = [];
    this.bound_sampler = null;
    this.bound_uniform_buf = null;

    this.position_buf = device.createBuffer({
      label: "Persistent warp-dot positions",
      size: kWarpDotCount * 8,
      usage: GPUBufferUsage.STORAGE,
    });
    this.instance_buf = device.createBuffer({
      label: "GPU-generated warp-dot draw instances",
      size: kWarpDotCount * 16,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.VERTEX,
    });
    this.control_buf = device.createBuffer({
      label: "Fading-dot scalar controls",
      size: 32,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const module = device.createShaderModule({
      label: "Fading-dot advection", code: warp_field_wgsl + kWarpDotsWGSL,
    });
    this.pipeline = device.createComputePipeline({
      label: "Fading-dot advection", layout: "auto",
      compute: { module, entryPoint: "cs_main" },
    });
  }

  set_params({ strength = 0, true_time = 0, point_size = 1 } = {}) {
    this.strength = strength;
    this.true_time = true_time;
    this.point_size = point_size;
  }

  encode_update(encoder, warp_views, sampler, warp_uniform_buf, width, height) {
    if (this.strength <= 0.001) return;
    if (!this.bind_group || this.bound_sampler !== sampler ||
        this.bound_uniform_buf !== warp_uniform_buf ||
        warp_views.some((view, i) => view !== this.bound_views[i])) {
      this.bind_group = this.device.createBindGroup({
        layout: this.pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: { buffer: this.position_buf } },
          { binding: 1, resource: { buffer: this.instance_buf } },
          { binding: 2, resource: { buffer: this.control_buf } },
          { binding: 3, resource: { buffer: warp_uniform_buf } },
          ...warp_views.map((view, i) => ({ binding: i + 4, resource: view })),
          { binding: 12, resource: sampler },
        ],
      });
      this.bound_views = warp_views.slice();
      this.bound_sampler = sampler;
      this.bound_uniform_buf = warp_uniform_buf;
    }

    this.data_f32.set([
      this.true_time, this.strength, this.point_size, this.initialized ? 0 : 1,
      this.initialized ? width / this.previous_width : 1,
      this.initialized ? height / this.previous_height : 1,
    ]);
    this.data_u32[6] = (this.seed + this.frame++) >>> 0;
    this.device.queue.writeBuffer(this.control_buf, 0, this.data);

    const pass = encoder.beginComputePass({ label: "Advect warp dots" });
    pass.setPipeline(this.pipeline);
    pass.setBindGroup(0, this.bind_group);
    pass.dispatchWorkgroups(Math.ceil(kWarpDotCount / kWorkgroupSize));
    pass.end();
    this.initialized = true;
    this.previous_width = width;
    this.previous_height = height;
  }

  draw(encoder, target_view, renderer) {
    if (this.strength <= 0.001 || !this.initialized) return;
    // Reuse DotRenderer's shader, blend state and corner geometry unchanged.
    // Only the instance source differs: compute-generated STORAGE | VERTEX.
    const pass = encoder.beginRenderPass({
      label: "Draw GPU warp dots",
      colorAttachments: [{ view: target_view, loadOp: "load", storeOp: "store" }],
    });
    pass.setPipeline(renderer.pipeline);
    pass.setBindGroup(0, renderer.bind_group);
    pass.setVertexBuffer(0, renderer.corner_buf);
    pass.setVertexBuffer(1, this.instance_buf);
    pass.draw(4, kWarpDotCount, 0, 0);
    pass.end();
  }

  destroy() {
    this.position_buf.destroy();
    this.instance_buf.destroy();
    this.control_buf.destroy();
  }
}

const kBokehParticleWGSL = `
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
  //return ${kBokehMinInnerRadius} + ${kBokehApertureScale} * abs(z - focus);

  let camera_z = f32(${kBokehCameraZ});
  let distance = max(z - camera_z, 0.0001);
  let focal_distance = max(focus - camera_z, 0.0001);
  let perspective_scale = focal_distance / distance;

  return ${kBokehMinInnerRadius}
       + ${kBokehApertureScale} * abs(z - focus) * perspective_scale;  
}
`;

const kBokehLifetimeWGSL = `
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
`;

// Intentionally separate from the warp-dot advection shader: tune freely.
const kBokehAdvectionWGSL = kBokehParticleWGSL + kBokehLifetimeWGSL + `
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
${Array.from({length: 8}, (_, i) => `@group(0) @binding(${i + 4}) var warp${i}: texture_2d<f32>;`).join('\n')}
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

@compute @workgroup_size(${kWorkgroupSize})
fn cs_bokeh(@builtin(global_invocation_id) gid: vec3u) {
  let id = gid.x;
  if (id >= ${kBokehDotCapacity}u) { return; }
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
    //           + ${kBokehEdgeFalloff};
    let size_scale = max(output_size.x, output_size.y) * (1.0 / f32(${kBokehReferenceScreenSize}));
    let radius = bokeh_inner_radius(p.position.z, bokeh_controls.frame.y)
               * size_scale + ${kBokehEdgeFalloff};

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
`;

// Scatter only the newest window of events after all death tickets are assigned.
// Direct concurrent ring writes would race when >window deaths occur in a frame.
const kBokehRecordDeathsWGSL = kBokehLifetimeWGSL + `
@group(0) @binding(0) var<storage, read> events: array<BokehDeathEvent>;
@group(0) @binding(1) var<storage, read_write> history: BokehLifetimeHistory;
@compute @workgroup_size(${kBokehStatsWorkgroupSize})
fn cs_record_deaths(@builtin(local_invocation_index) lane: u32) {
  let end_ticket = atomicLoad(&history.next_ticket);
  let start_ticket = history.processed_ticket;
  let start_offset = history.write_offset;
  let deaths_this_update = end_ticket - start_ticket;
  for (var id = lane; id < ${kBokehDeathEventCount}u; id += ${kBokehStatsWorkgroupSize}u) {
    let event = events[id];
    if (event.age >= 0.0 && end_ticket - event.ticket <= ${kBokehLifetimeWindowSize}u) {
      let offset = (start_offset + (event.ticket - start_ticket)) % ${kBokehLifetimeWindowSize}u;
      history.ages[offset] = event.age;
    }
  }
  // One workgroup owns the entire scatter. Finish all ring reads/writes before
  // advancing metadata; unsigned ticket subtraction also handles u32 rollover.
  storageBarrier();
  if (lane == 0u) {
    history.sample_count = min(${kBokehLifetimeWindowSize}u,
                               history.sample_count + deaths_this_update);
    history.processed_ticket = end_ticket;
    history.write_offset = (start_offset + deaths_this_update) % ${kBokehLifetimeWindowSize}u;
  }
}
`;

// Runs only when requesting a stats readback, not every frame.
const kBokehStatsWGSL = kBokehLifetimeWGSL + `
struct BokehStats {
  alive_count: u32,
  lifetime_sample_count: u32,
  lifetime_sum: f32,
  padding: u32,
};
@group(0) @binding(0) var<storage, read_write> history: BokehLifetimeHistory;
@group(0) @binding(1) var<storage, read_write> alive_count: atomic<u32>;
@group(0) @binding(2) var<storage, read_write> stats: BokehStats;
var<workgroup> partial_sums: array<f32, ${kBokehStatsWorkgroupSize}>;
@compute @workgroup_size(${kBokehStatsWorkgroupSize})
fn cs_bokeh_stats(@builtin(local_invocation_index) lane: u32) {
  let count = history.sample_count;
  var sum = 0.0;
  for (var i = lane; i < count; i += ${kBokehStatsWorkgroupSize}u) {
    sum += history.ages[i];
  }
  partial_sums[lane] = sum;
  workgroupBarrier();
  for (var stride = ${kBokehStatsWorkgroupSize / 2}u; stride > 0u; stride >>= 1u) {
    if (lane < stride) { partial_sums[lane] += partial_sums[lane + stride]; }
    workgroupBarrier();
  }
  if (lane == 0u) {
    stats.alive_count = atomicLoad(&alive_count);
    stats.lifetime_sample_count = count;
    stats.lifetime_sum = partial_sums[0];
    stats.padding = 0u;
  }
}
`;

const kBokehDrawWGSL = kBokehParticleWGSL + `
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
  let size_scale = max(bokeh_draw.frame.x, bokeh_draw.frame.y) * (1.0 / f32(${kBokehReferenceScreenSize}));
  let r = bokeh_inner_radius(z, bokeh_draw.frame.z) * size_scale;

  // Half-pixel padding lets edge pixels integrate their covered area.
  let offset = corners[vid] * (r + ${kBokehEdgeFalloff} + 0.5);
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
  let edge = ${kBokehEdgeFalloff};
  let min_r = ${kBokehMinInnerRadius} * size_scale;
  let focused_area = min_r*min_r + min_r*edge + 0.3*edge*edge;
  let area = r*r + r*edge + 0.3*edge*edge;
  out.color = 
      color * (${kBokehHdrBrightness} * envelope * focused_area / area) * 
      bokeh_draw.background.w;  // ~opacity
  return out;
}
@fragment
fn fs_bokeh(in: BokehVertex) -> @location(0) vec4f {
  let radius = length(in.offset);
  let outer_radius = in.inner_radius + ${kBokehEdgeFalloff};
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
`;

class GpuBokehDots {
  constructor(device, warp_field_wgsl) {
    this.device = device;
    this.pending = [];
    this.true_time = 0;
    this.focal_distance = kCameraFocalDistance;
    this.output_width = 1;
    this.output_height = 1;
    this.initialized = false;
    this.bound_views = [];
    this.draw_pipelines = new Map();
    this.draw_bind_groups = new Map();
    const buffer = (label, size, usage) => device.createBuffer({label, size, usage});
    this.bokeh_dot_buf = buffer('Bokeh dots: persistent state', kBokehDotCapacity * kBokehDotStride,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST);
    this.bokeh_spawn_buf = buffer('Bokeh dots: new particles', kBokehDotCapacity * kBokehDotStride,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST);
    this.bokeh_alive_count_buf = buffer('Bokeh dots: alive count', 4,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST);
    this.bokeh_death_events_buf = buffer('Bokeh dots: death events', kBokehDeathEventCount * 8,
      GPUBufferUsage.STORAGE);
    this.bokeh_history_buf = buffer('Bokeh dots: rolling lifetime history', 16 + kBokehLifetimeWindowSize * 4,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST);
    this.bokeh_stats_buf = buffer('Bokeh dots: stats snapshot', 16,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC);
    this.bokeh_alive_readback_buf = buffer('Bokeh dots: async stats readback', 16,
      GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ);
    this.bokeh_alive_count = 0;
    this.bokeh_average_lifespan = null; // Seconds; null until seeded or a death is read back.
    this.bokeh_lifetime_sample_count = 0;
    this.bokeh_history_generation = 0;
    this.bokeh_alive_readback_pending = false;
    this.bokeh_alive_last_request_ms = -Infinity;
    this.bokeh_alive_readback_warned = false;
    this.bokeh_destroyed = false;
    this.bokeh_cursor_buf = buffer('Bokeh dots: spawn cursor', 4,
      GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST);
    this.bokeh_update_buf = buffer('Bokeh dots: update controls', kBokehUpdateUniformBytes,
      GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
    this.bokeh_draw_buf = buffer('Bokeh dots: draw controls', 32,
      GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
    this.bokeh_palette_buf = buffer('Bokeh dots: current linear HDR palette', 256 * 16,
      GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST);
    this.bokeh_update_data = new ArrayBuffer(kBokehUpdateUniformBytes);
    this.bokeh_bounds_width = 0;
    this.bokeh_bounds_height = 0;
    this.bokeh_update_f32 = new Float32Array(this.bokeh_update_data);
    this.bokeh_update_f32.fill(1.0, kBokehFollowWeightsOffset);
    this.bokeh_update_u32 = new Uint32Array(this.bokeh_update_data);
    this.bokeh_draw_data = new Float32Array(8);
    this.bokeh_spawn_data = new Float32Array(kBokehDotCapacity * kBokehDotStride / 4);
    this.bokeh_zero = new Uint32Array(1);
    this.bokeh_pipeline = device.createComputePipeline({
      label: 'Bokeh dots: advection and lifetime', layout: 'auto',
      compute: {module: device.createShaderModule({code: warp_field_wgsl + kBokehAdvectionWGSL}),
                entryPoint: 'cs_bokeh'},
    });
    this.bokeh_record_deaths_pipeline = device.createComputePipeline({
      label: 'Bokeh dots: record death ages', layout: 'auto',
      compute: {module: device.createShaderModule({code: kBokehRecordDeathsWGSL}),
                entryPoint: 'cs_record_deaths'},
    });
    this.bokeh_record_deaths_group = device.createBindGroup({
      layout: this.bokeh_record_deaths_pipeline.getBindGroupLayout(0), entries: [
        {binding: 0, resource: {buffer: this.bokeh_death_events_buf}},
        {binding: 1, resource: {buffer: this.bokeh_history_buf}},
      ],
    });
    this.bokeh_stats_pipeline = device.createComputePipeline({
      label: 'Bokeh dots: reduce rolling lifetime ages', layout: 'auto',
      compute: {module: device.createShaderModule({code: kBokehStatsWGSL}),
                entryPoint: 'cs_bokeh_stats'},
    });
    this.bokeh_stats_group = device.createBindGroup({
      layout: this.bokeh_stats_pipeline.getBindGroupLayout(0), entries: [
        {binding: 0, resource: {buffer: this.bokeh_history_buf}},
        {binding: 1, resource: {buffer: this.bokeh_alive_count_buf}},
        {binding: 2, resource: {buffer: this.bokeh_stats_buf}},
      ],
    });
    this.opacity = 1.0;
  }

  // XY uses the same aspect-correct normalized coordinates as the waveform.
  // Returns how many requests were queued. At GPU capacity, excess spawns are
  // dropped without overwriting living particles (no synchronous readback).
  add_bokeh_dots(points, width, height, true_time) {
    if (!(width > 0 && height > 0 && Number.isFinite(true_time))) return 0;
    const xform = new TransformHelper(width, height);
    let accepted = 0;
    for (const p of points) {
      if (this.pending.length >= kBokehDotCapacity) break;
      const {x, y, z, z2 = z, use_background = false, palette_position = 0.5,
             fade_in = 0.25, hold = 2.0, fade_out = 1.0} = p;
      if (![x, y, z, z2, palette_position, fade_in, hold, fade_out].every(Number.isFinite) ||
          z < 0 || z > 1 || z2 < 0 || z2 > 1 || palette_position < 0 || palette_position > 1 ||
          Math.min(fade_in, hold, fade_out) < 0 || fade_in + hold + fade_out <= 0) continue;
      this.pending.push([
        (xform.NormToScreenX(x) + 0.5) / width,
        (xform.NormToScreenY(y) + 0.5) / height, z, 1,
        palette_position, use_background ? 1 : 0, true_time, fade_in,
        hold, fade_out, z, z2,
      ]);
      accepted++;
    }
    return accepted;
  }

  set_bokeh_frame({true_time, focal_distance = 0.5, output_width, output_height, opacity}) {
    if (![true_time, focal_distance, output_width, output_height].every(Number.isFinite) ||
        output_width <= 0 || output_height <= 0) throw new RangeError('Invalid bokeh frame controls');
    this.true_time = true_time;
    this.focal_distance = Math.max(0, Math.min(1, focal_distance));
    this.output_width = output_width;
    this.output_height = output_height;
    this.opacity = opacity;
  }

  // Copy each frame; uploaded with the existing bokeh controls writeBuffer().
  // These factors multiply, rather than replace, the normal warp blend weights.
  set_follow_motions(follow_motions) {
    this.bokeh_update_f32.set(follow_motions, kBokehFollowWeightsOffset);
  }

  set_bokeh_palette(palette) {
    if (palette.length !== 1024) throw new RangeError('Expected 256 RGBA palette entries');
    this.device.queue.writeBuffer(this.bokeh_palette_buf, 0,
      palette instanceof Float32Array ? palette : new Float32Array(palette));
  }

  encode_bokeh_update(encoder, warp_views, sampler, warp_uniform_buf, width, height) {
    if (width !== this.bokeh_bounds_width || height !== this.bokeh_bounds_height) {
      const xform = new TransformHelper(width, height);
      const x0 = (xform.NormToScreenX(-kMinMaxBokehDotXY) + 0.5) / width;
      const x1 = (xform.NormToScreenX( kMinMaxBokehDotXY) + 0.5) / width;
      const y0 = (xform.NormToScreenY(-kMinMaxBokehDotXY) + 0.5) / height;
      const y1 = (xform.NormToScreenY( kMinMaxBokehDotXY) + 0.5) / height;
      this.bokeh_update_f32.set([
        Math.min(x0, x1), Math.min(y0, y1),
        Math.max(x0, x1), Math.max(y0, y1),
      ], 8);
      this.bokeh_bounds_width = width;
      this.bokeh_bounds_height = height;
    }
    if (!this.bokeh_bind_group || sampler !== this.bound_sampler ||
        warp_uniform_buf !== this.bound_uniform_buf ||
        warp_views.some((view, i) => view !== this.bound_views[i])) {
      this.bokeh_bind_group = this.device.createBindGroup({
        layout: this.bokeh_pipeline.getBindGroupLayout(0), entries: [
          {binding: 0, resource: {buffer: this.bokeh_dot_buf}},
          {binding: 2, resource: {buffer: this.bokeh_update_buf}},
          {binding: 3, resource: {buffer: warp_uniform_buf}},
          ...warp_views.map((view, i) => ({binding: i + 4, resource: view})),
          {binding: 12, resource: sampler},
          {binding: 13, resource: {buffer: this.bokeh_spawn_buf}},
          {binding: 14, resource: {buffer: this.bokeh_cursor_buf}},
          {binding: 15, resource: {buffer: this.bokeh_alive_count_buf}},
          {binding: 16, resource: {buffer: this.bokeh_death_events_buf}},
          {binding: 17, resource: {buffer: this.bokeh_history_buf}},
        ],
      });
      this.bound_views = warp_views.slice();
      this.bound_sampler = sampler;
      this.bound_uniform_buf = warp_uniform_buf;
    }
    let spawn_count = 0;
    for (const p of this.pending) {
      if (this.true_time >= p[6] + p[7] + p[8] + p[9]) continue;
      this.bokeh_spawn_data.set(p, spawn_count++ * 12);
    }
    if (spawn_count) this.device.queue.writeBuffer(this.bokeh_spawn_buf, 0,
      this.bokeh_spawn_data.buffer, 0, spawn_count * kBokehDotStride);
    this.pending.length = 0;
    this.device.queue.writeBuffer(this.bokeh_cursor_buf, 0, this.bokeh_zero);
    this.bokeh_update_f32.set([this.true_time, this.focal_distance, this.output_width, this.output_height]);
    this.bokeh_update_u32[4] = spawn_count;
    this.device.queue.writeBuffer(this.bokeh_update_buf, 0, this.bokeh_update_data);
    // Reset in command order immediately before the dispatch that counts.
    encoder.clearBuffer(this.bokeh_alive_count_buf);
    const pass = encoder.beginComputePass({label: 'Advect bokeh dots'});
    pass.setPipeline(this.bokeh_pipeline);
    pass.setBindGroup(0, this.bokeh_bind_group);
    pass.dispatchWorkgroups(Math.ceil(kBokehDotCapacity / kWorkgroupSize));
    pass.end();
    const record_pass = encoder.beginComputePass({label: 'Record bokeh death ages'});
    record_pass.setPipeline(this.bokeh_record_deaths_pipeline);
    record_pass.setBindGroup(0, this.bokeh_record_deaths_group);
    record_pass.dispatchWorkgroups(1);
    record_pass.end();
    this.initialized = true;
  }

  // Seed once at startup, before spawning, with the caller's maximum lifetime
  // (fade-in + hold + fade-out), in seconds. Real deaths replace these entries.
  // Calling again explicitly resets the history, but does not reset particles.
  initialize_lifetime_history(max_lifetime) {
    const seed = Math.fround(max_lifetime);
    if (!Number.isFinite(max_lifetime) || !Number.isFinite(seed) || seed <= 0) {
      throw new RangeError('Bokeh maximum lifetime must be positive, finite seconds.');
    }
    if (this.bokeh_destroyed) return;
    const data = new ArrayBuffer(16 + kBokehLifetimeWindowSize * 4);
    // Header: next_ticket, sample_count, processed_ticket, write_offset.
    new Uint32Array(data, 0, 4)[1] = kBokehLifetimeWindowSize;
    new Float32Array(data, 16).fill(seed);
    this.device.queue.writeBuffer(this.bokeh_history_buf, 0, data);
    ++this.bokeh_history_generation;
    this.bokeh_lifetime_sample_count = kBokehLifetimeWindowSize;
    this.bokeh_average_lifespan = seed;
    this.bokeh_alive_last_request_ms = -Infinity;
  }

  // Synchronous getter; schedules a nonblocking refresh at the configured interval.
  // Reports the most recently read GPU snapshot, initially 0. Calling every
  // engine frame is fine. It counts alive slots, not just visibly bright dots.
  // When simulation is frozen, the last simulated alive count is retained.
  get_alive_count() {
    const now_ms = performance.now();
    if (!this.bokeh_destroyed && this.initialized &&
        !this.bokeh_alive_readback_pending &&
        now_ms - this.bokeh_alive_last_request_ms >= kBokehAliveCountReadbackIntervalMs) {
      this.bokeh_alive_readback_pending = true;
      this.bokeh_alive_last_request_ms = now_ms;
      void this._read_bokeh_alive_count();
    }
    return this.bokeh_alive_count;
  }

  // Average actual lifespan in seconds over the latest window of deaths.
  // Includes any remaining startup seeds. Without seeding or deaths, returns null.
  // Both getters share one throttled readback.
  get_average_lifespan() {
    this.get_alive_count();
    return this.bokeh_average_lifespan;
  }

  async _read_bokeh_alive_count() {
    const readback = this.bokeh_alive_readback_buf;
    const history_generation = this.bokeh_history_generation;
    try {
      // A separate submission ensures the copy precedes mapAsync(), without
      // requiring the caller to notify us when its warp encoder is submitted.
      // This snapshot follows all previously submitted simulation work.
      const encoder = this.device.createCommandEncoder({label: 'Read bokeh particle stats'});
      const pass = encoder.beginComputePass({label: 'Reduce bokeh lifespan window'});
      pass.setPipeline(this.bokeh_stats_pipeline);
      pass.setBindGroup(0, this.bokeh_stats_group);
      pass.dispatchWorkgroups(1);
      pass.end();
      encoder.copyBufferToBuffer(this.bokeh_stats_buf, 0, readback, 0, 16);
      this.device.queue.submit([encoder.finish()]);
      await readback.mapAsync(GPUMapMode.READ, 0, 16);
      if (!this.bokeh_destroyed) {
        const data = readback.getMappedRange(0, 16);
        const integers = new Uint32Array(data);
        this.bokeh_alive_count = integers[0];
        // A snapshot submitted before re-seeding must not overwrite the new mean.
        if (history_generation === this.bokeh_history_generation) {
          this.bokeh_lifetime_sample_count = integers[1];
          this.bokeh_average_lifespan = integers[1] > 0
            ? new Float32Array(data)[2] / integers[1] : null;
        }
      }
    } catch (error) {
      // Device loss or destruction must not cause an unhandled rejection.
      if (!this.bokeh_destroyed && !this.bokeh_alive_readback_warned) {
        console.warn('Bokeh stats readback failed; retaining cached values.', error);
        this.bokeh_alive_readback_warned = true;
      }
    } finally {
      if (readback.mapState === 'mapped') readback.unmap();
      this.bokeh_alive_readback_pending = false;
    }
  }

  // Draw only into the presenter's pass; never into GPUWarp's iterative target.
  draw_bokeh(pass, format, {width, height, focal_distance = this.focal_distance,
                          true_time = this.true_time, background = [0, 0, 0]} = {}) {
    if (!this.initialized || !(width > 0 && height > 0)) return;
    if (!this.draw_pipelines.has(format)) {
      const module = this.device.createShaderModule({code: kBokehDrawWGSL});
      const pipeline = this.device.createRenderPipeline({
        label: 'Present-only HDR bokeh dots', layout: 'auto',
        vertex: {module, entryPoint: 'vs_bokeh'},
        fragment: {module, entryPoint: 'fs_bokeh', targets: [{format,
          writeMask: GPUColorWrite.RED | GPUColorWrite.GREEN | GPUColorWrite.BLUE,
          blend: {color: {operation: 'add', srcFactor: 'one', dstFactor: 'one'},
                  alpha: {operation: 'add', srcFactor: 'zero', dstFactor: 'one'}}}]},
        primitive: {topology: 'triangle-strip'},
      });
      this.draw_pipelines.set(format, pipeline);
      this.draw_bind_groups.set(format, this.device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0), entries: [
          {binding: 0, resource: {buffer: this.bokeh_dot_buf}},
          {binding: 1, resource: {buffer: this.bokeh_draw_buf}},
          {binding: 2, resource: {buffer: this.bokeh_palette_buf}},
        ],
      }));
    }
    this.bokeh_draw_data.set([width, height, Math.max(0, Math.min(1, focal_distance)), true_time,
      background[0], background[1], background[2], this.opacity]);
    this.device.queue.writeBuffer(this.bokeh_draw_buf, 0, this.bokeh_draw_data);
    pass.setPipeline(this.draw_pipelines.get(format));
    pass.setBindGroup(0, this.draw_bind_groups.get(format));
    pass.draw(4, kBokehDotCapacity);
  }

  destroy() {
    this.bokeh_destroyed = true;
    for (const buffer of [this.bokeh_death_events_buf, this.bokeh_history_buf, this.bokeh_stats_buf,
      this.bokeh_alive_count_buf, this.bokeh_alive_readback_buf,
      this.bokeh_dot_buf, this.bokeh_spawn_buf, this.bokeh_cursor_buf,
      this.bokeh_update_buf, this.bokeh_draw_buf, this.bokeh_palette_buf]) buffer.destroy();
    this.pending.length = 0;
  }
}

// Compatibility facade for GPUWarp. Existing methods affect WARP DOTS only,
// except encode_update()/destroy(), which service both independent pools.
export class GpuFadingDots {
  constructor(device, warp_field_wgsl) {
    this.warp_dots = new GpuWarpDots(device, warp_field_wgsl);
    this.bokeh_dots = new GpuBokehDots(device, warp_field_wgsl);
  }
  // Safe to call each engine frame; returns a slightly delayed cached count.
  get_bokeh_alive_count() { return this.bokeh_dots.get_alive_count(); }
  // Call once before spawning; max_lifetime = fade_in + hold + fade_out, in seconds.
  initialize_bokeh_lifetime_history(max_lifetime) {
    this.bokeh_dots.initialize_lifetime_history(max_lifetime);
  }
  // Seconds, or null until seeded or at least one death has been read back.
  get_bokeh_average_lifespan() { return this.bokeh_dots.get_average_lifespan(); }
  set_bokeh_follow_motions(follow_motions) {
    this.bokeh_dots.set_follow_motions(follow_motions);
  }
  set_params(params) { this.warp_dots.set_params(params); }
  encode_update(encoder, views, sampler, uniforms, width, height) {
    this.warp_dots.encode_update(encoder, views, sampler, uniforms, width, height);
    this.bokeh_dots.encode_bokeh_update(encoder, views, sampler, uniforms, width, height);
  }
  draw(encoder, target_view, renderer) { this.warp_dots.draw(encoder, target_view, renderer); }
  destroy() { this.warp_dots.destroy(); this.bokeh_dots.destroy(); }
}
