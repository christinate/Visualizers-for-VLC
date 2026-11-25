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

import { Warp, Preset } from "./preset.js"

function build_preset(payload) {
	return new Preset(payload.global_preset_number, payload.t1, payload.t2);
}

function rebuild_warp_maps(payload) {
  let preset = payload.preset;

  // The data for 'preset' was cloned between the threads.
  // ...BUT the methods were stripped, so we need to restore those.
  Object.setPrototypeOf(preset, Preset.prototype);
  for (const warp of preset.warps) {
    Object.setPrototypeOf(warp, Warp.prototype);
  }

  const calling_from_background_thread = true;
  preset.RebuildWarpMaps(calling_from_background_thread);
  return preset;
}

// worker.js (module worker)
self.onmessage = (e) => {
  const { payload } = e.data;
  if (e.data.type === "build_preset") {
    // Do heavy work here
    const result = build_preset(payload);

    self.postMessage({ type: "build_preset_done", result });
  }
  if (e.data.type === "rebuild_warp_maps") {
    // Do heavy work here
    const result = rebuild_warp_maps(payload);

    self.postMessage({ type: "rebuild_warp_maps_done", result });
  }
};
