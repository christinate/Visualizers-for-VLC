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
 * Research-informed foreground/background pairing for Geiss HDR.
 *
 * Input/output: LINEAR sRGB [r,g,b], components in [0,1].
 * No dependencies. Synchronous; call when choosing a palette, not every frame.
 *
 * adventurousness:
 *   0 = highest-scoring candidate, no randomness.
 *   1 = broader weighted selection.
 *
 * Scientific basis:
 * Gramazio et al. (2017), Eq. 6, adapted from Schloss & Palmer (2011).
 * https://doi.org/10.1109/TVCG.2016.2598918
 * https://doi.org/10.3758/s13414-010-0027-0
 *
 * The pair-preference model below is an approximation: its interpolation of
 * coolness from the study's 32 reference colors differs from Colorgorical.
 * Foreground contrast, harmony, blending, and chroma terms are our artistic
 * heuristics, not fitted research models. The combined model has not been
 * validated with human observers or HDR scenes.
 */

export const kBackgroundColorDefaults = Object.freeze({
  adventurousness: 0.5,
  max_lightness_difference: 18, // CIELAB L* units, allowed range [0,30].
  mixing_fraction: 0.15,       // Blend-quality scoring weight, [0,1].
  random: Math.random,         // Optionally your seeded RNG.
});

const clamp = (x, lo, hi) => Math.max(lo, Math.min(hi, x));
const lerp = (a, b, t) => a + (b - a) * t;
const kTau = Math.PI * 2;

const chroma = lab => Math.hypot(lab[1], lab[2]);
const hue = lab => Math.atan2(lab[2], lab[1]);

const hue_difference = (a, b) =>
  Math.abs(Math.atan2(Math.sin(a - b), Math.cos(a - b)));

const lab_f = t =>
  t > (6 / 29) ** 3
    ? Math.cbrt(t)
    : t / (3 * (6 / 29) ** 2) + 4 / 29;

const lab_inv = t =>
  t > 6 / 29
    ? t ** 3
    : 3 * (6 / 29) ** 2 * (t - 4 / 29);

function rgb_to_xyz([r, g, b]) {
  return [
    .4124564 * r + .3575761 * g + .1804375 * b,
    .2126729 * r + .7151522 * g + .0721750 * b,
    .0193339 * r + .1191920 * g + .9503041 * b,
  ];
}

function xyz_to_lab([x, y, z], white = [.95047, 1, 1.08883]) {
  const fx = lab_f(x / white[0]);
  const fy = lab_f(y / white[1]);
  const fz = lab_f(z / white[2]);

  return [
    116 * fy - 16,
    500 * (fx - fy),
    200 * (fy - fz),
  ];
}

const rgb_to_lab = rgb => xyz_to_lab(rgb_to_xyz(rgb));

// Re-express XYZ relative to Illuminant C for the study model.
// This changes the reference white; it is not chromatic adaptation.
const rgb_to_study_lab = rgb =>
  xyz_to_lab(rgb_to_xyz(rgb), [.98074, 1, 1.18232]);

function lab_to_rgb([L, a, b]) {
  const fy = (L + 16) / 116;
  const x = .95047 * lab_inv(fy + a / 500);
  const y = lab_inv(fy);
  const z = 1.08883 * lab_inv(fy - b / 200);

  return [
     3.2404542 * x - 1.5371385 * y - .4985314 * z,
    -.9692660 * x + 1.8760108 * y + .0415560 * z,
     .0556434 * x - .2040259 * y + 1.0572252 * z,
  ];
}

// Measured xyY reference colors from Schloss & Palmer, Table 1.
// Four cuts per hue: saturated, light, muted, dark.
// Hue order: red, orange, yellow, chartreuse, green, cyan, blue, purple.
const kStudyXyY = [
  [.549,.313,22.93], [.407,.326,49.95],
  [.441,.324,22.93], [.506,.311,7.60],

  [.513,.412,49.95], [.399,.366,68.56],
  [.423,.375,34.86], [.481,.388,10.76],

  [.446,.472,91.25], [.391,.413,91.25],
  [.407,.426,49.95], [.437,.450,18.43],

  [.387,.504,68.56], [.357,.420,79.90],
  [.360,.436,42.40], [.369,.473,18.43],

  [.254,.449,42.40], [.288,.381,63.90],
  [.281,.392,34.86], [.261,.419,12.34],

  [.226,.335,49.95], [.267,.330,68.56],
  [.254,.328,34.86], [.233,.324,13.92],

  [.200,.230,34.86], [.255,.278,59.25],
  [.241,.265,28.90], [.212,.236,10.76],

  [.272,.156,18.43], [.290,.242,49.95],
  [.287,.222,22.93], [.280,.181,7.60],
];

// Munsell hue distances from 10R, in 2.5-hue steps.
const kMunsellCoolness = [2, 2, 6, 10, 13.5, 18, 16, 10];

const kStudyColors = kStudyXyY.map(([x, y, measured_Y], i) => {
  const Y = measured_Y / 112;

  return {
    lab: xyz_to_lab(
      [x * Y / y, Y, (1 - x - y) * Y / y],
      [.98074, 1, 1.18232]
    ),
    coolness: kMunsellCoolness[Math.floor(i / 4)],
  };
});

function study_coolness(lab) {
  const neighbors = kStudyColors
    .map(c => ({
      distance: lab.reduce(
        (sum, v, j) => sum + (v - c.lab[j]) ** 2,
        0
      ),
      coolness: c.coolness,
    }))
    .sort((a, b) => a.distance - b.distance)
    .slice(0, 4);

  if (neighbors[0].distance < 1e-12) {
    return neighbors[0].coolness;
  }

  let total = 0;
  let sum = 0;

  for (const c of neighbors) {
    const weight = 1 / (c.distance ** 2 + 1e-12);
    total += weight;
    sum += weight * c.coolness;
  }

  return sum / total;
}

function describe(rgb) {
  const lab = rgb_to_lab(rgb);
  const study_lab = rgb_to_study_lab(rgb);

  return {
    rgb,
    lab,
    C: chroma(lab),
    h: hue(lab),
    study_lab,
    study_h: hue(study_lab),
    coolness: study_coolness(study_lab),
  };
}

function pair_preference(fg, bg) {
  const reliability = clamp(Math.min(fg.C, bg.C) / 12, 0, 1);

  // Published regression coefficients / 100, with the normalization
  // ranges used for the LCh adaptation.
  const empirical =
      .751481 * (fg.coolness + bg.coolness - 4) / 32
    + .476133 * Math.abs(fg.study_lab[0] - bg.study_lab[0]) / 63.3673
    - .464222
      * (
        hue_difference(fg.study_h, bg.study_h) * 180 / Math.PI
        - .033547949
      )
      / (179.266981384 - .033547949);

  // Hue becomes unreliable near gray. Use a heuristic neutral fallback.
  const neutral =
    .476133 * Math.abs(fg.lab[0] - bg.lab[0]) / 63.3673;

  return lerp(neutral, empirical, reliability);
}

function gamut_rgb(L, C, h) {
  const a = Math.cos(h);
  const b = Math.sin(h);

  const candidate = c => lab_to_rgb([L, c * a, c * b]);
  const valid = rgb =>
    rgb.every(v => v >= -1e-7 && v <= 1 + 1e-7);

  let rgb = candidate(C);

  // Reduce chroma while retaining lightness and hue.
  if (!valid(rgb)) {
    let low = 0;
    let high = C;

    for (let i = 0; i < 22; i++) {
      const mid = (low + high) / 2;

      if (valid(candidate(mid))) {
        low = mid;
      } else {
        high = mid;
      }
    }

    rgb = candidate(low);
  }

  // Only removes tiny numerical excursions after gamut mapping.
  return rgb.map(v => clamp(v, 0, 1));
}

function get_options(options) {
  const p = { ...kBackgroundColorDefaults, ...options };

  for (const key of ['adventurousness', 'mixing_fraction']) {
    if (!Number.isFinite(p[key]) || p[key] < 0 || p[key] > 1) {
      throw new RangeError(`${key} must be a number in [0,1]`);
    }
  }

  if (
    !Number.isFinite(p.max_lightness_difference) ||
    p.max_lightness_difference < 0 ||
    p.max_lightness_difference > 30
  ) {
    throw new RangeError(
      'max_lightness_difference must be in [0,30] CIELAB units'
    );
  }

  if (typeof p.random !== 'function') {
    throw new TypeError('random must be a function');
  }

  return p;
}

function rank_candidates(foreground_rgb, p) {
  if (
    !foreground_rgb ||
    foreground_rgb.length !== 3 ||
    !Array.from(foreground_rgb).every(
      v => Number.isFinite(v) && v >= 0 && v <= 1
    )
  ) {
    throw new TypeError(
      'Expected three finite LINEAR sRGB components in [0,1]'
    );
  }

  const fg = describe(Array.from(foreground_rgb));
  const candidates = [];
  const seen = new Set();

  const max_delta = p.max_lightness_difference;
  const target_delta = Math.min(12, max_delta);

  // Fixed grid: adventurousness affects selection, not generation.
  for (const step of [-1, -2/3, -1/3, 0, 1/3, 2/3, 1]) {
    const L = clamp(fg.lab[0] + step * max_delta, 0, 100);

    for (let i = 0; i < 72; i++) {
      const h = i * kTau / 72;

      for (const fraction of [.4, .65, .9, 1.15]) {
        const rgb = gamut_rgb(
          L,
          clamp(Math.max(24, fg.C) * fraction, 8, 90),
          h
        );

        const key = rgb.map(v => Math.round(v * 1e6)).join(',');

        if (seen.has(key)) continue;
        seen.add(key);

        const bg = describe(rgb);

        const delta_L = Math.abs(fg.lab[0] - bg.lab[0]);
        const delta_ab = Math.hypot(
          fg.lab[1] - bg.lab[1],
          fg.lab[2] - bg.lab[2]
        );

        const reliability = clamp(Math.min(fg.C, bg.C) / 12, 0, 1);
        const angle = hue_difference(fg.h, bg.h);
        const preference = pair_preference(fg, bg);

        // Artistic terms: moderate contrast, hue separation, and a
        // tendency toward a slightly less saturated/darker background.
        const lightness_fit = Math.exp(
          -.5 * ((delta_L - target_delta) / 7) ** 2
        );

        const foreground =
            .5 * reliability * Math.sin(angle / 2)
          + .15 * (1 - Math.exp(-delta_ab / 25))
          + .2 * lightness_fit
          + .1 * clamp((fg.C - bg.C) / 30, -1, 1)
          + .05 * clamp((fg.lab[0] - bg.lab[0]) / 18, -1, 1);

        const harmony =
          reliability * (.5 + .5 * Math.cos(angle));

        const desired_C = Math.max(16, fg.C * .8);
        const chroma_fit = Math.exp(
          -.5 * ((bg.C - desired_C) / 25) ** 2
        );

        // Check actual linear RGB mixtures. Penalize severe chroma
        // cancellation without requiring fully saturated mixtures.
        let blend = 0;

        for (const t of [.25, .5, .75]) {
          const mixed = rgb_to_lab(
            fg.rgb.map((v, j) => lerp(v, rgb[j], t))
          );

          const expected_C = lerp(fg.C, bg.C, t);

          blend += clamp(
            chroma(mixed) / Math.max(8, expected_C),
            0,
            1
          ) / 3;
        }

        const base =
            .35 * preference
          + .45 * foreground
          + .08 * harmony
          + .12 * chroma_fit;

        let score = lerp(base, blend, p.mixing_fraction);

        // Suppress near-duplicates, including neutral pairs.
        const distance = Math.hypot(delta_L, delta_ab);
        score -= .3 * Math.exp(-.5 * (distance / 12) ** 2);

        candidates.push({
          rgb,
          score,
          preference,
          foreground,
          harmony,
          blend,
          lightness_difference: delta_L,
          probability: 0,
        });
      }
    }
  }

  candidates.sort((a, b) => b.score - a.score);

  const best = candidates[0].score;

  if (p.adventurousness === 0) {
    candidates[0].probability = 1;
  } else {
    const temperature = .01 + .10 * p.adventurousness;
    const window = .06 + .20 * p.adventurousness;
    let sum = 0;

    for (const c of candidates) {
      c.probability = c.score >= best - window
        ? Math.exp((c.score - best) / temperature)
        : 0;

      sum += c.probability;
    }

    for (const c of candidates) {
      c.probability /= sum;
    }
  }

  return candidates;
}

/**
 * Optional diagnostics: candidates sorted by descending score,
 * including component scores and selection probabilities.
 * Scores are relative utilities, not calibrated liking ratings.
 */
export function GetBackgroundColorCandidates(
  foreground_rgb,
  options = {}
) {
  return rank_candidates(foreground_rgb, get_options(options));
}

/**
 * Returns one linear RGB tuple.
 * Higher-scoring candidates have greater selection probability.
 * adventurousness=0 does not call random().
 */
export function PickBackgroundColor(foreground_rgb, options = {}) {
  const p = get_options(options);
  const candidates = rank_candidates(foreground_rgb, p);

  if (p.adventurousness === 0) {
    return candidates[0].rgb.slice();
  }

  let target = p.random();

  if (!Number.isFinite(target) || target < 0 || target >= 1) {
    throw new RangeError('random() must return a number in [0,1)');
  }

  let last = candidates[0];

  for (const c of candidates) {
    if (c.probability === 0) continue;

    last = c;
    target -= c.probability;

    if (target < 0) {
      return c.rgb.slice();
    }
  }

  return last.rgb.slice();
}