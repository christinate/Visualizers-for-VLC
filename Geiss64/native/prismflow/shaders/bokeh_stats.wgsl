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

struct BokehStats {
  alive_count: u32,
  lifetime_sample_count: u32,
  lifetime_sum: f32,
  padding: u32,
};
@group(0) @binding(0) var<storage, read_write> history: BokehLifetimeHistory;
@group(0) @binding(1) var<storage, read_write> alive_count: atomic<u32>;
@group(0) @binding(2) var<storage, read_write> stats: BokehStats;
var<workgroup> partial_sums: array<f32, 256>;
@compute @workgroup_size(256)
fn cs_bokeh_stats(@builtin(local_invocation_index) lane: u32) {
  let count = history.sample_count;
  var sum = 0.0;
  for (var i = lane; i < count; i += 256u) {
    sum += history.ages[i];
  }
  partial_sums[lane] = sum;
  workgroupBarrier();
  for (var stride = 128u; stride > 0u; stride >>= 1u) {
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

