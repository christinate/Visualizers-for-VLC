// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain assets/prismflow/NOTICE.txt, LICENSE.txt and OUTPUTS.txt.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stop_token>

namespace prismflow {
using Vec3=std::array<double,3>;
struct WormholeParameters {
    double r1=.28,r2=.50;
    double rotation=.18,inward_speed=.08,fall_acceleration=.35;
    double elevation_degrees=55,azimuth_degrees=0,camera_distance=3.5;
    double view_half_extent=1.2;
    bool perspective=true;
    double center_x=0,center_y=0,flow_step=.01,map_scale=2/(109*.9);
};
enum class SurfacePart:uint8_t { Plane,Lip,Tube,Miss };
struct WormholeProfile {double radius,z;};
struct WormholeProjection {double x,y,depth;};
struct WormholeHit {double s,theta;Vec3 point;SurfacePart part;};
struct WormholeRate {double inward,angular;};
struct WormholePosition {double s,theta;};
struct WormholeSample {double dx=0,dy=0;SurfacePart part=SurfacePart::Miss;};
class WormholeMotion {
    WormholeParameters p_;
    double radius_,lipLength_,epsilon_;
    Vec3 right_,up_,forward_,camera_;
    double lipDistance(double radius,double z)const;
public:
    explicit WormholeMotion(const WormholeParameters& parameters={});
    WormholeProfile profile(double s)const;
    Vec3 surfacePoint(double s,double theta)const;
    std::optional<WormholeProjection> project(const Vec3&)const;
    std::optional<WormholeHit> intersect(double x,double y)const;
    WormholeRate rates(double s)const;
    WormholePosition advance(double s,double theta,double dt)const;
    WormholeSample sample(double x,double y)const;
};
// Counts correspond to Plane/Lip/Tube/Miss. Displacements are backward lookups,
// not forward particle velocities. Float32 rounding happens at the map write.
using WormholeStats=std::array<uint64_t,4>;
WormholeStats fillWormholeMap(std::span<float> output,uint32_t width,uint32_t height,
    const WormholeParameters& parameters={},std::stop_token stop={});
}
