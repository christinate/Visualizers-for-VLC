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

@group(0) @binding(0) var<storage, read> events: array<BokehDeathEvent>;
@group(0) @binding(1) var<storage, read_write> history: BokehLifetimeHistory;
@compute @workgroup_size(256)
fn cs_record_deaths(@builtin(local_invocation_index) lane: u32) {
  let end_ticket = atomicLoad(&history.next_ticket);
  let start_ticket = history.processed_ticket;
  let start_offset = history.write_offset;
  let deaths_this_update = end_ticket - start_ticket;
  for (var id = lane; id < 8192u; id += 256u) {
    let event = events[id];
    if (event.age >= 0.0 && end_ticket - event.ticket <= 1024u) {
      let offset = (start_offset + (event.ticket - start_ticket)) % 1024u;
      history.ages[offset] = event.age;
    }
  }
  // One workgroup owns the entire scatter. Finish all ring reads/writes before
  // advancing metadata; unsigned ticket subtraction also handles u32 rollover.
  storageBarrier();
  if (lane == 0u) {
    history.sample_count = min(1024u,
                               history.sample_count + deaths_this_update);
    history.processed_ticket = end_ticket;
    history.write_offset = (start_offset + deaths_this_update) % 1024u;
  }
}

