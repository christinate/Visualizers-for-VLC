// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR image_palette.js, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "image_palette.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace prismflow {
namespace {
struct Sample {Color lab;double weight;};
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Prism Flow image palette generation cancelled");}
double luminance(const Color& c){return .2126*c[0]+.7152*c[1]+.0722*c[2];}
double lerp(double a,double b,double t){return a+(b-a)*t;}
double smoothstep(double t){return t*t*(3-2*t);}
double distance(const Color& a,const Color& b){return std::pow(a[0]-b[0],2.)+std::pow(a[1]-b[1],2.)+std::pow(a[2]-b[2],2.);}
double chroma(const Sample& c){return std::hypot(c.lab[1],c.lab[2]);}
double srgb(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
Color toOklab(const Color& c){const auto [r,g,b]=c;
    const double l=std::cbrt(.4122214708*r+.5363325363*g+.0514459929*b),m=std::cbrt(.2119034982*r+.6806995451*g+.1073969566*b),s=std::cbrt(.0883024619*r+.2817188376*g+.6299787005*b);
    return {.2104542553*l+.7936177850*m-.0040720468*s,1.9779984951*l-2.4285922050*m+.4505937099*s,.0259040371*l+.7827717662*m-.8086757660*s};
}
Color fromOklab(double L,double a,double b){
    const double l=std::pow(L+.3963377774*a+.2158037573*b,3.),m=std::pow(L-.1055613458*a-.0638541728*b,3.),s=std::pow(L-.0894841775*a-1.2914855480*b,3.);
    return {4.0767416621*l-3.3077115913*m+.2309699292*s,-1.2684380046*l+2.6097574011*m-.3413193965*s,-.0041960863*l-.7034186147*m+1.7076147010*s};
}
size_t weightedIndex(std::span<const double> weights,const RandomFunction& random){
    double total=0;for(auto w:weights)total+=w;if(!(total>0))return 0;
    double target=random()*total;for(size_t n=0;n<weights.size();++n){target-=weights[n];if(target<0)return n;}return weights.size()-1;
}
std::vector<Sample> clusters(std::span<const Sample> samples,const ImagePaletteOptions& p,const RandomFunction& random,std::stop_token stop){
    std::vector<double> weights;weights.reserve(samples.size());for(const auto& s:samples)weights.push_back(s.weight);
    std::vector<Color> centers={samples[weightedIndex(weights,random)].lab};std::vector<double> nearest(samples.size(),std::numeric_limits<double>::infinity());
    while(centers.size()<std::min(size_t(p.clusterCount),samples.size())){
        const auto last=centers.back();double total=0;for(size_t n=0;n<samples.size();++n){if((n&255)==0)cancelled(stop);nearest[n]=std::min(nearest[n],distance(samples[n].lab,last));weights[n]=nearest[n]*samples[n].weight;total+=weights[n];}
        if(total<1e-12)break;centers.push_back(samples[weightedIndex(weights,random)].lab);
    }
    std::vector<std::array<double,4>> sums;
    for(unsigned iteration=0;iteration<p.iterations;++iteration){
        cancelled(stop);sums.assign(centers.size(),{});
        for(size_t n=0;n<samples.size();++n){if((n&255)==0)cancelled(stop);const auto& s=samples[n];size_t best=0;double bestD=std::numeric_limits<double>::infinity();
            for(size_t i=0;i<centers.size();++i){const double d=distance(s.lab,centers[i]);if(d<bestD){best=i;bestD=d;}}
            auto& sum=sums[best];for(unsigned j=0;j<3;++j)sum[j]+=s.lab[j]*s.weight;sum[3]+=s.weight;
        }
        for(size_t i=0;i<centers.size();++i)if(sums[i][3]>0)for(unsigned j=0;j<3;++j)centers[i][j]=sums[i][j]/sums[i][3];
    }
    std::vector<Sample> result;for(size_t i=0;i<centers.size();++i)if(sums[i][3]>0)result.push_back({centers[i],sums[i][3]});return result;
}
Color colorAtLuminance(double Y,double a,double b){
    const double L=std::cbrt(Y);const auto candidate=[&](double scale)->std::optional<Color>{
        auto rgb=fromOklab(L,a*scale,b*scale);const double current=luminance(rgb);if(!(current>0))return {};
        for(auto& v:rgb){v=v*Y/current;if(!std::isfinite(v)||v<0||v>1)return {};}return rgb;
    };
    if(auto full=candidate(1))return *full;double low=0,high=1;Color best{Y,Y,Y};
    for(unsigned n=0;n<24;++n){const double mid=(low+high)*.5;if(auto rgb=candidate(mid)){low=mid;best=*rgb;}else high=mid;}return best;
}
ImageColors fromClusters(std::span<const Sample> c,const ImagePaletteOptions& p,const RandomFunction& random,std::stop_token stop){
    std::vector<double> primaryWeights;primaryWeights.reserve(c.size());for(const auto& s:c)primaryWeights.push_back(std::pow(s.weight,.65)*std::pow(.025+chroma(s),1.15)*(.1+std::pow(std::sin(std::numbers::pi*std::clamp(s.lab[0],0.,1.)),2.)));
    const auto primary=weightedIndex(primaryWeights,random);const double cp=chroma(c[primary]),hp=std::atan2(c[primary].lab[2],c[primary].lab[1]);
    std::vector<double> accentWeights;accentWeights.reserve(c.size());bool any=false;
    for(size_t n=0;n<c.size();++n){const double C=chroma(c[n]),h=std::atan2(c[n].lab[2],c[n].lab[1]),angle=std::abs(std::atan2(std::sin(h-hp),std::cos(h-hp)));double w=0;
        if(n!=primary&&C>=.015&&cp>=.015&&angle>=std::numbers::pi/6&&angle<=std::numbers::pi*155/180)w=std::pow(c[n].weight,.55)*C*std::sin(angle*.5);accentWeights.push_back(w);any=any||w>0;
    }
    const auto accent=any?weightedIndex(accentWeights,random):primary;
    const double strength=accent==primary?0:.4+.45*random();const double center=.58+.14*random(),width=.19+.05*random(),exposure=1.55+.35*random(),saturation=.95+.25*random();
    const double smoothness=p.smoothness?*p.smoothness:.35+(1-.35)*random(),chaos=1-smoothness;
    std::vector<Color> knots;
    if(chaos>0){const auto count=unsigned(std::floor(2+5*chaos+.5));std::vector<bool> used(c.size(),false);size_t usedCount=0,previous=primary;
        for(unsigned n=0;n<count;++n){if(usedCount==c.size()){std::fill(used.begin(),used.end(),false);usedCount=0;}std::vector<double> weights(c.size(),0);
            for(size_t j=0;j<c.size();++j){if(used[j])continue;const double C=chroma(c[j]),P=chroma(c[previous]);const double cosine=C>.015&&P>.015?(c[j].lab[1]*c[previous].lab[1]+c[j].lab[2]*c[previous].lab[2])/(C*P):1;const double difference=.5*(1-std::clamp(cosine,-1.,1.));weights[j]=primaryWeights[j]*(.15+difference);}
            const auto index=weightedIndex(weights,random);if(!used[index]){used[index]=true;++usedCount;}previous=index;knots.push_back(c[previous].lab);
        }
    }
    ImageColors colors{};for(unsigned i=1;i<8;++i){cancelled(stop);const double t=double(i)/7,Y=p.highlightLuminance*std::pow(t,exposure),mix=strength*std::exp(-.5*std::pow((t-center)/width,2.)),taper=1-.65*smoothstep(std::max(0.,(t-.65)/.35));
        double a=lerp(c[primary].lab[1],c[accent].lab[1],mix),b=lerp(c[primary].lab[2],c[accent].lab[2],mix);
        if(chaos>0){const double position=double(i-1)/6*(knots.size()-1);const auto left=std::min(size_t(std::floor(position)),knots.size()-2);const double local=smoothstep(position-left);a=lerp(a,lerp(knots[left][1],knots[left+1][1],local),chaos);b=lerp(b,lerp(knots[left][2],knots[left+1][2],local),chaos);}
        colors[i]=colorAtLuminance(Y,a*saturation*taper,b*saturation*taper);
    }return colors;
}
}
void validateImagePaletteOptions(const ImagePaletteOptions& p){
    if(!p.sampleSize||p.sampleSize>512||!p.clusterCount||p.clusterCount>32||!p.iterations||p.iterations>50||!std::isfinite(p.highlightLuminance)||p.highlightLuminance<=0||p.highlightLuminance>=1||(p.smoothness&&(!std::isfinite(*p.smoothness)||*p.smoothness<0||*p.smoothness>1)))throw std::invalid_argument("Invalid Prism Flow image palette options");
}
ImageColors paletteFromImageRgba(uint32_t width,uint32_t height,std::span<const uint8_t> bytes,const RandomFunction& supplied,const ImagePaletteOptions& p,std::stop_token stop){
    validateImagePaletteOptions(p);if(!supplied)throw std::invalid_argument("Image palette needs a random generator");if(!width||!height||width>512||height>512||bytes.size()!=uint64_t(width)*height*4)throw std::invalid_argument("Expected bounded, color-managed sRGB RGBA8 image");cancelled(stop);
    const RandomFunction random=[&]{cancelled(stop);const double v=supplied();if(!std::isfinite(v)||v<0||v>=1)throw std::invalid_argument("Image palette random() must return [0,1)");return v;};
    std::vector<Sample> samples;samples.reserve(size_t(width)*height);
    for(size_t n=0;n<bytes.size();n+=4){if((n&1023)==0)cancelled(stop);const double alpha=double(bytes[n+3])/255;if(alpha<.05)continue;const Color rgb={srgb(double(bytes[n])/255),srgb(double(bytes[n+1])/255),srgb(double(bytes[n+2])/255)};samples.push_back({toOklab(rgb),alpha});}
    if(samples.empty())throw std::runtime_error("The image contains no visible pixels");const auto clustered=clusters(samples,p,random,stop);return fromClusters(clustered,p,random,stop);
}
Palette imagePaletteRgba(const ImageColors& colors){Palette result;result.reserve(32);for(const auto& c:colors){for(double v:c){if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid image palette RGB");result.push_back(float(v));}result.push_back(1);}return result;}
}
