#define UNICODE
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
using Microsoft::WRL::ComPtr;
static std::string utf8(const wchar_t* text){int length=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);std::string out(length,0);WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),length,nullptr,nullptr);if(!out.empty())out.pop_back();return out;}
int main(){
    using json=nlohmann::json;json result={{"outputs",json::array()},{"active_paths",json::array()}};
    ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
    for(UINT a=0;;++a){ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(a,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 ad{};adapter->GetDesc1(&ad);
        for(UINT n=0;;++n){ComPtr<IDXGIOutput> output;if(adapter->EnumOutputs(n,&output)==DXGI_ERROR_NOT_FOUND)break;ComPtr<IDXGIOutput6> out6;DXGI_OUTPUT_DESC1 d{};
            if(SUCCEEDED(output.As(&out6))&&SUCCEEDED(out6->GetDesc1(&d)))result["outputs"].push_back({{"adapter",utf8(ad.Description)},{"device",utf8(d.DeviceName)},{"attached",bool(d.AttachedToDesktop)},{"color_space",int(d.ColorSpace)},{"hdr_active",d.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020},{"bits_per_color",d.BitsPerColor},{"min_nits",d.MinLuminance},{"max_nits",d.MaxLuminance},{"max_full_frame_nits",d.MaxFullFrameLuminance}});
        }}
    UINT paths=0,modes=0;if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&paths,&modes)==ERROR_SUCCESS){
        std::vector<DISPLAYCONFIG_PATH_INFO> path(paths);std::vector<DISPLAYCONFIG_MODE_INFO> mode(modes);
        if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&paths,path.data(),&modes,mode.data(),nullptr)==ERROR_SUCCESS)for(UINT n=0;n<paths;++n){
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color{};color.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;color.header.size=sizeof(color);color.header.adapterId=path[n].targetInfo.adapterId;color.header.id=path[n].targetInfo.id;
            DISPLAYCONFIG_TARGET_DEVICE_NAME name{};name.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;name.header.size=sizeof(name);name.header.adapterId=color.header.adapterId;name.header.id=color.header.id;DisplayConfigGetDeviceInfo(&name.header);
            auto status=DisplayConfigGetDeviceInfo(&color.header);
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 detailed{};detailed.header=color.header;detailed.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;detailed.header.size=sizeof(detailed);auto hdrStatus=DisplayConfigGetDeviceInfo(&detailed.header);
            result["active_paths"].push_back({{"name",utf8(name.monitorFriendlyDeviceName)},{"query_status",status},{"advanced_color_supported",bool(color.advancedColorSupported)},{"advanced_color_enabled",bool(color.advancedColorEnabled)},{"hdr_query_status",hdrStatus},{"hdr_supported",hdrStatus==ERROR_SUCCESS&&bool(detailed.highDynamicRangeSupported)},{"hdr_user_enabled",hdrStatus==ERROR_SUCCESS&&bool(detailed.highDynamicRangeUserEnabled)},{"active_color_mode",int(detailed.activeColorMode)},{"bits_per_channel",color.bitsPerColorChannel}});
        }}
    std::cout<<result.dump(2)<<'\n';
}
