/* Image-inspired palettes for Geiss HDR. No dependencies; all output is LINEAR
 * sRGB, not gamma-encoded sRGB. Browser color management converts decoded input
 * to sRGB through Canvas2D before analysis (including tagged wide-gamut images).
 *
 * const colors = await GeneratePaletteFromImage(file);
 * // Eight [r,g,b] tuples. Stop 0 is black; Y strictly increases to 0.85.
 * // Rerun for another palette with randomly selected smoothness.
 * // Optional: { smoothness: 1 } reproduces the original primary/accent style;
 * // { smoothness: 0 } allows a different image-derived hue at every stop.
 *
 * Main.js integration (see the accompanying response): insert the synchronous
 * HandleImagePaletteDrop(e) check BEFORE awaiting collect_dropped_audio_files().
 * It handles direct image files, not images nested in dropped folders or URLs.
 * First image wins for a multi-image drop; a mixed direct audio/image drop goes
 * to the existing audio handler. Unsupported image codecs report a decode error.
 */

// Tune how smooth you want the (craziest) generated palettes to be here, from [0..1].
const kMinPaletteSmoothness = 0.35;

export const kImagePaletteDefaults = Object.freeze({
  sample_size: 128,       // longest side of the downsampled image
  cluster_count: 12,
  cluster_iterations: 10,
  highlight_luminance: 0.85,
  smoothness: null,     // null = random each palette; otherwise a number in [0,1]
  random: Math.random,   // optionally pass your seeded generator
});

const luminance = rgb => 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
const lerp = (a, b, t) => a + (b - a) * t;
const smoothstep = t => t * t * (3 - 2 * t);
const distance2 = (a, b) => (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2;
const srgb_to_linear = v => v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4;

function linear_rgb_to_oklab([r, g, b]) {
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [
    0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
  ];
}

function oklab_to_linear_rgb(L, a, b) {
  const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3;
  const m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3;
  const s = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3;
  return [
     4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
    -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
  ];
}

function get_options(options) {
  const p = { ...kImagePaletteDefaults, ...options };
  for (const name of ["sample_size", "cluster_count", "cluster_iterations"]) {
    if (!Number.isInteger(p[name]) || p[name] < 1) throw new RangeError(`${name} must be a positive integer`);
  }
  if (p.sample_size > 512 || p.cluster_count > 32 || p.cluster_iterations > 50) {
    throw new RangeError("Use sample_size <= 512, cluster_count <= 32, cluster_iterations <= 50");
  }
  if (!Number.isFinite(p.highlight_luminance) || p.highlight_luminance <= 0 || p.highlight_luminance >= 1) {
    throw new RangeError("highlight_luminance must be between 0 and 1, exclusively");
  }
  if (typeof p.random !== "function") throw new TypeError("random must be a function");
  if (p.smoothness !== null &&
      (!Number.isFinite(p.smoothness) || p.smoothness < 0 || p.smoothness > 1)) {
    throw new RangeError("smoothness must be null or a number in [0,1]");
  }
  const random = p.random;
  p.random = () => {
    const r = random();
    if (!Number.isFinite(r) || r < 0 || r >= 1) throw new RangeError("random() must return [0,1)");
    return r;
  };
  return p;
}

function weighted_index(weights, random) {
  const sum = weights.reduce((a, b) => a + b, 0);
  if (!(sum > 0)) return 0;
  let target = random() * sum;
  for (let i = 0; i < weights.length; i++) {
    target -= weights[i];
    if (target < 0) return i;
  }
  return weights.length - 1;
}

// Weighted k-means++ initialization, followed by a few ordinary k-means passes.
// Alpha supplies statistical weight; transparent padding never acts as black.
function cluster_colors(samples, p) {
  const weights = samples.map(s => s[3]);
  const centers = [samples[weighted_index(weights, p.random)].slice(0, 3)];
  const nearest = new Float64Array(samples.length).fill(Infinity);
  while (centers.length < Math.min(p.cluster_count, samples.length)) {
    const last = centers[centers.length - 1];
    const pick_weights = samples.map((s, i) => {
      nearest[i] = Math.min(nearest[i], distance2(s, last));
      return nearest[i] * s[3];
    });
    if (pick_weights.reduce((a, b) => a + b, 0) < 1e-12) break;
    centers.push(samples[weighted_index(pick_weights, p.random)].slice(0, 3));
  }
  let populations;
  for (let iteration = 0; iteration < p.cluster_iterations; iteration++) {
    const sums = centers.map(() => [0, 0, 0, 0]);
    for (const s of samples) {
      let best = 0, best_d = Infinity;
      for (let i = 0; i < centers.length; i++) {
        const d = distance2(s, centers[i]);
        if (d < best_d) { best = i; best_d = d; }
      }
      const sum = sums[best];
      for (let j = 0; j < 3; j++) sum[j] += s[j] * s[3];
      sum[3] += s[3];
    }
    populations = sums.map(s => s[3]);
    for (let i = 0; i < centers.length; i++) {
      if (populations[i] > 0) centers[i] = sums[i].slice(0, 3).map(v => v / populations[i]);
    }
  }
  return centers.map((lab, i) => ({ lab, weight: populations[i] }))
      .filter(c => c.weight > 0);
}

// Preserve requested LINEAR luminance while reducing Oklab chroma until RGB is
// in gamut. This avoids independent component clipping and preserves hue better.
function color_at_luminance(Y, a, b) {
  const L = Math.cbrt(Y);
  function candidate(scale) {
    const rgb = oklab_to_linear_rgb(L, a * scale, b * scale);
    const current_Y = luminance(rgb);
    if (!(current_Y > 0)) return null;
    const out = rgb.map(v => v * Y / current_Y);
    return out.every(v => Number.isFinite(v) && v >= 0 && v <= 1) ? out : null;
  }
  const full = candidate(1);
  if (full) return full;
  let low = 0, high = 1, best = [Y, Y, Y];
  for (let i = 0; i < 24; i++) {
    const mid = (low + high) * 0.5;
    const rgb = candidate(mid);
    if (rgb) { low = mid; best = rgb; } else { high = mid; }
  }
  return best;
}

function palette_from_clusters(clusters, p) {
  const chroma = c => Math.hypot(c.lab[1], c.lab[2]);
  // Temper population dominance, mildly favor saturation, and de-emphasize
  // almost-black/almost-white clusters. All are heuristic artistic choices.
  const primary_weights = clusters.map(c =>
    c.weight ** 0.65 * (0.025 + chroma(c)) ** 1.15 *
    (0.1 + Math.sin(Math.PI * Math.min(1, Math.max(0, c.lab[0]))) ** 2));
  const primary = clusters[weighted_index(primary_weights, p.random)];
  const cp = chroma(primary);
  const hp = Math.atan2(primary.lab[2], primary.lab[1]);
  const accent_weights = clusters.map(c => {
    const C = chroma(c);
    if (c === primary || C < 0.015 || cp < 0.015) return 0;
    const h = Math.atan2(c.lab[2], c.lab[1]);
    const angle = Math.abs(Math.atan2(Math.sin(h - hp), Math.cos(h - hp)));
    // Prefer an image-derived accent 30-155 degrees away. Avoid nearly opposite
    // pairs, whose interpolated middle readily becomes muddy/desaturated.
    if (angle < Math.PI / 6 || angle > Math.PI * 155 / 180) return 0;
    return c.weight ** 0.55 * C * Math.sin(angle * 0.5);
  });
  const accent = accent_weights.some(w => w > 0)
      ? clusters[weighted_index(accent_weights, p.random)] : primary;

  const strength = accent === primary ? 0 : 0.4 + 0.45 * p.random();
  const center = 0.58 + 0.14 * p.random();
  const width = 0.19 + 0.05 * p.random();
  const exposure_curve = 1.55 + 0.35 * p.random();
  const saturation = 0.95 + 0.25 * p.random();
  // Draw AFTER the original algorithm's random choices, so explicit smoothness
  // 1 gives exactly the old result, even with the same seeded random generator.
  const smoothness = p.smoothness ?? (kMinPaletteSmoothness + (1.0 - kMinPaletteSmoothness) * p.random());
  const chaos = 1 - smoothness;
  const hue_knots = [];
  if (chaos > 0) {
    const knot_count = Math.round(2 + 5 * chaos);
    const used = new Set();
    let previous = primary;
    for (let i = 0; i < knot_count; i++) {
      if (used.size === clusters.length) used.clear();
      // Sample actual image clusters, preferring a change from the previous
      // hue and avoiding repeats until every cluster has had a chance.
      const weights = clusters.map((c, j) => {
        if (used.has(j)) return 0;
        const C = chroma(c), P = chroma(previous);
        const cosine = C > 0.015 && P > 0.015
          ? (c.lab[1] * previous.lab[1] + c.lab[2] * previous.lab[2]) / (C * P) : 1;
        const difference = 0.5 * (1 - Math.max(-1, Math.min(1, cosine)));
        return primary_weights[j] * (0.15 + difference);
      });
      const index = weighted_index(weights, p.random);
      used.add(index);
      previous = clusters[index];
      hue_knots.push(previous.lab);
    }
  }
  const colors = [[0, 0, 0]];
  for (let i = 1; i < 8; i++) {
    const t = i / 7;
    const Y = p.highlight_luminance * t ** exposure_curve;
    const accent_mix = strength * Math.exp(-0.5 * ((t - center) / width) ** 2);
    // A gentle chroma taper makes the high-luminance endpoint a tinted pale
    // highlight. It also leaves more room for hue at intermediate brightness.
    const highlight_taper = 1 - 0.65 * smoothstep(Math.max(0, (t - 0.65) / 0.35));
    let a = lerp(primary.lab[1], accent.lab[1], accent_mix);
    let b = lerp(primary.lab[2], accent.lab[2], accent_mix);
    if (chaos > 0) {
      // Spread 2..7 image colors over the seven nonblack stops. At zero
      // smoothness each stop lands on its own knot; no primary/accent bias.
      const position = (i - 1) / 6 * (hue_knots.length - 1);
      const left = Math.min(Math.floor(position), hue_knots.length - 2);
      const mix = smoothstep(position - left);
      a = lerp(a, lerp(hue_knots[left][1], hue_knots[left + 1][1], mix), chaos);
      b = lerp(b, lerp(hue_knots[left][2], hue_knots[left + 1][2], mix), chaos);
    }
    a = a * saturation * highlight_taper;
    b = b * saturation * highlight_taper;
    colors.push(color_at_luminance(Y, a, b));
  }
  return colors;
}

/** Synchronous extraction from sRGB RGBA8 ImageData (also useful for tests).
 * Pass small/downsampled ImageData. Result is Array(8) of LINEAR [r,g,b] arrays.
 */
export function GeneratePaletteFromImageData(image_data, options = {}) {
  const p = get_options(options);
  const { data, width, height } = image_data;
  if (!(data instanceof Uint8ClampedArray || data instanceof Uint8Array) ||
      !Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 ||
      data.length !== width * height * 4) {
    throw new TypeError("Expected sRGB RGBA8 ImageData");
  }
  const samples = [];
  for (let i = 0; i < data.length; i += 4) {
    const alpha = data[i + 3] / 255;
    if (alpha < 0.05) continue;
    const rgb = [0, 1, 2].map(j => srgb_to_linear(data[i + j] / 255));
    samples.push([...linear_rgb_to_oklab(rgb), alpha]);
  }
  if (!samples.length) throw new Error("The image contains no visible pixels");
  return palette_from_clusters(cluster_colors(samples, p), p);
}

async function decode_image(file) {
  if (typeof createImageBitmap === "function") {
    try {
      const bitmap = await createImageBitmap(file);
      return { source: bitmap, width: bitmap.width, height: bitmap.height, close: () => bitmap.close() };
    } catch { /* Some browser codecs decode via HTMLImageElement instead. */ }
  }
  const url = URL.createObjectURL(file);
  try {
    const image = new Image();
    await new Promise((resolve, reject) => {
      image.onload = resolve;
      image.onerror = () => reject(new Error(`Could not decode image: ${file.name || "image"}`));
      image.src = url;
    });
    return { source: image, width: image.naturalWidth, height: image.naturalHeight,
      close: () => URL.revokeObjectURL(url) };
  } catch (error) {
    URL.revokeObjectURL(url);
    throw error;
  }
}

/** Decode a browser File/Blob and return eight linear-sRGB tuples.
 * Image pixels stay local. No uploads, services, or external libraries.
 */
export async function GeneratePaletteFromImage(file, options = {}) {
  if (!(file instanceof Blob)) throw new TypeError("Expected an image File or Blob");
  const p = get_options(options);
  const image = await decode_image(file);
  try {
    if (!image.width || !image.height) throw new Error("Image has no dimensions");
    const scale = Math.min(1, p.sample_size / Math.max(image.width, image.height));
    const canvas = document.createElement("canvas");
    canvas.width = Math.max(1, Math.round(image.width * scale));
    canvas.height = Math.max(1, Math.round(image.height * scale));
    const ctx = canvas.getContext("2d", { colorSpace: "srgb", willReadFrequently: true });
    if (!ctx) throw new Error("Canvas2D is unavailable");
    ctx.drawImage(image.source, 0, 0, canvas.width, canvas.height);
    const pixels = ctx.getImageData(0, 0, canvas.width, canvas.height);
    return GeneratePaletteFromImageData(pixels, p);
  } finally { image.close(); }
}

/** Snapshot the dropped File synchronously, before async decoding releases the
 * browser's drop data store. Leave ordinary audio and folder drops untouched.
 */
function dropped_image(data_transfer) {
  if (!data_transfer) return null;
  const files = Array.from(data_transfer.files || []);
  const audio = f => f.type.startsWith("audio/") || /\.(mp3|wav|m4a|aac|ogg|flac|opus|wma)$/i.test(f.name);
  if (files.some(audio)) return null;
  // Do not intercept a directory drop even if it has an image-like filename.
  const items = Array.from(data_transfer.items || []);
  if (items.some(item => item.webkitGetAsEntry?.()?.isDirectory)) return null;
  return files.find(f => f.type.startsWith("image/") ||
    /\.(png|jpe?g|webp|avif|gif|bmp|svg|ico|heic|heif)$/i.test(f.name)) || null;
}

/** Returns a SYNCHRONOUS (event) => boolean handler for your existing drop
 * listener. True means this drop was consumed. Do NOT await this boolean.
 * Async decode/extraction errors go to on_error; newest image drop wins.
 * on_palette receives (colors, file). You hook that up to your preset yourself.
 */
export function CreateImagePaletteDropHandler(on_palette, options = {}) {
  if (typeof on_palette !== "function") throw new TypeError("Expected on_palette callback");
  const { on_error = error => console.error("Image palette:", error), ...palette_options } = options;
  if (typeof on_error !== "function") throw new TypeError("Expected on_error callback");
  let request_id = 0;
  return function HandleImagePaletteDrop(e) {
    const file = dropped_image(e.dataTransfer);
    if (!file) return false;
    e.preventDefault();
    const id = ++request_id;
    GeneratePaletteFromImage(file, palette_options)
      .then(colors => id === request_id ? on_palette(colors, file) : undefined)
      .catch(error => { if (id === request_id) on_error(error); });
    return true;
  };
}
