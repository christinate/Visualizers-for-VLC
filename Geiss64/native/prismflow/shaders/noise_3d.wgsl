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

struct NoiseSeed { value: u32, pad0: u32, pad1: u32, pad2: u32 };
@group(0) @binding(1) var<uniform> seed: NoiseSeed;
fn hash_u32(value: u32) -> u32 {
  var x = value;
  x = (x ^ (x >> 16u)) * 0x7feb352du;
  x = (x ^ (x >> 15u)) * 0x846ca68bu;
  return x ^ (x >> 16u);
}
fn random_channel(value: u32) -> f32 {
  return min(f32(hash_u32(value) >> 8u) / 16777216.0, 0.99951171875);
}
fn noise_rgba(index: u32) -> vec4f {
  let x = hash_u32(index ^ seed.value);
  return vec4f(random_channel(x ^ 0xa511e9b3u), random_channel(x ^ 0x63d83595u),
               random_channel(x ^ 0xb5297a4du), random_channel(x ^ 0x1b56c4e9u));
}

@group(0) @binding(0) var output_noise: texture_storage_3d<rgba16float, write>;
@compute @workgroup_size(8, 8, 1)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
  let size = textureDimensions(output_noise);
  if (any(id.xyz >= size)) { return; }
  let index = (id.z * size.y + id.y) * size.x + id.x;
  textureStore(output_noise, vec3i(id.xyz), noise_rgba(index));
}

