// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain assets/prismflow/NOTICE.txt, LICENSE.txt and OUTPUTS.txt.
#include "wormhole.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>
#include <stdexcept>

namespace prismflow {
WormholeMotion::WormholeMotion(const WormholeParameters& p):p_(p){
    for(double value:{p.r1,p.r2,p.rotation,p.inward_speed,p.fall_acceleration,p.elevation_degrees,
        p.azimuth_degrees,p.camera_distance,p.view_half_extent,p.center_x,p.center_y,p.flow_step,p.map_scale})
        if(!std::isfinite(value))throw std::invalid_argument("Wormhole parameters must be finite");
    if(p.r1<=0||p.r2<=0||p.camera_distance<=0||p.view_half_extent<=0||p.flow_step<=0)
        throw std::invalid_argument("Wormhole radii, camera distance, extent and flow step must be positive");
    if(p.fall_acceleration<0||p.elevation_degrees<=0||p.elevation_degrees>90)
        throw std::invalid_argument("Wormhole acceleration must be nonnegative and elevation in (0,90]");
    radius_=p.r1+p.r2;lipLength_=std::numbers::pi*p.r2*.5;
    if(!std::isfinite(radius_)||!std::isfinite(lipLength_))throw std::invalid_argument("Wormhole geometry exceeds its numeric range");
    const double elevation=p.elevation_degrees*std::numbers::pi/180,azimuth=p.azimuth_degrees*std::numbers::pi/180;
    const double ce=std::cos(elevation),se=std::sin(elevation),ca=std::cos(azimuth),sa=std::sin(azimuth);
    right_={ca,sa,0};up_={-sa*se,ca*se,ce};forward_={-sa*ce,ca*ce,-se};
    for(unsigned n=0;n<3;++n)camera_[n]=-forward_[n]*p.camera_distance;
    epsilon_=1e-8*std::max(radius_,p.camera_distance);
}
WormholeProfile WormholeMotion::profile(double s)const{
    if(s<0)return {radius_-s,0};
    if(s<=lipLength_){const double angle=s/p_.r2;return {radius_-p_.r2*std::sin(angle),p_.r2*(std::cos(angle)-1)};}
    return {p_.r1,-p_.r2-(s-lipLength_)};
}
Vec3 WormholeMotion::surfacePoint(double s,double theta)const{
    const auto q=profile(s);return {q.radius*std::cos(theta),q.radius*std::sin(theta),q.z};
}
std::optional<WormholeProjection> WormholeMotion::project(const Vec3& point)const{
    Vec3 v{};for(unsigned n=0;n<3;++n)v[n]=point[n]-camera_[n];
    auto dot=[&](const Vec3& b){return v[0]*b[0]+v[1]*b[1]+v[2]*b[2];};
    const double depth=dot(forward_);if(depth<=0)return {};
    const double scale=(p_.perspective?p_.camera_distance/depth:1)/p_.view_half_extent;
    return WormholeProjection{p_.center_x+dot(right_)*scale,p_.center_y-dot(up_)*scale,depth};
}
double WormholeMotion::lipDistance(double radius,double z)const{
    const double a=radius-radius_,b=z+p_.r2;
    if(a<=0&&b>=0)return std::abs(std::hypot(a,b)-p_.r2);
    return std::min(std::hypot(radius-radius_,z),std::hypot(radius-p_.r1,z+p_.r2));
}
std::optional<WormholeHit> WormholeMotion::intersect(double x,double y)const{
    const double vx=(x-p_.center_x)*p_.view_half_extent,vy=-(y-p_.center_y)*p_.view_half_extent;
    Vec3 origin{},direction{};
    if(p_.perspective){
        origin=camera_;for(unsigned n=0;n<3;++n)direction[n]=p_.camera_distance*forward_[n]+vx*right_[n]+vy*up_[n];
        const double norm=std::hypot(direction[0],direction[1],direction[2]);for(auto& value:direction)value/=norm;
    }else{
        for(unsigned n=0;n<3;++n)origin[n]=camera_[n]+vx*right_[n]+vy*up_[n];direction=forward_;
    }
    if(direction[2]>=-1e-14)return {};
    auto at=[&](double t){Vec3 point{};for(unsigned n=0;n<3;++n)point[n]=origin[n]+t*direction[n];return point;};
    const double tPlane=-origin[2]/direction[2];if(tPlane<0)return {};
    const auto plane=at(tPlane);const double planeRadius=std::hypot(plane[0],plane[1]);
    if(planeRadius>=radius_)return WormholeHit{radius_-planeRadius,std::atan2(plane[1],plane[0]),plane,SurfacePart::Plane};
    const double tBottom=(-p_.r2-origin[2])/direction[2];double t=tPlane;
    for(unsigned iteration=0;iteration<1024&&t<=tBottom+epsilon_;++iteration){
        const auto point=at(t);const double radius=std::hypot(point[0],point[1]),distance=lipDistance(radius,point[2]);
        if(distance<=epsilon_){
            const double angle=std::atan2(std::max(0.,radius_-radius),std::max(0.,point[2]+p_.r2));
            const double s=p_.r2*angle,theta=std::atan2(point[1],point[0]);return WormholeHit{s,theta,surfacePoint(s,theta),SurfacePart::Lip};
        }
        t+=distance*.95;
    }
    const auto bottom=at(tBottom);
    const double a=direction[0]*direction[0]+direction[1]*direction[1];if(a<1e-28)return {};
    const double b=bottom[0]*direction[0]+bottom[1]*direction[1];
    const double c=bottom[0]*bottom[0]+bottom[1]*bottom[1]-p_.r1*p_.r1;
    const double discriminant=b*b-a*c;if(discriminant<0||c>epsilon_*radius_)return {};
    const double root=std::sqrt(discriminant),dt=b>=0?-c/(b+root):(-b+root)/a;
    if(!std::isfinite(dt)||dt<-epsilon_)return {};
    const auto point=at(tBottom+std::max(0.,dt));
    return WormholeHit{lipLength_+std::max(0.,-point[2]-p_.r2),std::atan2(point[1],point[0]),point,SurfacePart::Tube};
}
WormholeRate WormholeMotion::rates(double s)const{
    const auto q=profile(s);const double ratio=radius_/q.radius;
    const double sign=p_.inward_speed==0?std::copysign(0.,p_.inward_speed):p_.inward_speed>0?1.:-1.;
    return {sign*ratio*std::sqrt(p_.inward_speed*p_.inward_speed+2*p_.fall_acceleration*std::max(0.,-q.z)),p_.rotation*ratio*ratio};
}
WormholePosition WormholeMotion::advance(double s,double theta,double dt)const{
    const auto a=rates(s),b=rates(s+dt*a.inward*.5),c=rates(s+dt*b.inward*.5),d=rates(s+dt*c.inward);
    return {s+dt/6*(a.inward+2*b.inward+2*c.inward+d.inward),theta+dt/6*(a.angular+2*b.angular+2*c.angular+d.angular)};
}
WormholeSample WormholeMotion::sample(double x,double y)const{
    if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("Wormhole sample coordinates must be finite");
    const auto hit=intersect(x,y);if(!hit)return {};
    const auto rate=rates(hit->s);
    const double count=std::max(1.,std::ceil(p_.flow_step*std::max(rate.inward/(.1*p_.r2),std::abs(rate.angular)/.05)));
    if(!std::isfinite(count)||count>4096)throw std::range_error("Wormhole flow too stiff: reduce flow_step or speeds");
    const unsigned steps=unsigned(count);double s=hit->s,theta=hit->theta;const double dt=-p_.flow_step/steps;
    for(unsigned n=0;n<steps;++n){const auto next=advance(s,theta,dt);s=next.s;theta=next.theta;}
    const auto source=project(surfacePoint(s,theta)),dest=project(hit->point);if(!source||!dest)return {};
    const double gain=p_.map_scale/p_.flow_step;
    return {(source->x-dest->x)*gain,(source->y-dest->y)*gain,hit->part};
}
WormholeStats fillWormholeMap(std::span<float> output,uint32_t width,uint32_t height,const WormholeParameters& parameters,std::stop_token stop){
    const uint64_t pixels=uint64_t(width)*height;
    if(!width||!height||pixels>16'777'216||output.size()!=pixels*2)throw std::invalid_argument("Expected two float components per pixel within the native map budget");
    const WormholeMotion motion(parameters);WormholeStats stats{};
    const double xExtent=width>=height?1.:double(width)/height,yExtent=width>=height?double(height)/width:1.;size_t offset=0;
    for(uint32_t y=0;y<height;++y){if(stop.stop_requested())throw std::runtime_error("Prism Flow motion generation cancelled");const double fy=(double(y)*2/height-1)*yExtent;
        for(uint32_t x=0;x<width;++x){const double fx=(double(x)*2/width-1)*xExtent;const auto value=motion.sample(fx,fy);
            if(!std::isfinite(value.dx)||!std::isfinite(value.dy)||std::abs(value.dx)>std::numeric_limits<float>::max()||std::abs(value.dy)>std::numeric_limits<float>::max())throw std::runtime_error("Wormhole displacement exceeds the finite float32 range");
            output[offset++]=float(value.dx);output[offset++]=float(value.dy);++stats[size_t(value.part)];
        }
    }
    return stats;
}
}
