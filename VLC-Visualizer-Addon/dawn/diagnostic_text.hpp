// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
namespace visualizers {
// Copy a bounded valid UTF-8 diagnostic without allocating. Preserve complete
// scalars, replace invalid codec/OS bytes with '?', and always terminate.
inline size_t copyDiagnostic(char* output,size_t capacity,const char* input,size_t maximum=4095)noexcept{
    if(!output||!capacity)return 0;maximum=std::min(maximum,size_t(4095));const auto limit=std::min(capacity-1,maximum);size_t written=0,offset=0;
    while(input&&offset<maximum&&input[offset]&&written<limit){
        const uint8_t first=uint8_t(input[offset]);size_t length=1;uint32_t code=first,minimum=0;bool valid=true;
        if(first>=0xc2&&first<=0xdf){length=2;code=first&31;minimum=0x80;}else if(first>=0xe0&&first<=0xef){length=3;code=first&15;minimum=0x800;}else if(first>=0xf0&&first<=0xf4){length=4;code=first&7;minimum=0x10000;}else if(first>=0x80)valid=false;
        if(offset+length>maximum)break;
        if(valid)for(size_t n=1;n<length;++n){const uint8_t c=uint8_t(input[offset+n]);if((c&0xc0)!=0x80){valid=false;break;}code=(code<<6)|(c&63);}
        valid=valid&&code>=minimum&&code<=0x10ffff&&!(code>=0xd800&&code<=0xdfff);
        if(!valid){output[written++]='?';++offset;continue;}if(written+length>limit)break;for(size_t n=0;n<length;++n)output[written++]=input[offset++];
    }output[written]=0;return written;
}
inline std::string boundedDiagnostic(const char* input,size_t maximum=1024){maximum=std::min(maximum,size_t(4095));std::string output(maximum+1,'\0');const auto count=copyDiagnostic(output.data(),output.size(),input,maximum);output.resize(count);return output;}
}
