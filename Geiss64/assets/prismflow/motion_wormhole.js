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

/*
 * Reservoir spillway / wormhole motion map for Geiss HDR.
 * New, standalone helper; no dependencies, random numbers, or browser APIs.
 * Coordinates: world Z up; map X right, map Y down, normalized to [-1,+1].
 * Geometry: plane Z=0 -> circular fillet of radius r2 -> infinite tube radius r1.
 *
 * Maps contain BACKWARD lookup displacements, not forward particle velocities.
 * The shader must still apply your normal warp strength (BASE_MOTION_SCALE / fps).
 * map_scale defaults to 2/(109*0.9), compensating for that BASE_MOTION_SCALE
 * and the extra legacy 0.5 in the supplied GPU shader. If those change, update it.
 * It approximately gives one simulation second per real second at strength 1.
 * flow_step controls a short RK4 backtrace used to estimate image velocity;
 * it is NOT the actual frame timestep. Smaller steps improve this approximation.
 *
 * This is an artistic prescribed flow, not a fluid solver. The cylinder is
 * geometrically infinite. A ray exactly down the open tube has no intersection;
 * that exceptional pixel gets zero motion. Texture wrapping, decay, seeding,
 * palette, and handling of newly exposed/occluded image content stay with Geiss.
 * A single 2D feedback texture cannot preserve independently hidden surface data.
 * Leave room in the image around the rim to reduce wrapped-edge artifacts.
 */

export const kWormholeDefaults = Object.freeze({
  // The ONLY two surface geometry parameters (world units):
  r1: 0.28,
  r2: 0.50,

  // Flow controls:
  rotation: 0.18,
  inward_speed: 0.08,
  fall_acceleration: 0.35,

  // Camera, looking at the world origin. Azimuth zero views from world -Y.
  elevation_degrees: 55,
  azimuth_degrees: 0,
  camera_distance: 3.5,
  view_half_extent: 1.2, // half image extent at the origin, before perspective
  perspective: true,    // false gives orthographic projection
  center_x: 0,          // placement in normalized map coordinates
  center_y: 0,

  // Integration/calibration, normally leave alone:
  flow_step: 0.01,
  map_scale: 2 / (109 * 0.9),
});

/** Create the geometry/camera/flow once, for generating many map samples. */
export function CreateWormholeMotion(options = {}) {
  const p = { ...kWormholeDefaults, ...options };
  for (const [name, value] of Object.entries(p)) {
    if (name !== "perspective" && !Number.isFinite(value)) {
      throw new TypeError(`Wormhole: ${name} must be finite`);
    }
  }
  if (typeof p.perspective !== "boolean") throw new TypeError("perspective must be boolean");
  for (const name of ["r1", "r2", "camera_distance", "view_half_extent", "flow_step"]) {
    if (p[name] <= 0) throw new RangeError(`Wormhole: ${name} must be positive`);
  }
  //if (p.inward_speed < 0 || p.fall_acceleration < 0 || p.elevation_degrees <= 0 || p.elevation_degrees > 90) {
  //  throw new RangeError("Wormhole: inward speed and acceleration >= 0; elevation in (0,90]");
  //}
  if (p.fall_acceleration < 0 || p.elevation_degrees <= 0 || p.elevation_degrees > 90) {
    throw new RangeError("Wormhole: acceleration >= 0; elevation in (0,90]");
  }

  const R = p.r1 + p.r2;
  const L = Math.PI * p.r2 * 0.5;
  const elevation = p.elevation_degrees * Math.PI / 180;
  const azimuth = p.azimuth_degrees * Math.PI / 180;
  const ce = Math.cos(elevation), se = Math.sin(elevation);
  const ca = Math.cos(azimuth), sa = Math.sin(azimuth);
  const right = [ca, sa, 0];
  const up = [-sa * se, ca * se, ce];
  const forward = [-sa * ce, ca * ce, -se];
  const camera = forward.map(v => -v * p.camera_distance);
  const epsilon = 1e-8 * Math.max(R, p.camera_distance);

  function profile(s) {
    if (s < 0) return { radius: R - s, z: 0 };
    if (s <= L) {
      const angle = s / p.r2;
      return { radius: R - p.r2 * Math.sin(angle), z: p.r2 * (Math.cos(angle) - 1) };
    }
    return { radius: p.r1, z: -p.r2 - (s - L) };
  }

  function surface_point(s, theta) {
    const q = profile(s);
    return [q.radius * Math.cos(theta), q.radius * Math.sin(theta), q.z];
  }

  function project(point) {
    const v = point.map((x, i) => x - camera[i]);
    const dot = b => v[0] * b[0] + v[1] * b[1] + v[2] * b[2];
    const depth = dot(forward);
    if (depth <= 0) return null;
    const scale = (p.perspective ? p.camera_distance / depth : 1) / p.view_half_extent;
    return { x: p.center_x + dot(right) * scale, y: p.center_y - dot(up) * scale, depth };
  }

  // Exact unsigned distance to the quarter-circle meridian. Rotating this
  // profile preserves nearest distances, so this is also a surface distance.
  function lip_distance(radius, z) {
    const a = radius - R;
    const b = z + p.r2;
    if (a <= 0 && b >= 0) return Math.abs(Math.hypot(a, b) - p.r2);
    return Math.min(Math.hypot(radius - R, z), Math.hypot(radius - p.r1, z + p.r2));
  }

  function intersect(x, y) {
    const vx = (x - p.center_x) * p.view_half_extent;
    const vy = -(y - p.center_y) * p.view_half_extent;
    let origin, direction;
    if (p.perspective) {
      origin = camera;
      direction = forward.map((v, i) => p.camera_distance * v + vx * right[i] + vy * up[i]);
      const n = Math.hypot(...direction);
      direction = direction.map(v => v / n);
    } else {
      origin = camera.map((v, i) => v + vx * right[i] + vy * up[i]);
      direction = forward;
    }
    // Upper part of the image can see sky if the horizon enters the view.
    if (direction[2] >= -1e-14) return null;
    const at = t => origin.map((v, i) => v + t * direction[i]);
    const t_plane = -origin[2] / direction[2];
    if (t_plane < 0) return null;
    const plane = at(t_plane);
    const plane_radius = Math.hypot(plane[0], plane[1]);
    if (plane_radius >= R) {
      return { s: R - plane_radius, theta: Math.atan2(plane[1], plane[0]), point: plane, part: "plane" };
    }

    // Search the rounded lip from top to bottom. Sphere tracing with its exact
    // unsigned distance cannot step across the first surface intersection.
    const t_bottom = (-p.r2 - origin[2]) / direction[2];
    let t = t_plane;
    for (let iteration = 0; iteration < 1024 && t <= t_bottom + epsilon; iteration++) {
      const point = at(t);
      const radius = Math.hypot(point[0], point[1]);
      const distance = lip_distance(radius, point[2]);
      if (distance <= epsilon) {
        const angle = Math.atan2(Math.max(0, R - radius), Math.max(0, point[2] + p.r2));
        const s = p.r2 * angle;
        const theta = Math.atan2(point[1], point[0]);
        return { s, theta, point: surface_point(s, theta), part: "lip" };
      }
      t += distance * 0.95;
    }

    // Infinite cylinder. Start at its top; the positive exit root from inside
    // the bore is the wall visible down the hole. No artificial floor/depth cap.
    const bottom = at(t_bottom);
    const A = direction[0] ** 2 + direction[1] ** 2;
    if (A < 1e-28) return null;
    const B = bottom[0] * direction[0] + bottom[1] * direction[1];
    const C = bottom[0] ** 2 + bottom[1] ** 2 - p.r1 ** 2;
    const discriminant = B * B - A * C;
    if (discriminant < 0 || C > epsilon * R) return null;
    const root = Math.sqrt(discriminant);
    const dt = B >= 0 ? -C / (B + root) : (-B + root) / A;
    if (!Number.isFinite(dt) || dt < -epsilon) return null;
    const point = at(t_bottom + Math.max(0, dt));
    return {
      s: L + Math.max(0, -point[2] - p.r2),
      theta: Math.atan2(point[1], point[0]), point, part: "tube",
    };
  }

  function rates(s) {
    const q = profile(s);
    const ratio = R / q.radius;
    return {
      //inward: ratio * Math.sqrt(p.inward_speed ** 2 + 2 * p.fall_acceleration * Math.max(0, -q.z)),
      inward:
          Math.sign(p.inward_speed) * ratio *
          Math.sqrt(
            p.inward_speed ** 2 +
            2 * p.fall_acceleration * Math.max(0, -q.z)
          ),
      angular: p.rotation * ratio * ratio,
    };
  }

  // Fourth-order Runge-Kutta, signed dt (negative for a lookup backtrace).
  function advance(s, theta, dt) {
    const a = rates(s);
    const b = rates(s + dt * a.inward * 0.5);
    const c = rates(s + dt * b.inward * 0.5);
    const d = rates(s + dt * c.inward);
    return {
      s: s + dt / 6 * (a.inward + 2 * b.inward + 2 * c.inward + d.inward),
      theta: theta + dt / 6 * (a.angular + 2 * b.angular + 2 * c.angular + d.angular),
    };
  }

  function sample(x, y) {
    const hit = intersect(x, y);
    if (!hit) return { dx: 0, dy: 0, part: "miss" };
    // Subdivide where a large angular rate or large fall speed would make
    // a single integration step inaccurate (deep throat / small inner radius).
    const rate = rates(hit.s);
    const steps = Math.max(1, Math.ceil(p.flow_step * Math.max(
      rate.inward / (0.1 * p.r2), Math.abs(rate.angular) / 0.05
    )));
    if (steps > 4096) throw new RangeError("Wormhole flow too stiff: reduce flow_step or speeds");
    let s = hit.s, theta = hit.theta;
    const dt = -p.flow_step / steps;
    for (let i = 0; i < steps; i++) ({ s, theta } = advance(s, theta, dt));
    const source = project(surface_point(s, theta));
    const dest = project(hit.point);
    if (!source || !dest) return { dx: 0, dy: 0, part: "miss" };
    const gain = p.map_scale / p.flow_step;
    return { dx: (source.x - dest.x) * gain, dy: (source.y - dest.y) * gain, part: hit.part };
  }

  return { parameters: p, profile, surface_point, project, intersect, rates, advance, sample };
}

/** Fill the existing src_dxy array; no changes to the shader or preset format. */
export function FillWormholeMotionMap(src_dxy, W, H, options = {}) {
  if (!Number.isInteger(W) || !Number.isInteger(H) || W <= 0 || H <= 0 ||
      !(src_dxy instanceof Float32Array) || src_dxy.length !== W * H * 2) {
    throw new TypeError("Expected Float32Array(W * H * 2), with positive integer W,H");
  }
  const motion = CreateWormholeMotion(options);
  const stats = { plane: 0, lip: 0, tube: 0, miss: 0 };
  const x_extent = W >= H ? 1 : W / H;
  const y_extent = W >= H ? H / W : 1;
  let offset = 0;
  for (let y = 0; y < H; y++) {
    const fy = (y * 2 / H - 1) * y_extent;
    for (let x = 0; x < W; x++) {
      const fx = (x * 2 / W - 1) * x_extent;
      const v = motion.sample(fx, fy);
      if (!Number.isFinite(v.dx) || !Number.isFinite(v.dy)) {
        throw new Error(`Nonfinite wormhole displacement at ${x},${y}`);
      }
      src_dxy[offset++] = v.dx;
      src_dxy[offset++] = v.dy;
      stats[v.part]++;
    }
  }
  return stats;
}
