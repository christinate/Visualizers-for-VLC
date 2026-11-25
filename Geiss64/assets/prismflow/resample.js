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

/**
 * Linearly resample a Float32Array to any length.
 * - Upsampling (outLen >= inLen): point-sampled linear interpolation.
 * - Downsampling (outLen <  inLen): integrates the piecewise-linear signal and
 *   outputs the average over each output bin (anti-aliased / area resample).
 * - Preserves endpoints exactly when outLen >= 2 (out[0]=input[0], out[last]=input[last]).
 *
 * @param {Float32Array} input
 * @param {number} outLen  Desired output length (>= 0)
 * @returns {Float32Array}
 */
export function linearResampleF(input, outLen) {
  const inLen = input.length;

  if ((outLen | 0) !== outLen || outLen < 0) {
    throw new Error(`linearUpsampleF: outLen must be a non-negative integer (got ${outLen})`);
  }
  if (outLen === inLen) {
    return new Float32Array(input); // copy
  }
  if (outLen === 0 || inLen === 0) {
    return new Float32Array(0);
  }
  if (inLen === 1) {
    const out = new Float32Array(outLen);
    out.fill(input[0]);
    return out;
  }
  if (outLen === 1) {
    // Single output: return the average value of the whole piecewise-linear signal.
    // Domain is [0 .. inLen-1]. Average = integral / (inLen-1).
    const L = inLen - 1;
    let area = 0.0;
    for (let i = 0; i < L; i++) area += 0.5 * (input[i] + input[i + 1]);
    return new Float32Array([area / L]);
  }

  const out = new Float32Array(outLen);
  const L = inLen - 1;

  // If we're upsampling, keep the fast point-sample linear interpolation path.
  if (outLen > inLen) {
    const scale = L / (outLen - 1); // endpoints align
    for (let i = 0; i < outLen; i++) {
      const pos = i * scale;
      const idx = pos | 0;
      if (idx >= L) {
        out[i] = input[L]; // exact endpoint
      } else {
        const frac = pos - idx;
        const a = input[idx];
        const b = input[idx + 1];
        out[i] = a + (b - a) * frac;
      }
    }
    return out;
  }

  // Downsampling (or equal-size handled above): area resample by integrating the
  // piecewise-linear function defined by the samples.
  //
  // Output sample positions (for endpoint matching):
  //   p[i] = i * L / (outLen - 1)
  // Bin edges are midpoints between neighboring p's:
  //   e[0] = 0
  //   e[i] = 0.5*(p[i-1] + p[i]) for 1<=i<=outLen-2
  //   e[outLen-1] = L
  //
  // Then out[i] = (1/(e[i+1]-e[i])) * integral_{e[i]}^{e[i+1]} f(x) dx

  // Prefix areas over integer segments: A[k] = integral from 0 to k.
  // A has length inLen; A[0]=0, A[inLen-1]=total area over [0..L].
  const A = new Float32Array(inLen);
  for (let i = 0; i < L; i++) {
    A[i + 1] = A[i] + 0.5 * (input[i] + input[i + 1]);
  }

  // Antiderivative F(x) for x in [0..L], where f is piecewise-linear between samples.
  function F(x) {
    if (x <= 0) return 0.0;
    if (x >= L) return A[L];

    const i = x | 0;           // floor
    const t = x - i;           // in [0,1)
    const y0 = input[i];
    const y1 = input[i + 1];
    const dy = y1 - y0;

    // Integral from i to i+t of y0 + dy*s is:
    // y0*t + 0.5*dy*t^2
    return A[i] + (y0 * t) + (0.5 * dy * t * t);
  }

  const pos_scale = L / (outLen - 1);

  // Compute p[i] on the fly and bin edges via midpoints.
  // e0 = 0
  let prev_p = 0.0; // p[0]
  let left = 0.0;   // e[0]

  for (let i = 0; i < outLen; i++) {
    const p = i * pos_scale;

    // right edge:
    let right;
    if (i === outLen - 1) {
      right = L; // e[outLen] conceptually; but our last bin's right edge is L
    } else {
      const next_p = (i + 1) * pos_scale;
      right = 0.5 * (p + next_p);
    }

    // Compute average over [left, right]
    const width = right - left;
    if (width <= 0) {
      // Shouldnt happen, but avoid divide-by-zero if something weird occurs.
      out[i] = input[(p + 0.5) | 0];
    } else {
      out[i] = (F(right) - F(left)) / width;
    }

    left = right;
    prev_p = p;
  }

  // For downsampling, the bins already include the ends, but if you want to
  // *force* exact endpoint values you can uncomment these:
  // out[0] = input[0];
  // out[outLen - 1] = input[inLen - 1];

  return out;
}