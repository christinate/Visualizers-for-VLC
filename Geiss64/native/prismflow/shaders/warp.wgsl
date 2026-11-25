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

struct ExposureOut {
  decay : f32,
  _pad0 : f32,
  _pad1 : f32,
  _pad2 : f32,
};

@group(0) @binding(0) var old_tex : texture_2d<f32>;        // rgba16float view (sampleType float)
@group(0) @binding(1) var warp0   : texture_2d<f32>;        // rg16float
@group(0) @binding(2) var warp1   : texture_2d<f32>;
@group(0) @binding(3) var warp2   : texture_2d<f32>;
@group(0) @binding(4) var warp3   : texture_2d<f32>;
@group(0) @binding(5) var warp4   : texture_2d<f32>;
@group(0) @binding(6) var warp5   : texture_2d<f32>;
@group(0) @binding(7) var warp6   : texture_2d<f32>;
@group(0) @binding(8) var warp7   : texture_2d<f32>;
@group(0) @binding(9) var samp    : sampler;               // filtering sampler
@group(0) @binding(10) var<uniform> u : WarpUniforms;
@group(0) @binding(11) var noise_tex : texture_2d<f32>;      // r8unorm, loaded via textureLoad
@group(0) @binding(12) var<storage, read> exposure : ExposureOut;
@group(0) @binding(13) var index_samp : sampler; // linear, repeat on both axes

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
  var pos = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(-1.0,  3.0),
    vec2f( 3.0, -1.0)
  );
  let p = pos[vid];
  return vec4f(p, 0.0, 1.0);
}

// p.xy : the output coordinates in [0..index_width - 1, 0 .. index_height - 1]
@fragment
fn fs_main(@builtin(position) p: vec4f) -> @location(0) vec4f {

  let inv_size = vec2f(u.index_width_inv, u.index_height_inv);
  let base_pos = p.xy + vec2f(u.shift_x, u.shift_y);
  let weights0123 = vec4f(u.w0, u.w1, u.w2, u.w3);
  let weights4567 = vec4f(u.w4, u.w5, u.w6, u.w7);
  var warp_vec1 = warp_displacement_pixels(p.xy, vec4f(1.0), vec4f(1.0));



  // Painterly:
  if (u.painterly_str > 0.0) {
    let sample0 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
    let vel_scale = 1.0 + ((sample0.r - sample0.b) * 0.2 - 0.05) * (u.painterly_str * 3);
    warp_vec1 *= vel_scale;

    //xxx - this alternative was pretty cool:
    //warp_vec1 += warp_vec1.yx * vec2f(-1.0, 1.0) * (vel_scale - 1);

    //xxx - this alternative was pretty cool:
    //let t = vel_scale - 1;
    //warp_vec1 = warp_vec1.xy * (1.0 - t) + warp_vec1.yx * vec2f(-1.0, 1.0) * t;

    //xxx - this alternative was pretty cool:
    //let t = (vel_scale - 1) * 2;
    //warp_vec1 = warp_vec1.xy * (1.0 - t) - warp_vec1.xy * t;
  }

  // Do 4-channel noise lookup.
  let nx = (u32(p.x * u.oversample_inv) + u32(u.noise_offset_x)) & (256u - 1u);
  let ny = (u32(p.y * u.oversample_inv) + u32(u.noise_offset_y)) & (256u - 1u);
  let n01 = textureLoad(noise_tex, vec2u(nx, ny), 0);  // [0,1]
  let noise = n01 * 2.0 - 1.0;                               // [-1,1]


  // If the displacement is significantly less than 1 pixel, add noise, so that you
  // at least get a tiny bit of softening -- we don't want those regions to remain
  // oddly crisp.
  const min_warp_displacement_pixel_frac = 7;   // 5 ~ 1/5th of a pixel
  let warp_noise = 1.0 - saturate(length(warp_vec1) * min_warp_displacement_pixel_frac);
  warp_vec1 += noise.zw * warp_noise * 0.1;



  var sample1 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
  //var sample3 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1) * inv_size, 0.0);
  
  // Apply a little extra softening, over time, on some channels.
  let kDist = 1.33 * u.oversample;    // [1 .. 1.5]   ~ determines the wavelength (size/spacing) of the stripes.  Use 1.33 for either.
  let softer = (     
      // Use 4 queens pattern:
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f( 1.5, -0.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f(-1.5,  0.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f( 0.5,  1.5) * kDist)) * inv_size, 0.0) +
      textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 + (vec2f(-0.5, -1.5) * kDist)) * inv_size, 0.0)) * 0.25;

  // For Barkley diffusion: weaker blur -> smaller AND slower spirals
  // NOTE: Softening on 4th channel is ignored right now -- overridden below...
  const kRgbSoften = vec4f(0.0, 0.05, 1.0, 0.0);// 0.7;//0.05;//0.7;        // [0..1]    Use 0.05 for Barkley (~G).  Use 0.7 for grey-scott (~B).
  var values = sample1 * (vec4f(1.0,1.0,1.0,1.0) - kRgbSoften) + kRgbSoften * softer;
  
  // Alpha channel has no softening, and instead, warps at a different speed.
  var sample2 = textureSampleLevel(old_tex, index_samp, (base_pos + warp_vec1 * u.layer2_motion_scale) * inv_size, 0.0);
  values.w = sample2.w;




  // Soft frequency injector:
  if (u.frequency_injector_str > 0.0) {
    // Note: Here we scale the adjustment intensity by 1/~oversample, to help reduce
    // the overzealous (contrasty) impact @ higher oversamples (if not adjusted).
    values.xy += sin((values.x - values.y) * (3.8 * u.frequency_injector_freq_scale)) * (0.0085 * u.frequency_injector_str * (1 * 0.15 + 0.85 * u.oversample_inv));
    values.xyz = saturate(values.xyz);
  }      
  


  
  // Add some grey-scott reaction diffusion.
  if (u.rx_diff_grey_scott_str > 0.0)
  {
    // G: your weighted Gaussian samples of the old G channel.
    let R = values.r;
    let G = values.b;
    
    const feed = 0.035;
    const kill = 0.059;//0.06;//0.05;//0.05;//0.062;    // 0.06 ~ little dots; 0.05 ~ stripes
    let dt = 1.0 * u.rx_diff_grey_scott_str;
    
    // Calculate from the old values before updating either channel.
    let reaction = G * R * R;
    
    let next_R = R + dt * (
        reaction - (feed + kill) * R
    );
    
    let next_G = G + dt * (
        -reaction + feed * (1.0 - G)
    );
    
    values.r = saturate(next_R);
    values.b = saturate(next_G);
  }


  
  // Add Barkley reaction-diffusion.  Note that this wants the G channel blur (above) to be much slower/milder,
  // otherwise the spirals are huge and move fast.
  /*if (u.rx_diff_barkley_str > 0.0) {
    let R = values.g;  // excitation
    let G = values.r;  // recovery
    
    const a = 0.75;
    const b = 0.02;
    const epsilon = 0.02;
    let dt = 0.03;//0.04 * u.rx_diff_barkley_str;//0.01;   // smaller timestep -> smaller AND slower spirals
    
    let threshold = (G + b) * (1.0 / a);
    let dR = R * (1.0 - R) * (R - threshold) * (1.0 / epsilon);
    let dG = R - G;
    
    values.g = saturate(R + dt * dR);
    values.r = saturate(G + dt * dG);    
  }*/







	// Add noise.
  // Tiny unfiltered noise (textureLoad). Wrap by & (size-1) since size is 256.
  // p.xy are pixel coords in the output render target space.
  // Note that we divide by the oversample factor so that the visual noise
  //   is at the frequency of the display, rather than the canvas.
  //
  // Sprinkle: pick a *tiny* scale. Tune later.
  // Multiplicative noise:  (affects all brightnesses equally)
  //let kNoiseStrength = 1.0 + u.noise_str * 0.03; //1.015;
	//values *= pow(kNoiseStrength, noise);
  // Additive noise:  (nice because it affects deep shadows only)
  // Total noise scale reference:
  //   0 ~ smooth.  0.005 ~ barely visible.  0.01 ~ nice, subtle noise grain.  0.03 ~ very strong, bold noise grain.
  //xxx - TODO: Experiment with using noise.rgba.
  values += noise.r * u.noise_str * 0.022;


	let decay = exposure.decay;
	values *= decay;	
  
  return clamp(values, vec4f(0.0), vec4f(1.0));
}
