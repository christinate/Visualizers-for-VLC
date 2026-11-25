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

// ===============================
// Palette overlay pass (no sampler, uses textureLoad + discard)
// Draws a 256x64 quad anchored to lower-right.
// ===============================

// WGSL: full-screen-ish quad generated from vertex_index (no vertex buffer).
export const kPaletteOverlayWGSL = /* wgsl */`
struct UiUniforms {
  screen_w : f32,
  screen_h : f32,
  rect_w   : f32,
  rect_h   : f32,
  // ---- padding boundary
  margin_x : f32,
  margin_y : f32,
  palette_swatch_count : f32,
  bkg_r : f32,
  // ---- padding boundary
  bkg_g : f32,
  bkg_b : f32,
  pad1 : f32,
  pad2 : f32,
};

@group(0) @binding(0) var palette_tex : texture_2d<f32>;
@group(0) @binding(1) var<uniform> ui : UiUniforms;

struct VSOut {
  @builtin(position) pos : vec4<f32>,
  @location(0) uv : vec2<f32>,
};

// Two triangles (6 verts) with UVs spanning [0..1].
// We place the rect in pixel space then convert to NDC.
@vertex
fn vs_main(@builtin(vertex_index) vid : u32) -> VSOut {
  var out : VSOut;

  // 6 vertices => (0,0)(1,0)(0,1) and (0,1)(1,0)(1,1)
  var p : vec2<f32>;
  switch (vid) {
    case 0u: { p = vec2<f32>(0.0, 0.0); }
    case 1u: { p = vec2<f32>(1.0, 0.0); }
    case 2u: { p = vec2<f32>(0.0, 1.0); }
    case 3u: { p = vec2<f32>(0.0, 1.0); }
    case 4u: { p = vec2<f32>(1.0, 0.0); }
    default:{ p = vec2<f32>(1.0, 1.0); } // vid == 5
  }

  let swatch_size = ui.rect_w / max(ui.palette_swatch_count, 1.0);
  let gap = swatch_size * 0.25;
  let has_background =
      ui.bkg_r >= 0.0 && ui.bkg_g >= 0.0 && ui.bkg_b >= 0.0;

  // Enlarge the quad leftward when the background swatch is present.
  let extra_width = select(0.0, swatch_size + gap, has_background);

  // Also accommodate a square taller than the original palette rectangle.
  let total_height = select(
      ui.rect_h,
      max(ui.rect_h, swatch_size),
      has_background);

  let x0 = ui.screen_w - ui.margin_x - ui.rect_w;
  let y0 = ui.screen_h - ui.margin_y - ui.rect_h;
  let bottom = ui.screen_h - ui.margin_y;

  let px = x0 - extra_width + p.x * (ui.rect_w + extra_width);
  let py = bottom - total_height + p.y * total_height;

  let ndc_x = (px / ui.screen_w) * 2.0 - 1.0;
  let ndc_y = 1.0 - (py / ui.screen_h) * 2.0;

  out.pos = vec4<f32>(ndc_x, ndc_y, 0.0, 1.0);

  // Coordinates relative to the ORIGINAL palette rectangle.
  // Negative X now covers the background swatch and its separating gap.
  out.uv = vec2<f32>(
      (px - x0) / ui.rect_w,
      (py - y0) / ui.rect_h);

  return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  let count = ui.palette_swatch_count;
  if (count < 1.0) {
    discard;
  }

  let swatch_size = ui.rect_w / count;
  let gap = swatch_size * 0.25;

  // Local pixel coordinates relative to the original palette rectangle.
  let x = in.uv.x * ui.rect_w;
  let y = in.uv.y * ui.rect_h;

  if (x < 0.0) {
    let has_background =
        ui.bkg_r >= 0.0 && ui.bkg_g >= 0.0 && ui.bkg_b >= 0.0;

    let inside_background =
        x >= -(swatch_size + gap) &&
        x < -gap &&
        y >= ui.rect_h - swatch_size &&
        y < ui.rect_h;

    if (!has_background || !inside_background) {
      discard;
    }

    // Background RGB is already linear.
    return vec4<f32>(ui.bkg_r, ui.bkg_g, ui.bkg_b, 1.0);
  }

  // Discard any extra area above the original palette rectangle.
  if (y < 0.0 || y >= ui.rect_h) {
    discard;
  }

  let dims = textureDimensions(palette_tex);

  let swatch_index = clamp(
      floor(in.uv.x * count),
      0.0,
      count - 1.0);

  let t = swatch_index / max(count - 1.0, 1.0);
  let xi = clamp(
      i32(t * f32(dims.x - 1u)),
      0,
      i32(dims.x - 1u));

  var col = textureLoad(palette_tex, vec2<i32>(xi, 0), 0);

  // Preserve the existing palette texture decoding.
  col = col * col;
  col *= 3.0;

  return vec4<f32>(col.rgb, 1.0);
}
`;

export class PaletteOverlayPass {
  constructor(device, canvas_format, palette_texture /* GPUTexture */) {
    this.device = device;
    this.canvas_format = canvas_format;

    // Uniforms: 6 floats = 24 bytes; WGSL uniform layout needs 16-byte alignment,
    // so we pad to 32 bytes to be safe.
    const kUiUniformBytes = 64;
    this.ui_ubo = device.createBuffer({
      size: kUiUniformBytes,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      label: "palette_overlay_ui_ubo",
    });

    this.bind_group_layout = device.createBindGroupLayout({
      label: "palette_overlay_bgl",
      entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float", viewDimension: "2d" } },
        { binding: 1, visibility: GPUShaderStage.VERTEX|GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } },
      ],
    });

    this.pipeline_layout = device.createPipelineLayout({
      label: "palette_overlay_pl",
      bindGroupLayouts: [this.bind_group_layout],
    });

    const module = device.createShaderModule({
      label: "palette_overlay_wgsl",
      code: kPaletteOverlayWGSL,
    });

    this.pipeline = device.createRenderPipeline({
      label: "palette_overlay_pipeline",
      layout: this.pipeline_layout,
      vertex: {
        module,
        entryPoint: "vs_main",
      },
      fragment: {
        module,
        entryPoint: "fs_main",
        targets: [
          {
            format: canvas_format,
            // No blending needed if you use discard for transparency.
            // If you later decide you want alpha compositing instead, add blend here.
          },
        ],
      },
      primitive: { topology: "triangle-list", cullMode: "none" },
      // depthStencil omitted => no depth
    });

    this.palette_view = palette_texture.createView({ label: "palette_overlay_palette_view" });

    this.bind_group = device.createBindGroup({
      label: "palette_overlay_bg",
      layout: this.bind_group_layout,
      entries: [
        { binding: 0, resource: this.palette_view },
        { binding: 1, resource: { buffer: this.ui_ubo } },
      ],
    });
  }

  // Call whenever canvas size changes (or each frame; it’s tiny).
  update_ui_uniforms(
      screen_w, screen_h, 
      palette_swatch_count = 8, 
      rect_w = 256, rect_h = 64, 
      margin_x = 8, margin_y = 8,
      bkg_rgb = null) {
    // std140-ish packing safety: we’ll pack 6 f32 then pad to 8 f32 (32 bytes).
    const data = new Float32Array(12);
    data[0] = screen_w;
    data[1] = screen_h;
    data[2] = rect_w;
    data[3] = rect_h;
    // ---- padding boundary
    data[4] = margin_x;
    data[5] = margin_y;
    data[6] = palette_swatch_count;
    data[7] = (bkg_rgb != null) ? bkg_rgb[0] : -1.0;
    // ---- padding boundary
    data[8] = (bkg_rgb != null) ? bkg_rgb[1] : -1.0;
    data[9] = (bkg_rgb != null) ? bkg_rgb[2] : -1.0;
    data[10] = 0.0;  // padding
    data[11] = 0.0;  // padding
    // ---- padding boundary
    this.device.queue.writeBuffer(this.ui_ubo, 0, data.buffer, 0, data.byteLength);
  }

  // Call inside your existing render pass that targets the swapchain view,
  // *after* warp and dots are drawn.
  draw(render_pass_encoder) {
    render_pass_encoder.setPipeline(this.pipeline);
    render_pass_encoder.setBindGroup(0, this.bind_group);
    render_pass_encoder.draw(6, 1, 0, 0); // 6 verts, 1 instance
  }
}

// ===============================
// Usage sketch
// ===============================
//
// 1) Create once (after you have device, canvas_format, and the palette texture):
//   this.palette_overlay = new PaletteOverlayPass(this.device, this.format, this.g_palette1.texture);
//
// 2) Each resize or per frame (use your actual canvas pixel size):
//   this.palette_overlay.update_ui_uniforms(this.cw, this.ch, 256, 64, 8, 8);
//
// 3) In your final onscreen render pass (same one you draw dots into):
//   this.palette_overlay.draw(pass_encoder);
//