// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
// Preserves the reference's artistic foreground/background scoring model.
// Scores are relative utilities, not calibrated human preference or HDR ratings.
#include "background.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

namespace prismflow {
namespace {
double clamp(double x,double lo,double hi){return std::max(lo,std::min(hi,x));}
double lerp(double a,double b,double t){return a+(b-a)*t;}
double chroma(const Color& lab){return std::hypot(lab[1],lab[2]);}
double hue(const Color& lab){return std::atan2(lab[2],lab[1]);}
double hueDifference(double a,double b){return std::abs(std::atan2(std::sin(a-b),std::cos(a-b)));}
double labF(double t){return t>std::pow(6./29,3.)?std::cbrt(t):t/(3*std::pow(6./29,2.))+4./29;}
double labInverse(double t){return t>6./29?std::pow(t,3.):3*std::pow(6./29,2.)*(t-4./29);}
Color rgbToXyz(const Color& rgb){const auto [r,g,b]=rgb;return {
    .4124564*r+.3575761*g+.1804375*b,
    .2126729*r+.7151522*g+.0721750*b,
    .0193339*r+.1191920*g+.9503041*b};}
Color xyzToLab(const Color& xyz,const Color& white={.95047,1,1.08883}){
    const double fx=labF(xyz[0]/white[0]),fy=labF(xyz[1]/white[1]),fz=labF(xyz[2]/white[2]);
    return {116*fy-16,500*(fx-fy),200*(fy-fz)};
}
Color rgbToLab(const Color& rgb){return xyzToLab(rgbToXyz(rgb));}
Color rgbToStudyLab(const Color& rgb){return xyzToLab(rgbToXyz(rgb),{.98074,1,1.18232});}
Color labToRgb(const Color& lab){const auto [l,a,b]=lab;const double fy=(l+16)/116;
    const double x=.95047*labInverse(fy+a/500),y=labInverse(fy),z=1.08883*labInverse(fy-b/200);
    return {3.2404542*x-1.5371385*y-.4985314*z,
        -.9692660*x+1.8760108*y+.0415560*z,
        .0556434*x-.2040259*y+1.0572252*z};
}
// Measured reference colors, same order and illuminant as the preserved source.
constexpr std::array<Color,32> studyXyY={{
    {.549,.313,22.93},{.407,.326,49.95},{.441,.324,22.93},{.506,.311,7.60},
    {.513,.412,49.95},{.399,.366,68.56},{.423,.375,34.86},{.481,.388,10.76},
    {.446,.472,91.25},{.391,.413,91.25},{.407,.426,49.95},{.437,.450,18.43},
    {.387,.504,68.56},{.357,.420,79.90},{.360,.436,42.40},{.369,.473,18.43},
    {.254,.449,42.40},{.288,.381,63.90},{.281,.392,34.86},{.261,.419,12.34},
    {.226,.335,49.95},{.267,.330,68.56},{.254,.328,34.86},{.233,.324,13.92},
    {.200,.230,34.86},{.255,.278,59.25},{.241,.265,28.90},{.212,.236,10.76},
    {.272,.156,18.43},{.290,.242,49.95},{.287,.222,22.93},{.280,.181,7.60}}};
constexpr std::array<double,8> munsellCoolness={2,2,6,10,13.5,18,16,10};
struct StudyColor {Color lab;double coolness;};
const auto studyColors=[](){std::array<StudyColor,32> colors{};
    for(size_t i=0;i<colors.size();++i){const auto [x,y,measuredY]=studyXyY[i];const double luminance=measuredY/112;
        colors[i]={xyzToLab({x*luminance/y,luminance,(1-x-y)*luminance/y},{.98074,1,1.18232}),munsellCoolness[i/4]};}
    return colors;
}();
double studyCoolness(const Color& lab){
    struct Neighbor {double distance,coolness;};std::array<Neighbor,32> neighbors{};
    for(size_t i=0;i<neighbors.size();++i){double distance=0;
        for(unsigned j=0;j<3;++j)distance+=std::pow(lab[j]-studyColors[i].lab[j],2.);
        neighbors[i]={distance,studyColors[i].coolness};}
    std::stable_sort(neighbors.begin(),neighbors.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});
    if(neighbors[0].distance<1e-12)return neighbors[0].coolness;
    double total=0,sum=0;for(unsigned i=0;i<4;++i){const auto& neighbor=neighbors[i];const double weight=1/(std::pow(neighbor.distance,2.)+1e-12);total+=weight;sum+=weight*neighbor.coolness;}
    return sum/total;
}
struct Description {Color rgb,lab,studyLab;double c,h,studyH,coolness;};
Description describe(const Color& rgb){const auto lab=rgbToLab(rgb),study=rgbToStudyLab(rgb);return {rgb,lab,study,chroma(lab),hue(lab),hue(study),studyCoolness(study)};}
double pairPreference(const Description& fg,const Description& bg){
    const double reliability=clamp(std::min(fg.c,bg.c)/12,0,1);
    const double empirical=.751481*(fg.coolness+bg.coolness-4)/32
        +.476133*std::abs(fg.studyLab[0]-bg.studyLab[0])/63.3673
        -.464222*(hueDifference(fg.studyH,bg.studyH)*180/std::numbers::pi-.033547949)/(179.266981384-.033547949);
    const double neutral=.476133*std::abs(fg.lab[0]-bg.lab[0])/63.3673;
    return lerp(neutral,empirical,reliability);
}
Color gamutRgb(double l,double c,double h){
    const double a=std::cos(h),b=std::sin(h);auto candidate=[&](double scale){return labToRgb({l,scale*a,scale*b});};
    auto valid=[](const Color& rgb){return std::all_of(rgb.begin(),rgb.end(),[](double v){return v>=-1e-7&&v<=1+1e-7;});};
    auto rgb=candidate(c);if(!valid(rgb)){double low=0,high=c;
        for(unsigned i=0;i<22;++i){const double mid=(low+high)/2;if(valid(candidate(mid)))low=mid;else high=mid;}
        rgb=candidate(low);
    }
    for(auto& value:rgb)value=clamp(value,0,1);return rgb;
}
void validate(const Color& foreground,const BackgroundOptions& p){
    if(!std::isfinite(p.adventurousness)||p.adventurousness<0||p.adventurousness>1||
       !std::isfinite(p.mixingFraction)||p.mixingFraction<0||p.mixingFraction>1||
       !std::isfinite(p.maxLightnessDifference)||p.maxLightnessDifference<0||p.maxLightnessDifference>30)
        throw std::invalid_argument("Invalid Prism Flow background options");
    for(auto value:foreground)if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Expected three finite linear sRGB components in [0,1]");
}
}
std::vector<BackgroundCandidate> backgroundCandidates(const Color& foreground,const BackgroundOptions& p,std::stop_token stop){
    validate(foreground,p);if(stop.stop_requested())throw std::runtime_error("Prism Flow background generation cancelled");
    const auto fg=describe(foreground);std::vector<BackgroundCandidate> candidates;candidates.reserve(2016);
    std::set<std::array<int32_t,3>> seen;const double maxDelta=p.maxLightnessDifference,targetDelta=std::min(12.,maxDelta);
    for(double step:{-1.,-2./3,-1./3,0.,1./3,2./3,1.}){
        const double lightness=clamp(fg.lab[0]+step*maxDelta,0,100);
        for(unsigned i=0;i<72;++i){
            if(stop.stop_requested())throw std::runtime_error("Prism Flow background generation cancelled");
            const double h=i*(std::numbers::pi*2)/72;
            for(double fraction:{.4,.65,.9,1.15}){
                const auto rgb=gamutRgb(lightness,clamp(std::max(24.,fg.c)*fraction,8,90),h);
                std::array<int32_t,3> key{};for(unsigned j=0;j<3;++j)key[j]=int32_t(std::floor(rgb[j]*1e6+.5));
                if(!seen.insert(key).second)continue;
                const auto bg=describe(rgb);const double deltaL=std::abs(fg.lab[0]-bg.lab[0]),deltaAb=std::hypot(fg.lab[1]-bg.lab[1],fg.lab[2]-bg.lab[2]);
                const double reliability=clamp(std::min(fg.c,bg.c)/12,0,1),angle=hueDifference(fg.h,bg.h),preference=pairPreference(fg,bg);
                const double lightnessFit=std::exp(-.5*std::pow((deltaL-targetDelta)/7,2.));
                const double foregroundScore=.5*reliability*std::sin(angle/2)+.15*(1-std::exp(-deltaAb/25))+.2*lightnessFit
                    +.1*clamp((fg.c-bg.c)/30,-1,1)+.05*clamp((fg.lab[0]-bg.lab[0])/18,-1,1);
                const double harmony=reliability*(.5+.5*std::cos(angle)),desiredC=std::max(16.,fg.c*.8);
                const double chromaFit=std::exp(-.5*std::pow((bg.c-desiredC)/25,2.));double blend=0;
                for(double t:{.25,.5,.75}){Color mixedRgb{};for(unsigned j=0;j<3;++j)mixedRgb[j]=lerp(fg.rgb[j],rgb[j],t);
                    const auto mixed=rgbToLab(mixedRgb);const double expectedC=lerp(fg.c,bg.c,t);blend+=clamp(chroma(mixed)/std::max(8.,expectedC),0,1)/3;
                }
                const double base=.35*preference+.45*foregroundScore+.08*harmony+.12*chromaFit;
                double score=lerp(base,blend,p.mixingFraction);const double distance=std::hypot(deltaL,deltaAb);
                score-=.3*std::exp(-.5*std::pow(distance/12,2.));
                candidates.push_back({rgb,score,preference,foregroundScore,harmony,blend,deltaL,0});
            }
        }
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    if(candidates.empty())throw std::runtime_error("Prism Flow background candidates are unavailable");
    const double best=candidates.front().score;
    if(p.adventurousness==0)candidates.front().probability=1;
    else{const double temperature=.01+.10*p.adventurousness,window=.06+.20*p.adventurousness;double sum=0;
        for(auto& candidate:candidates){candidate.probability=candidate.score>=best-window?std::exp((candidate.score-best)/temperature):0;sum+=candidate.probability;}
        for(auto& candidate:candidates)candidate.probability/=sum;
    }
    return candidates;
}
Color pickBackgroundColor(const Color& foreground,const BackgroundOptions& options,const RandomFunction& random,std::stop_token stop){
    if(!random)throw std::invalid_argument("A background random source is required");
    const auto candidates=backgroundCandidates(foreground,options,stop);if(options.adventurousness==0)return candidates.front().rgb;
    double target=random();if(!std::isfinite(target)||target<0||target>=1)throw std::invalid_argument("Background random source must return [0,1)");
    const auto* last=&candidates.front();for(const auto& candidate:candidates){if(candidate.probability==0)continue;last=&candidate;target-=candidate.probability;if(target<0)return candidate.rgb;}
    return last->rgb;
}
}
