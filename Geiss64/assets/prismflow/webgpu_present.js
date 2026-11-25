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

import { ShowError, HideError } from "./error.js"
import { GPUWarp, kWarpWGSL } from "./gpu_warp.js";
import { PaletteOverlayPass } from "./draw_palette.js";
//import { kWarpWGSL } from "./warp_pass.wgsl"; // export const kWarpWGSL = `...`;

/*
Feedback textures store four normalized rgba16float channels. Presentation uses
R and G (the original two channels), averaging all supersamples before palette
lookup. B and A remain available for future presentation effects.
*/

//console.log("LOADED webgpu_present.js", new Date().toISOString());

//----------------------------------- WGSL:

const shaderWGSL = /* wgsl */`
struct Uniforms {
  //rgb_scale : vec4f,
  //rgb_power : vec4f,
  oversample : vec4f,		// .x = oversample, .y = 1.0/overesample
  //--------- padding boundary ---------
	title_left_px   : f32,
	title_top_px    : f32,
	title_width_px  : f32,
	title_height_px : f32,
  //--------- padding boundary ---------
	present_w       : f32,
	present_h       : f32,
	title_intensity : f32,
	pad1            : f32,
  //--------- padding boundary ---------
  // The next 3 values are for HDR display calibration:
  calibrating_hdr_headroom : f32,
  selected_square_index : f32,
  width_frac : f32,
  pad2       : f32,
  //--------- padding boundary ---------
  pad3       : f32,
  pad4       : f32,
  layer1_density : f32,
  layer2_brightness : f32,
  //--------- padding boundary ---------
  // The background or companion color for the current palette.
  // Note that at this point, it has already been adapted to the
  //   headroom of the HDR display.  Values can be > 1.
  // And they are not packed to u8 like the palette is (which involves
  //   applying a scale and a power).
  bkg_r : f32,    
  bkg_g : f32,    
  bkg_b : f32,    
  hdr_display_headroom : f32,    
  //--------- padding boundary ---------

  };

@group(0) @binding(0) var indexTex : texture_2d<f32>;
@group(0) @binding(1) var<uniform> u : Uniforms;
@group(0) @binding(2) var paletteTex : texture_2d<f32>;
//@group(0) @binding(3) var paletteSamp : sampler;
@group(0) @binding(3) var title_tex  : texture_2d<f32>;
@group(0) @binding(4) var title_samp : sampler;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
  // Fullscreen triangle
  var pos = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(-1.0,  3.0),
    vec2f( 3.0, -1.0)
  );
  let p = pos[vid];
  return vec4f(p, 0.0, 1.0);
}

// Returns [0..1]
fn overlay_title(frag_px: vec2f) -> f32 {
  if (u.title_intensity <= 0.0) {
    return 0.0;
  }

  let left   = u.title_left_px * u.oversample.y;
  let top    = u.title_top_px * u.oversample.y;
  let right  = left + u.title_width_px * u.oversample.y;
  let bottom = top  + u.title_height_px * u.oversample.y;

  if (frag_px.x < left || frag_px.x >= right || frag_px.y < top || frag_px.y >= bottom) {
    return 0.0;
  }

  let uv = vec2f(
    (frag_px.x - left) * (1.0 / max(u.title_width_px  * u.oversample.y,  1.0)),
    (frag_px.y - top ) * (1.0 / max(u.title_height_px * u.oversample.y, 1.0))
  );

  let a = textureSampleLevel(title_tex, title_samp, uv, 0.0).a * u.title_intensity;

  // White overlay; swap in tinting later if desired.
  //let title_rgb = vec3f(1.0, 1.0, 1.0);
  //return vec4f(base_color.rgb * (1.0 - a) + title_rgb * a, base_color.a);
  return a;
}

// Assumes x0 <= x1 and y0 <= y1. Boundary counts as inside.
fn point_is_inside_rounded_rect(
    x: f32, y: f32,
    x0: f32, x1: f32,
    y0: f32, y1: f32,
    rad: f32
) -> bool {
    let half_size = vec2f(x1 - x0, y1 - y0) * 0.5;
    let center = vec2f(x0, y0) + half_size;
    let r = clamp(rad, 0.0, min(half_size.x, half_size.y));

    let p = abs(vec2f(x, y) - center);
    let corner = max(p - (half_size - vec2f(r)), vec2f(0.0));

    return all(p <= half_size) && dot(corner, corner) <= r * r;
}

// Input should be in [0..1].
// Output color tuple will already be adapted to the HDR headroom of the
//   display, and output values can be > 1.
fn ReadPaletteWithLinearInterp(t: f32) -> vec4f {
  //xxx - todo: make the noise additive (to focus on shadows) and then re-tune it?  Or do we want to see it for brights too?
  //xxx - todo: gaps in palette (skip some interp), for a hard line, anywhere in the palette
  //let t = select(t2, t2 * 0.85 + 0.15, t2 > 0.4);
  //let t = select(t2, t2 * 0.95 + 0.05, t2 > 0.1);
  //let t = t2 * select(1.0, 2.0, t2 > 0.05 && t2 < 0.056);   // GOOD - little blip @ bottom end
  //let t = t2 + select(0, sin(t2 * 6.28 * 40), t2 < 0.05);
  //let t = t2 * (1 + sin(t2 * 6.28 * 40) * max(0.0, 0.12 - t2));  // GOOD - you can see noise @ bottom of palette
  //let t = t2 * (1 + sin(t2 * 6.28 * 40) * max(0.0, 0.2 - t2));  // shadow solarization

  let last = i32(textureDimensions(paletteTex).x) - 1;
  let pos = clamp(t, 0.0, 1.0) * f32(last);
  let index = i32(floor(pos));
  let fraction = fract(pos);

  let col1 = textureLoad(paletteTex, vec2i(index, 0), 0);
  let col2 = textureLoad(paletteTex, vec2i(min(index + 1, last), 0), 0);

  var col = mix(col1, col2, fraction);

	// Square it, to make up for palette packed with kPower == 0.5:
	//xxx - TODO: pull this power from constants.
	//xxx - SEE ALSO: Same unpacking formula repeated in draw_palette.js.
  col = col * col;    // kiv: requires that kPaletteU8PackingPower == 0.5

  //xxx - TODO: Pull this scale from constants.
	//xxx - SEE ALSO: Same unpacking formula repeated in draw_palette.js.
  const kPaletteU8PackingHeadroom = 3;
  col *= kPaletteU8PackingHeadroom;

  return col;
}

// Rotate linear RGB around the gray axis: R = G = B.
//
// turns: 0 = no rotation, 1 = a full revolution.
// Preserves neutral grays, RGB sum, and vector length.
// Does not preserve perceptual luminance or guarantee nonnegative RGB.
fn rotate_rgb(rgb: vec3f, turns: f32) -> vec3f {
  let angle = turns * 6.28318530718;
  let c = cos(angle);

  // Unit rotation axis:
  // k = normalize(vec3f(1.0)) = vec3f(1.0 / sqrt(3.0)).
  //
  // Rodrigues' formula:
  // rotated = rgb * cos(angle)
  //         + cross(k, rgb) * sin(angle)
  //         + k * dot(k, rgb) * (1 - cos(angle)).

  // The cross product with k is:
  // (B-G, R-B, G-R) / sqrt(3).
  let s = sin(angle) * 0.57735026919;

  // Projection onto the gray axis is vec3f(mean(R,G,B)).
  // This term keeps that axial component unchanged as we rotate.
  let common_ = (1.0 - c) *
      (rgb.r + rgb.g + rgb.b) / 3.0;

  return c * rgb
       + s * vec3f(
           rgb.b - rgb.g,
           rgb.r - rgb.b,
           rgb.g - rgb.r
         )
       + vec3f(common_);
}


@fragment
fn fs_main(@builtin(position) p: vec4f) -> @location(0) vec4f {
  // p.xy is in pixel coordinates of the render target
  let x: i32 = i32(p.x);
  let y: i32 = i32(p.y);

	//xxx - TODO: match scale in main.js here
	let sx: i32 = i32(f32(x) * u.oversample.x);
	let sy: i32 = i32(f32(y) * u.oversample.x);	// [sic] - scale is just one value.

	// Oversampling-friendly version:
	var sum = vec4f(0.0);
	let ss = i32(max(1.0, round(u.oversample.x)));
	for (var y = 0; y < ss; y++) {
		for (var x = 0; x < ss; x++) {	
			sum += textureLoad(indexTex, vec2i(sx + x, sy + y), 0).rgba;
		}
	}			

  // Average supersamples directly in normalized floating-point space.
  var channels = sum / f32(ss * ss);

  // Rough experiment with different mag. for layer 2:
  //channels.a = textureLoad(indexTex, vec2i(sx / 2 + i32(u.present_w) / 4, sy / 2 + i32(u.present_h) / 4), 0).a;

  /*
  Plan for alpha channel:
    - currently we draw into R,G,B,A (*verify).
    - currently we soften differently on R,G,B,A.
    - plan is to make A channel a different warp speed, not softened.
  */



  //let t1 = channels.r;
  //let t2 = channels.g;
  //var avg = (t1 + t2) * 0.5;
  //avg = min(1.0, avg + overlay_title(p.xy));

  // Add in the title.
  let title = overlay_title(p.xy);
  channels = saturate(channels + title);

  // FRONT LAYER: .r -> palette lookup.
  // Index into the palette -- using R (for now).
  // Note that the RGB tuple that comes back has already been adapted for the
  //   HDR headroom of the display, and can have values that exceed 1.0.
  // Also note that this function unpacks the value (undoes the fixed power and scale 
  //   applied at texture upload time).
  var col = ReadPaletteWithLinearInterp(channels.r);

  // REAR LAYER: .a -> use to scale 'bkg' color.
  // Color 2 ("background" layer color) is also already adapted to the
  // HDR headroom of the display.  Values can be > 1.
  var col2 = vec4f(u.bkg_r, u.bkg_g, u.bkg_b, 1.0) * channels.a * 2; //ReadPaletteWithLinearInterp(channels.a).gbra;  // TODO: Replace w/compatible 2nd palette
  
  // Mix the 2 layers:
  let layer1_luma = channels.r;
  let t_mix_col2 = saturate(1.0 - layer1_luma * u.layer1_density) * u.layer2_brightness;
  col *= (1 - u.layer2_brightness * 0.1);  // Reduce regular brightness in proportion to the ~overall amount we'll be adding -- so that perceived overall brightness is roughly neutral.
  col += col2 * t_mix_col2;





	// To bypass the palette:
  //col.rgb = channels.rgb;
  //col.rgb = channels.aaa;

    
  // HDR headroom calibration:
  if (u.calibrating_hdr_headroom > 0.0) {
    // ------- If you change this, also change the code in main.js ------------------
    const kSquareCount = 14u;
    const kHdrHeadroomValues = array<f32, kSquareCount>(
        1.00, 1.10, 1.20, 1.30, 1.40, 1.50, 1.65, 1.80, 2.00, 2.20, 2.40, 2.60, 2.80, 3.00 );
    let xc = u.present_w * 0.5;
    let yc = u.present_h * 0.5;
    let squares_w = u.present_w * u.width_frac;
    let square_size = floor(squares_w * (1.0 / f32(kSquareCount)));
    let x0 = xc - square_size * (f32(kSquareCount) * 0.5);
    let x1 = xc + square_size * (f32(kSquareCount) * 0.5);
    let y0 = yc - 0.5 * square_size;
    let y1 = yc + 0.5 * square_size;
    // ------------------------------------------------------------------------------

    const kCircleRad = 0.35;//0.23;  // [0 .. 0.5]
    const kSelectBorderWidth = 0.04;
    const kBlackMargin = 0.21;
    const kRounding = 0.12;  // [0 .. 0.5]
    const kCircleContrast = 0.25;

    let rounding_rad = (y1 - y0) * kRounding;
    if (point_is_inside_rounded_rect(p.x, p.y, x0, x1, y0, y1, rounding_rad)) {
      let square_index = clamp(
        floor((p.x - x0) / square_size),
        0.0,
        f32(kSquareCount - 1u)
      );
      let hdr_headroom = kHdrHeadroomValues[u32(clamp(square_index, 0.0, 12.0))];

      // Compute the normalized coords [0..1,0..1] within the square.
      let square_x0 = x0 + square_index * square_size;
      let square_y0 = y0;
      var nx = (p.x - square_x0) * (1.0 / square_size);  // [0..1]
      var ny = (p.y - square_y0) * (1.0 / square_size);  // [0..1]

      col = vec4f(hdr_headroom);

      // Draw the circle.
      let dx = nx - 0.5;
      let dy = ny - 0.5;
      if (dx * dx + dy * dy < kCircleRad * kCircleRad) {
        col -= vec4f(kCircleContrast);
      }

      // Draw a border around selected square.
      if (abs(square_index - u.selected_square_index) < 0.01 &&
          min(min(nx, 1.0 - nx), min(ny, 1.0 - ny)) < kSelectBorderWidth) {
        col = vec4f(1.0, 0.0, 1.0, 0.0) * 1.5; 
      }
    } else {
      // Draw the faded-to-black margin area.
      // This is important because a broadly bright/dark image can change the HDR headroom
      //   (on a Macbook Pro this doesn't happen, but on an XDR display it does).

      let margin = max(u.present_w, u.present_h) * kBlackMargin;
      var t = 0.5 + min(p.y - (y0 - margin), y1 + margin - p.y) * (1.0 / margin);
      t = saturate(t);              
      t = t * t * (3 - 2 * t);        
      col = mix(col, vec4f(0.0), t);
    }
  }

	return col;  // rg.rrrr;//FIXME_WAVEFORM
}
`;

export class WebGPUPresenter {
  // cw, ch: client size
  // iw, ih: index buffer size (might be 2x smaller, 3x smaller, etc)
  constructor(canvas, cw, ch, iw, ih, oversample) {
  	//console.log("WebGPUPresent constructor: ", cw, ch, iw, ih, scale);
    this.canvas = canvas;
    this.cw = cw;
    this.ch = ch;
    this.iw = iw;
    this.ih = ih;
    this.oversample = oversample;

    this.device = null;
    this.context = null;
    this.format = null;

    this.indexTex = null;
    this.paletteTex = null;

    this.pipeline = null;
    this.bindGroup = null;
    
    this.gpu_warp = null;
    this.palette_overlay = null;
    this.bokeh_background = [0, 0, 0];
    this.bokeh_calibrating = false;
  }
	
	BuildBindGroup(index_tex_view) {
	  // Rebuild bind group because it references the old texture view
	  this.bindGroup = this.device.createBindGroup({
	    layout: this.pipeline.getBindGroupLayout(0),
	    entries: [
	      { binding: 0, resource: index_tex_view },
	      { binding: 1, resource: { buffer: this.uniform_buf } },
	      { binding: 2, resource: this.paletteTex.createView() },
	      //{ binding: 3, resource: this.paletteSampler },
	      { binding: 3, resource: this.gpu_warp.get_overlay_texture_view() },
	      { binding: 4, resource: this.gpu_warp.get_overlay_sampler() },
	    ],
	  });
	}	
		
  // cw, ch: client size
  // iw, ih: index buffer size (might be 2x smaller, 3x smaller, etc)
  resize(cw, ch, iw, ih, oversample) {
  	//console.log("WebGPUPresent::RESIZE: ", cw, ch, iw, ih, scale);
    this.cw = cw;
    this.ch = ch;
    this.iw = iw;
    this.ih = ih;
    this.oversample = oversample;
		//console.log(`over/undersample = ${oversample}, ${undersample}`);  //xxx

		if (this.device != null) {
			this.configure_canvas(this.use_hdr);

			this.gpu_warp.resize(iw, ih);
			
			this.BuildBindGroup(this.gpu_warp.get_current_index_view());
		}
  }

	// TODO: We can get rid of this now.
	updateGlobals(true_time = 0.0, shader_params, hdr_calib_params, 
                bkg_r = 1.0, bkg_g = 0.0, bkg_b = 0.0, hdr_display_headroom = 1.0) {					
		//console.log(`${this.gpu_warp}`);  null!
		/*let data = new Float32Array([
! 				// *** Be sure to update this in 2 places -- see below also.
! 	  		this.oversample, 1.0 / this.oversample, 0.0, 0.0,
! 	  		0, 0, 0, 0,
! 	  		this.cw, this.ch, 0, 0,
!         0, 0, 0, 0, 
!         0, 0, 0, 0, 
! 	  		]); // pad to 16 bytes
    */

    let r = null;
		if (this.gpu_warp) {
			r = this.gpu_warp.get_overlay_rect(true_time);
    }
	  let data = new Float32Array([
			// *** Be sure to update this in 2 places -- see above also.
  		this.oversample, 1.0 / this.oversample, 0.0, 0.0,
      // ----------- boundary ----------
  		r?.left_px   ?? 0.0, 
      r?.top_px    ?? 0.0, 
      r?.width_px  ?? this.cw, 
      r?.height_px ?? this.ch,
      // ----------- boundary ----------
  		this.cw, 
      this.ch, 
      r?.intensity ?? 0.0, 
      0.0,
      // ----------- boundary ----------
      /*
      shader_params?.diffusion_colorization_1 ?? 0.0, 
      shader_params?.diffusion_colorization_2 ?? 0.0, 
      shader_params?.diffusion_colorization_3 ?? 0.0, 
      shader_params?.diffusion_colorization_4 ?? 0.0, 
      // ----------- boundary ----------
      shader_params?.diffusion_colorization_5 ?? 0.0, 
      shader_params?.diffusion_colorization_6 ?? 0.0, 
      shader_params?.diffusion_colorization_7 ?? 0.0, 
      shader_params?.diffusion_colorization_8 ?? 0.0, 
      */
      // ----------- boundary ----------
      hdr_calib_params?.calibrating_hdr_headroom ?? 0.0,
      hdr_calib_params?.selected_square_index ?? 0.0,
      hdr_calib_params?.width_frac ?? 0.0,
      0.0,
      // ----------- boundary ----------
      0.0,
      0.0,      
      shader_params?.layer1_density ?? 0.0,
      shader_params?.layer2_brightness ?? 0.0,
      // ----------- boundary ----------
      bkg_r,
      bkg_g,
      bkg_b,
      hdr_display_headroom,
      // ----------- boundary ----------
		]); // pad to 16 bytes
	  this.device.queue.writeBuffer(this.uniform_buf, 0, data);
	}
	
	configure_canvas(attempt_hdr) {
		const preferred_format = navigator.gpu.getPreferredCanvasFormat();
		if (attempt_hdr) {
			try {
		    // HDR:    
		    this.format = "rgba16float";
		    this.context.configure({
		      device: this.device,
		      format: this.format,
		      alphaMode: "opaque",
		      // Allow output values > 1 on HDR displays (falls back effectively to SDR otherwise)
		      toneMapping: { mode: "extended" },
		      // Optional: nicer gamut on wide-gamut displays
		      // Note: On Chrome, this works, and we get HDR.
		      // On Safari, this fixes the "black screen" problem (if we try to do HDR),
		      // but... it still just shows as SDR, with no warning.
		      // NOTE: On Chrome, P3 color shows as a little richer.
		      //colorSpace: "display-p3",
		    });
		    return true;  //HDR
			} catch (e) {
				console.log(`ERROR: rgba16float texture format not supported -> falling back to ${preferred_format}.`);
			}		
		}
		
		// LDR:
		this.use_hdr = false;
	  this.format = preferred_format;
	  this.context.configure({
	    device: this.device,
	    format: this.format,
	    alphaMode: "opaque",
	  });
	  return false;  //SDR
	}

  async init(attempt_hdr) {		
		if (!this.cw || !this.ch) {
			throw new Error(`Presenter index size invalid: cw=${this.cw} ch=${this.ch}`);
		}
    if (!navigator.gpu) {
      throw new Error("WebGPU not supported in this browser.");
    }
    // Host adaptation: prefer hardware performance and expose the actual adapter.
    const adapter = await navigator.gpu.requestAdapter({powerPreference:"high-performance"});
    this.adapterInfo = adapter ? {vendor:adapter.info.vendor, architecture:adapter.info.architecture, device:adapter.info.device, description:adapter.info.description, fallback:adapter.isFallbackAdapter} : null;
    if (!adapter) throw new Error("No WebGPU adapter found.");
    this.device = await adapter.requestDevice();

    this.context = this.canvas.getContext("webgpu");
		
		
		



		this.use_hdr = this.configure_canvas(attempt_hdr);

		this.uniform_buf = this.device.createBuffer({
		  size: 256, // uniform alignment; each vec4f needs 16 bytes
		  usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
		});
		this.updateGlobals();

    // Palette texture (256x1 RGBA)
    this.paletteTex = this.device.createTexture({
      size: { width: 256, height: 1 },
      format: "rgba8unorm", //"rgba16float", //,
      usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
    });

		this.paletteSampler = this.device.createSampler({
		  addressModeU: "clamp-to-edge",
		  addressModeV: "clamp-to-edge",
      addressModeW: "clamp-to-edge",
		  magFilter: "linear",
		  minFilter: "linear",
		  mipmapFilter: "nearest", // no mips anyway for 256x1 unless you add them
		});

    const module = this.device.createShaderModule({ code: shaderWGSL });

    this.pipeline = this.device.createRenderPipeline({
      layout: "auto",
      vertex: { module, entryPoint: "vs_main" },
      fragment: {
        module,
        entryPoint: "fs_main",
        targets: [{ format: this.format }],
      },
      primitive: { topology: "triangle-list" },
    });

		// After this.pipeline is created:
		this.gpu_warp = new GPUWarp(this.device, this.iw, this.ih, kWarpWGSL);
		this.gpu_warp.init();
		
		// Upload initial index (your existing byte buffer)
		this.gpu_warp.fill_index_u8_with_noise();
		
		this.palette_overlay = new PaletteOverlayPass(this.device, this.format, this.paletteTex);
		
		// Build presenter bind group using GPUWarp's *current* texture view
		this.BuildBindGroup(this.gpu_warp.get_current_index_view());
  }

	SetWarpMap(slot, warp_map) {
		this.gpu_warp.upload_warp_map(slot, warp_map);
	}

  // Bokeh uses current linear HDR colors, independently of packed palette sampling.
  upload_audio_spectrum(spectrum) {
    this.gpu_warp.dynamic_warp.upload_spectrum(spectrum);
  }

  set_bokeh_frame(true_time, focal_distance, background, calibrating = false, opacity = 1.0) {
    this.bokeh_background = background;
    this.bokeh_calibrating = calibrating;
    this.gpu_warp.fading_dots.bokeh_dots.set_bokeh_frame({
      true_time, focal_distance, output_width: this.cw, output_height: this.ch,
      opacity
    });
  }

  add_bokeh_dots(points, true_time) {
    return this.gpu_warp.fading_dots.bokeh_dots.add_bokeh_dots(
      points, this.iw, this.ih, true_time);
  }

	uploadPaletteRGBA8UNorm(palette_rgba_f32) {
    this.gpu_warp.fading_dots.bokeh_dots.set_bokeh_palette(palette_rgba_f32);
	  // palette_rgba_f32: Float32Array (or Array-like) length 256*4
	  // Assumes this.paletteTex was created with format: "rgba8unorm"
	  // Expected float range: [0..8]

		// Leave headroom for HDR values > 1.
		// We'll map [0 .. kHeadroom] to [0..255].
		const kPaletteU8PackingHeadroom = 3.0;

	  // Pack values in a cheap gamma-space so we can get
	  // roughly as much detail in the highlights as we can
	  // in the shadows.
		const kPaletteU8PackingPower = 0.5;
	
	  const kWidth = 256;
	  const kHeight = 1;
	  const kChannels = 4;
	  const kTexelCount = kWidth * kChannels;   // 1024

	  const packed_u8 = new Uint8Array(kTexelCount);
	
	  for (let i = 0; i < kTexelCount; i++) {
	    let v = Math.max(0.0, Math.min(1.0, palette_rgba_f32[i] * (1.0 / kPaletteU8PackingHeadroom)));
			v = Math.pow(v, kPaletteU8PackingPower);
			v *= 255;

      // Round to nearest integer for slightly better quantization.
      packed_u8[i] = (v + 0.5) | 0;
	  }
	
	  this.device.queue.writeTexture(
	    { texture: this.paletteTex },
	    packed_u8,
	    { bytesPerRow: kWidth * 4, rowsPerImage: 1 }, // 4 channels * 1 byte = 4 bytes/texel
	    { width: kWidth, height: kHeight }
	  );
	}
	
	warpAndDrawWaveform(warp_strengths, shader_params, hdr_calib_params, 
                      shift_x, shift_y, waveform, darkening, true_time,
                      bkg_r, bkg_g, bkg_b, hdr_display_headroom) {
		this.updateGlobals(true_time, shader_params, hdr_calib_params, 
                       bkg_r, bkg_g, bkg_b, hdr_display_headroom);

    let warp_strengths_8 = new Float32Array(8).fill(0.0);
    for (let i = 0; i < Math.min(warp_strengths.length, 8); i++) {
      warp_strengths_8[i] = warp_strengths[i];
    }

		// Set warp weights (per-frame you can tweak these)
    //xxx - call this straight from engine.js, instead of double-plumbing it through webgpu_present.js.
		this.gpu_warp.set_params({ w0 : warp_strengths_8[0], 
                               w1 : warp_strengths_8[1], 
                               w2 : warp_strengths_8[2], 
                               w3 : warp_strengths_8[3],
                               w4 : warp_strengths_8[4], 
                               w5 : warp_strengths_8[5], 
                               w6 : warp_strengths_8[6], 
                               w7 : warp_strengths_8[7],
                               warp_scale: 1,
			                         shift_x: shift_x, shift_y: shift_y, 
			                         darkening : darkening,
															 true_time : true_time,
                               shader_params : shader_params,
                               oversample : this.oversample
                               });

		// 1) advance the simulation one step on GPU
		this.gpu_warp.step(waveform);

		// 2) rebuild presenter bind group so binding(0) points at the NEW current index texture
		this.BuildBindGroup(this.gpu_warp.get_current_index_view());
	}
			
  draw(palette_swatch_count = 0, bkg_rgb = null) {		
    const encoder = this.device.createCommandEncoder();
    const view = this.context.getCurrentTexture().createView();

    const pass = encoder.beginRenderPass({
      colorAttachments: [{
        view,
        clearValue: { r: 0, g: 0, b: 0, a: 1 },
        loadOp: "clear",
        storeOp: "store",
      }],
    });

    pass.setPipeline(this.pipeline);
    pass.setBindGroup(0, this.bindGroup);
    pass.draw(3);

    // Added after image colorization, before palette UI. No feedback-buffer writes.
    if (!this.bokeh_calibrating) {
      this.gpu_warp.fading_dots.bokeh_dots.draw_bokeh(pass, this.format, {
        width: this.cw, height: this.ch, background: this.bokeh_background,
      });
    }

		if (palette_swatch_count > 0) {
			this.palette_overlay.update_ui_uniforms(
        this.cw, this.ch, palette_swatch_count, palette_swatch_count * 32, 32, 8, 8, bkg_rgb);
		  this.palette_overlay.draw(pass);
		}

    pass.end();

    this.device.queue.submit([encoder.finish()]);
  }
}
