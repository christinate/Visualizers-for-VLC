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

// Dynamic warp-map generation. Each cache entry owns its output texture and
// uniforms; only compiled pipelines are shared between instances of a mode.
import { kWarpMapSize, kDynamicMotionParamCount } from "./const.js";

export const kAudioSpectrumSize = 512;

const kStaticNoise3dSize = 64;
const kStaticNoise3dSlices = 16;
const kStaticNoise2dSize = 256;
const kDynamicNoise2dSize = 64;

// One set per GPUDevice, shared even between multiple GpuDynamicWarp managers.
const g_shared_noise = new WeakMap();
const kNoiseWorkgroupSize = 8;

// Single source of truth for extra scalar uniform names and values.
const kNoiseUniforms = [
  ["k_static_noise_3d_size", kStaticNoise3dSize],
  ["k_static_noise_3d_slices", kStaticNoise3dSlices],
  ["k_static_noise_2d_size", kStaticNoise2dSize],
  ["k_dynamic_noise_2d_size", kDynamicNoise2dSize],
];




// 'pos' is in [-1..1] x [-1..1].  Warp maps are always square.
// Add new modes here; each supplies dynamic_warp(pos) returning relative XY
// displacement in the same normalized units as the static CPU maps.
const kDynamicModes = {
  35: `
fn dynamic_warp(pos: vec2f) -> vec2f {
  /*
    // original prototype of ripple: (2 passes)
    let fxy = uv * 2.0 - 1.0;
    let rad = length(fxy);
    let freq = select(21.3, 15.3, i==0);
    let phase = f32(u.true_time) * select(5.182, 4.573, i==0);
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * 0.002;    
    warp_vec1 += normalize(fxy) * displacement;
  */

  let rad = length(pos);
  var ret = vec2f(0.0);
  {
    let freq = 21.3 * pow(1.2, params.c01 + params.c02 - 1);
    let speed = 5.182 * (1.0 - 0.65 * params.c03);
    let phase = params.c04 * 6.28 + f32(params.true_time) * speed;
    let str = 0.002 * (1.0 - 0.5 * pow(params.c05, 2));
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * str;
    ret += normalize(pos + 0.0001) * displacement;
  }
  {
    let freq = 15.3 * pow(1.2, params.c06 + params.c07 - 1);
    let speed = 4.573 * (1.0 - 0.65 * params.c08);
    let phase = params.c09 * 6.28 + f32(params.true_time) * speed;
    let str = 0.002 * (1.0 - 0.5 * pow(params.c10, 2));
    let displacement = cos(rad * freq - phase) * saturate((rad - 0.05) * 5) * str;
    ret += normalize(pos + 0.0001) * displacement;
  }

  return ret;
}`,

  37: `    
  // 3d noise-driven velocity field
// Use this to push random values in [0..1] toward 0.5:
fn push_toward_half(t: f32) -> f32 {
  return t * (3.0 + t * (4.0 * t - 6.0));   // (inverse smoothstep)
}
fn dynamic_warp(pos: vec2f) -> vec2f {
  // Span only a few noise texels across the map for a smooth, coarse field.
  let cells = mix(3.0, 9.0, params.c01);
  let offset = vec3f(params.c02, params.c03, params.c04);
  let anim_speed = 0.4 + 0.5 * pow(params.c05, 2);
  let slice = params.true_time * anim_speed;
  //let str = pow(1.3, (params.c06 + params.c07 + params.c08 + params.c09) * (1.0 / 4.0) * 2.0 - 1.0);
  let str = pow(2.2, push_toward_half(push_toward_half(params.c06)) * 2.0 - 1.0) * 0.004;

  // 3d noise:
  let uvw = offset + vec3f(
      (pos * 0.5 + 0.5) * cells * (1.0 / params.k_static_noise_3d_size),  // XY
      slice * (1.0 / params.k_static_noise_3d_slices));                   // Z
  let velocity = textureSampleLevel(static_noise_3d, noise_sampler, uvw, 0.0).rg * 2.0 - 1.0;

  return velocity * str;
}`,

  38: `
// randomly-aligned sine waves
fn rotate(v: vec2f, theta: f32) -> vec2f {
  // theta is in radians.
  let c = cos(theta);
  let s = sin(theta);
  return vec2f(c * v.x - s * v.y, s * v.x + c * v.y);
}
fn dynamic_warp(pos: vec2f) -> vec2f {

  let rot_ang = params.c15 * 1.57;
  var pos2 = rotate(pos.xy, rot_ang);

  var ret = vec2f(0.0);

  var cycles = mix(1.0, 5.0, params.c02);
  var unjulation_speed = mix(0.3, 1.0, pow(params.c03, 2)) * 0.1;//0.25;
  var amplitude = mix(0.003, 0.012, params.c01) * 0.23;
  {
    let direction = select(-1.0, 1.0, params.c05 >= 0.5);
    let phase = 6.28318530718 * (cycles * pos2.y * 0.5
        - direction * unjulation_speed * params.true_time + params.c04);
    ret += vec2f(amplitude * sin(phase), 0.0);
  }
  {
    //let amplitude = mix(0.003, 0.018, pow(params.c06, 0.25)) * 0.3;
    //let cycles = mix(1.0, 5.0, params.c07);
    //let speed = mix(0.3, 1.4, params.c08) * 0.5;

    amplitude        *= pow(1.2, params.c06);
    unjulation_speed *= pow(1.2, params.c07);
    cycles           *= pow(1.2, params.c08);

    let direction = select(-1.0, 1.0, params.c10 >= 0.5);
    let phase = 6.28318530718 * (cycles * pos2.x * 0.5
        - direction * unjulation_speed * params.true_time + params.c09);
    ret += vec2f(0.0, amplitude * sin(phase));
  }

  ret = rotate(ret, -rot_ang);

  return ret;
}`,

  39: `
fn dynamic_warp(pos: vec2f) -> vec2f {
  var rad = length(pos);
  rad = 1.0 - rad;
  let spectrum_magnitude = sample_audio_spectrum(rad);

  var ret = vec2f(0.0);

  let attenuate = saturate(length(pos) * 30);

  //ret -= normalize(pos + 0.0001) * pow(spectrum_magnitude, 0.5) * 0.002 * attenuate;
  //ret -= normalize(pos.yx + 0.0001) * vec2f(-1.0, 1.0) * pow(spectrum_magnitude, 0.5) * 0.001 * attenuate;
  ret -= pos.yx * vec2f(-1.0, 1.0) * pow(spectrum_magnitude, 0.5) * 0.004 * attenuate;


  return ret;
}`,


};

// Scalar struct fields are contiguous. Pad the entire block to 16 bytes.
// With 16 parameters this is 24 floats / 96 bytes, including padding.
const kExtraUniformCount = 3 + kNoiseUniforms.length;
const kUniformFloatCount = Math.ceil((kDynamicMotionParamCount + kExtraUniformCount) / 4) * 4;
const kParamFields = Array.from({ length: kDynamicMotionParamCount }, (_, i) =>
  `  c${String(i + 1).padStart(2, "0")}: f32,`).join("\n");
const kPaddingFields = Array.from({
  length: kUniformFloatCount - kDynamicMotionParamCount - kExtraUniformCount,
}, (_, i) => `  padding${i}: f32,`).join("\n");
const kSharedWGSL = `
struct DynamicParams {
${kParamFields}
  true_time: f32,
  width: f32,
  height: f32,
${kNoiseUniforms.map(([name]) => `  ${name}: f32,`).join("\n")}
${kPaddingFields}
};
@group(0) @binding(0) var<uniform> params: DynamicParams;
@group(0) @binding(1) var static_noise_3d: texture_3d<f32>;
@group(0) @binding(2) var static_noise_2d: texture_2d<f32>;
@group(0) @binding(3) var dynamic_noise_2d: texture_2d<f32>;
@group(0) @binding(4) var noise_sampler: sampler;
@group(0) @binding(5) var audio_spectrum: texture_2d<f32>;
@group(0) @binding(6) var spectrum_sampler: sampler;

// t=0 is the first spectrum bin, t=1 the last; interpolate between bins.
// Values retain the caller's normalization/log scaling (no extra gain here).
fn sample_audio_spectrum(t: f32) -> f32 {
  let size = f32(textureDimensions(audio_spectrum).x);
  let uv = (clamp(t, 0.0, 1.0) * (size - 1.0) + 0.5) / size;
  return textureSampleLevel(audio_spectrum, spectrum_sampler, vec2f(uv, 0.5), 0.0).r;
}

@vertex fn vs_main(@builtin(vertex_index) vertex_index: u32) -> @builtin(position) vec4f {
  let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(positions[vertex_index], 0.0, 1.0);
}

@fragment fn fs_main(@builtin(position) pixel: vec4f) -> @location(0) vec2f {
  // The consuming warp shader already adds a half-texel offset. Match the
  // static map lattice: texel (i,j) represents (2*i/width-1, 2*j/height-1).
  let pos = (pixel.xy - vec2f(0.5)) * 2.0 / vec2f(params.width, params.height) - vec2f(1.0);
  return dynamic_warp(pos);
}
`;

// Procedural GPU noise: independent hashes for RGBA, converted to half-float
// by textureStore. Clamp below 1 so half-float rounding cannot produce 1.0.
const kNoiseHashWGSL = `
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
`;

function create_noise_pipeline(device, dimension) {
  const is_3d = dimension === "3d";
  const module = device.createShaderModule({
    label: `Generate ${dimension} RGBA noise`,
    code: kNoiseHashWGSL + `
@group(0) @binding(0) var output_noise: texture_storage_${dimension}<rgba16float, write>;
@compute @workgroup_size(${kNoiseWorkgroupSize}, ${kNoiseWorkgroupSize}, 1)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
  let size = textureDimensions(output_noise);
  if (any(id.${is_3d ? "xyz" : "xy"} >= size)) { return; }
  let index = ${is_3d ? "(id.z * size.y + id.y) * size.x + id.x" : "id.y * size.x + id.x"};
  textureStore(output_noise, vec${is_3d ? "3" : "2"}i(id.${is_3d ? "xyz" : "xy"}), noise_rgba(index));
}
`,
  });
  return device.createComputePipeline({
    label: `Generate ${dimension} RGBA noise`, layout: "auto",
    compute: { module, entryPoint: "cs_main" },
  });
}

function create_noise_texture(device, pipeline, label, dimension, width, height, depth = 1) {
  const texture = device.createTexture({
    label, dimension, size: { width, height, depthOrArrayLayers: depth },
    format: "rgba16float",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.STORAGE_BINDING,
  });
  const view = texture.createView({ dimension });
  const seed_buf = device.createBuffer({
    label: `${label} seed`, size: 16,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  });
  const bind_group = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [{ binding: 0, resource: view }, { binding: 1, resource: { buffer: seed_buf } }],
  });
  return { texture, view, seed_buf, bind_group, pipeline, width, height, depth,
    seed_data: new Uint32Array(4) };
}

function encode_noise(device, encoder, noise, seed) {
  noise.seed_data[0] = seed;
  device.queue.writeBuffer(noise.seed_buf, 0, noise.seed_data);
  const pass = encoder.beginComputePass({ label: "Generate shared noise" });
  pass.setPipeline(noise.pipeline);
  pass.setBindGroup(0, noise.bind_group);
  pass.dispatchWorkgroups(Math.ceil(noise.width / kNoiseWorkgroupSize),
    Math.ceil(noise.height / kNoiseWorkgroupSize), noise.depth);
  pass.end();
}

function get_shared_noise(device) {
  let shared = g_shared_noise.get(device);
  if (shared) return shared;
  const pipeline_2d = create_noise_pipeline(device, "2d");
  const pipeline_3d = create_noise_pipeline(device, "3d");
  const static_noise_3d = create_noise_texture(device, pipeline_3d, "static_noise_3d", "3d",
    kStaticNoise3dSize, kStaticNoise3dSize, kStaticNoise3dSlices);
  const static_noise_2d = create_noise_texture(device, pipeline_2d, "static_noise_2d", "2d",
    kStaticNoise2dSize, kStaticNoise2dSize);
  const dynamic_noise_2d = create_noise_texture(device, pipeline_2d, "dynamic_noise_2d", "2d",
    kDynamicNoise2dSize, kDynamicNoise2dSize);
  const sampler = device.createSampler({
    label: "Wrapping linear noise sampler",
    addressModeU: "repeat", addressModeV: "repeat", addressModeW: "repeat",
    minFilter: "linear", magFilter: "linear", mipmapFilter: "nearest",
  });
  const spectrum_texture = device.createTexture({
    label: "Shared audio spectrum", dimension: "2d",
    size: [kAudioSpectrumSize, 1, 1], format: "r16float",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
  });
  const spectrum_view = spectrum_texture.createView();
  const spectrum_sampler = device.createSampler({
    label: "Clamped linear audio spectrum sampler",
    addressModeU: "clamp-to-edge", minFilter: "linear", magFilter: "linear",
  });
  const spectrum_half = new Uint16Array(kAudioSpectrumSize);
  const spectrum_f32 = new Float32Array(kAudioSpectrumSize);
  const spectrum_bits = new Uint32Array(spectrum_f32.buffer);
  // Silence until the first audio frame arrives.
  device.queue.writeTexture({texture: spectrum_texture}, spectrum_half, {},
    [kAudioSpectrumSize, 1, 1]);
  // Explicit layout keeps all bindings available even when a mode uses none.
  const bind_group_layout = device.createBindGroupLayout({ entries: [
    { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } },
    { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float", viewDimension: "3d" } },
    { binding: 2, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float", viewDimension: "2d" } },
    { binding: 3, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float", viewDimension: "2d" } },
    { binding: 4, visibility: GPUShaderStage.FRAGMENT, sampler: { type: "filtering" } },
    { binding: 5, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: "float", viewDimension: "2d" } },
    { binding: 6, visibility: GPUShaderStage.FRAGMENT, sampler: { type: "filtering" } },
  ] });
  const pipeline_layout = device.createPipelineLayout({ bindGroupLayouts: [bind_group_layout] });
  shared = { spectrum_texture, spectrum_view, spectrum_sampler, spectrum_half, spectrum_f32, spectrum_bits,
    static_noise_3d, static_noise_2d, dynamic_noise_2d, sampler,
    bind_group_layout, pipeline_layout, encoded_frames: new WeakSet(),
    seed: (Math.random() * 4294967296) >>> 0 };
  const encoder = device.createCommandEncoder({ label: "Initialize shared noise" });
  encode_noise(device, encoder, static_noise_3d, shared.seed ^ 0xa511e9b3);
  encode_noise(device, encoder, static_noise_2d, shared.seed ^ 0x63d83595);
  encode_noise(device, encoder, dynamic_noise_2d, shared.seed);
  device.queue.submit([encoder.finish()]);
  g_shared_noise.set(device, shared);
  return shared;
}

export class GpuDynamicWarp {
  constructor(device) {
    this.device = device;
    this.pipelines = new Map();
    this.noise = get_shared_noise(device);
  }

  // One 1024-byte upload per engine frame, shared by all dynamic warp maps.
  // Reuse staging arrays; round float32 to float16, including subnormals.
  upload_spectrum(spectrum) {
    if (spectrum.length !== kAudioSpectrumSize) {
      throw new RangeError(`Expected ${kAudioSpectrumSize} spectrum values`);
    }
    const {spectrum_f32, spectrum_bits, spectrum_half, spectrum_texture} = this.noise;
    for (let i = 0; i < kAudioSpectrumSize; i++) {
      const value = spectrum[i];
      spectrum_f32[i] = Number.isFinite(value) ? Math.max(-65504, Math.min(65504, value)) : 0;
      const bits = spectrum_bits[i];
      const sign = (bits >>> 16) & 0x8000;
      const exponent = ((bits >>> 23) & 255) - 127;
      const mantissa = bits & 0x7fffff;
      if (exponent < -25) {
        spectrum_half[i] = sign;
      } else {
        const shift = exponent < -14 ? -exponent - 1 : 13;
        const significand = exponent < -14 ? mantissa | 0x800000 : mantissa;
        let rounded = significand >>> shift;
        const remainder = significand & ((1 << shift) - 1);
        const halfway = 1 << (shift - 1);
        if (remainder > halfway || (remainder === halfway && (rounded & 1))) rounded++;
        spectrum_half[i] = sign | ((exponent < -14 ? 0 : (exponent + 15) << 10) + rounded);
      }
    }
    this.device.queue.writeTexture({texture: spectrum_texture}, spectrum_half, {},
      [kAudioSpectrumSize, 1, 1]);
  }

  get_pipeline(mode) {
    if (this.pipelines.has(mode)) return this.pipelines.get(mode);
    const formula = kDynamicModes[mode];
    if (!formula) throw new Error(`No dynamic warp shader for mode ${mode}`);
    const module = this.device.createShaderModule({
      label: `Dynamic warp mode ${mode}`, code: kSharedWGSL + formula,
    });
    const pipeline = this.device.createRenderPipeline({
      label: `Dynamic warp mode ${mode}`, layout: this.noise.pipeline_layout,
      vertex: { module, entryPoint: "vs_main" },
      fragment: { module, entryPoint: "fs_main", targets: [{ format: "rg16float" }] },
      primitive: { topology: "triangle-list" },
    });
    this.pipelines.set(mode, pipeline);
    return pipeline;
  }

  create_state(mode, motion_params) {
    if (motion_params.length !== kDynamicMotionParamCount) {
      throw new Error(`Dynamic mode ${mode}: expected ${kDynamicMotionParamCount} parameters`);
    }
    const pipeline = this.get_pipeline(mode);
    const uniform_buf = this.device.createBuffer({
      label: `Dynamic warp ${mode} instance uniforms`, size: kUniformFloatCount * 4,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const bind_group = this.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: { buffer: uniform_buf } },
        { binding: 1, resource: this.noise.static_noise_3d.view },
        { binding: 2, resource: this.noise.static_noise_2d.view },
        { binding: 3, resource: this.noise.dynamic_noise_2d.view },
        { binding: 4, resource: this.noise.sampler },
        { binding: 5, resource: this.noise.spectrum_view },
        { binding: 6, resource: this.noise.spectrum_sampler },
      ],
    });
    return { pipeline, uniform_buf, bind_group, motion_params,
      data: new Float32Array(kUniformFloatCount) };
  }

  // GPUWarp uses one command encoder per frame. This also works while time is
  // paused, and across multiple managers sharing the same device and encoder.
  // No dynamic-warp draw means no noise refresh is needed for that frame.
  begin_frame(encoder) {
    const shared = this.noise;
    if (shared.encoded_frames.has(encoder)) return;
    shared.encoded_frames.add(encoder);
    shared.seed = (shared.seed + 1) >>> 0;
    encode_noise(this.device, encoder, shared.dynamic_noise_2d, shared.seed);
  }

  encode(encoder, view, state, true_time) {
    this.begin_frame(encoder);
    const { data, motion_params, uniform_buf, pipeline, bind_group } = state;
    data.set(motion_params);
    data[kDynamicMotionParamCount] = true_time;
    data[kDynamicMotionParamCount + 1] = kWarpMapSize;
    data[kDynamicMotionParamCount + 2] = kWarpMapSize;
    for (let i = 0; i < kNoiseUniforms.length; i++) {
      data[kDynamicMotionParamCount + 3 + i] = kNoiseUniforms[i][1];
    }
    this.device.queue.writeBuffer(uniform_buf, 0, data);
    const pass = encoder.beginRenderPass({
      label: "Generate dynamic warp map",
      colorAttachments: [{ view, loadOp: "clear", storeOp: "store",
        clearValue: { r: 0, g: 0, b: 0, a: 0 } }],
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group);
    pass.draw(3);
    pass.end();
  }
}

