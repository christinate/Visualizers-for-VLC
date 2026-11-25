// SPDX-License-Identifier: GPL-2.0-or-later
#include "context.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace visualizers::gpu;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    try{
        unsigned roundtrips=0,midpoints=0;
        for(unsigned bits=0;bits<=0xffff;++bits){if((bits&0x7c00)==0x7c00)continue;
            require(encodeHalf(decodeHalf(uint16_t(bits)))==bits,"Finite half-float round trip changed bits");++roundtrips;}
        for(unsigned low=0;low<0x7bff;++low){const float a=decodeHalf(uint16_t(low)),b=decodeHalf(uint16_t(low+1));const float middle=(a+b)*.5f;
            const uint16_t expected=uint16_t((low&1)?low+1:low);
            require(encodeHalf(middle)==expected,"Positive midpoint did not round to even");require(encodeHalf(-middle)==(0x8000|expected),"Negative midpoint did not round to even");
            require(encodeHalf(std::nextafter(middle,-INFINITY))==low,"Value below midpoint rounded upward");require(encodeHalf(std::nextafter(middle,INFINITY))==low+1,"Value above midpoint rounded downward");++midpoints;
        }
        require(encodeHalf(std::numeric_limits<float>::infinity())==0&&encodeHalf(-std::numeric_limits<float>::infinity())==0&&encodeHalf(std::numeric_limits<float>::quiet_NaN())==0,"Nonfinite host conversion policy changed");
        require(encodeHalf(std::numeric_limits<float>::max())==0x7bff&&encodeHalf(-std::numeric_limits<float>::max())==0xfbff,"Finite host saturation policy changed");
        require(encodeHalf(1.9999f)==0x4000&&encodeHalf(std::ldexp(1.f,-24))==1,"Precision regression");
        std::cout<<nlohmann::json({{"result","pass"},{"finite_half_roundtrips",roundtrips},{"adjacent_half_midpoints",midpoints},{"signed_ties_and_neighbor_checks",midpoints*4},{"nonfinite_and_saturation_policy","preserved"}}).dump(2)<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
