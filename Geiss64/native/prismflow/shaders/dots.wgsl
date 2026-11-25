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

struct DotUniforms {
  width  : f32,
  height : f32,
  _pad0  : f32,
  _pad1  : f32,
};

// The radial distance over which the dots will fade from opaque to transparent,
// relative to the dot radius.  (Applies to 3x3 or larger dots only.)
// Range: [0..1]
// At 0, a crisp dot of the desired radius is drawn.
// At 1, a blurry dot of double that radius is drawn.
const kRadiusFade = 0.5;//0.25;//0.2;

@group(0) @binding(0) var<uniform> u : DotUniforms;

// Vertex output + fragment input
struct VSOut {
  @builtin(position) pos : vec4f,
  @location(0) a         : f32,
  @location(1) r1        : f32, // inner radius
  @location(2) r2        : f32, // outer radius
  @location(3) center_pc : vec2f, 
};

@vertex
fn vs_main(
  @location(0) corner_unit : vec2f, // (-1,-1),(+1,-1),(-1,+1),(+1,+1)
  @location(1) inst        : vec4f  // (center_x_pc, center_y_pc, size_px, alpha)
) -> VSOut {
  let center_pc = inst.xy;         // pixel-center coords (integer = pixel center)
  let size_px   = max(inst.z, 1.0);
  let a         = inst.w;

  // size_px:  quad size:   half_extent_pc:   circle_r:
  // 1         1x1          0.5               4
  // 2         2x2          1                 4
  // 3         5x5          2.5               3
  // 4         6x6          3                 4
  // 5         7x7          3.5               5

  // A size of 1 means cover exactly one pixel: center_pc +/- 0.5 in pixel-center coords.
  var half_extent_pc = 0.5 * size_px;

  // Compute circle radius of the white part of the dot to send down to fragment shader.
  // - If point size is 1x1: force very large so it fully covers the square.
  // - If radius > 1: circle_radius = radius - 1.
  // - Otherwise (i.e., ~3x3 to ~4x4-ish), also force large so it's fully covered.
  //   (This matches your intent: small dots should not get circularly clipped.)
  var r1 = 4.0;
  var r2 = 5.0;
  if (size_px >= 1.0001) {
		// Find the exact circle radius that would have the exact same square area
		// as the NxN-pixel square dot.
    let square_area = size_px * size_px;
    let ideal_r = pow(square_area / 3.1415927, 0.5);

		r1 = ideal_r * (1.0 - kRadiusFade);
		r2 = ideal_r * (1.0 + kRadiusFade);
		// The antialiasing will be from [rad - kRadiusFadeDistance/2] ... [rad + kRadiusFadeDistance/2],
		// so choose the square size to make sure we cover it.
    half_extent_pc = ceil(r2);
  }
 
  // Quad corners in pixel-center coords.
  let p_pc = center_pc + corner_unit * half_extent_pc;

  // Convert pixel-center coords -> pixel coords used for NDC mapping:
  // pixel center x corresponds to pixel coord x+0.5
  let p_px = p_pc + 0.5;

  let ndc_x = (p_px.x / u.width) * 2.0 - 1.0;
  let ndc_y = 1.0 - (p_px.y / u.height) * 2.0;

  var out: VSOut;
  out.pos = vec4f(ndc_x, ndc_y, 0.0, 1.0);
  out.a = a;
  out.r1 = r1;
  out.r2 = r2;
  out.center_pc = center_pc;
  return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4f {
  // Fragment position in pixel-center coords (integer means pixel center)
  let pc = in.pos.xy - 0.5;

  // Distance from dot center in pixel-center coords.
  // This should be exactly zero in the center of the anti-aliased edge.
  let r = length(pc - in.center_pc);

  // 1 inside circle_r, 0 outside, with a 1-pixel fade band.
  // Fade over [circle_r .. circle_r + 1]
  // smoothstep(edge0, edge1, x) is a clamped, smooth threshold function.
	//   It returns 0 when x <= edge0
	//   It returns 1 when x >= edge1
	//   Between edge0 and edge1, it transitions smoothly from 0→1 with zero slope at both ends (so no sharp corners / banding).
  //let coverage = 1.0 - smoothstep(in.circle_r, in.circle_r + 1.0, d);
  var coverage = 1.0 - (r - in.r1) / (in.r2 - in.r1 + 0.000001);
  coverage = max(0.0, min(1.0, coverage));
  
  let intensity = clamp(in.a, 0.0, 1.0) * coverage;
  return vec4f(intensity);
}

