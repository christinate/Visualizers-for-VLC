#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
using Microsoft::WRL::ComPtr;
class Presenter {
    HWND window_; bool hdr_=false,valid_=false;float peak_;bool vsync_;
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain3> chain_;ComPtr<ID3D11RenderTargetView> target_;
    ComPtr<ID3D11VertexShader> vs_;ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11Texture2D> texture_;ComPtr<ID3D11ShaderResourceView> view_;
    ComPtr<ID3D11SamplerState> sampler_;ComPtr<ID3D11Buffer> constants_;
    unsigned width_=0,height_=0,textureWidth_=0,textureHeight_=0;
    DXGI_FORMAT format_=DXGI_FORMAT_B8G8R8A8_UNORM;
public:
    Presenter(HWND window,bool requestHdr,int peak,bool vsync):window_(window),peak_(float(peak)),vsync_(vsync){
        D3D_FEATURE_LEVEL level;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_)))return;
        ComPtr<IDXGIDevice> dxDevice;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;
        if(FAILED(device_.As(&dxDevice))||FAILED(dxDevice->GetAdapter(&adapter))||FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))return;
        if(requestHdr){
            auto monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
            for(UINT a=0;;++a){ComPtr<IDXGIAdapter1> ad;ComPtr<IDXGIFactory1> f1;if(FAILED(factory.As(&f1))||f1->EnumAdapters1(a,&ad)==DXGI_ERROR_NOT_FOUND)break;
                for(UINT n=0;;++n){ComPtr<IDXGIOutput> output;if(ad->EnumOutputs(n,&output)==DXGI_ERROR_NOT_FOUND)break;DXGI_OUTPUT_DESC desc{};output->GetDesc(&desc);if(desc.Monitor!=monitor)continue;
                    ComPtr<IDXGIOutput6> out6;DXGI_OUTPUT_DESC1 d{};if(SUCCEEDED(output.As(&out6))&&SUCCEEDED(out6->GetDesc1(&d)))hdr_=d.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                }
            }
        }
        if(hdr_)format_=DXGI_FORMAT_R16G16B16A16_FLOAT;
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=640;desc.Height=480;desc.Format=format_;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> sc;
        HRESULT created=factory->CreateSwapChainForHwnd(device_.Get(),window,&desc,nullptr,nullptr,&sc);
        if(FAILED(created)&&hdr_){hdr_=false;format_=desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;created=factory->CreateSwapChainForHwnd(device_.Get(),window,&desc,nullptr,nullptr,&sc);}
        if(FAILED(created)||FAILED(sc.As(&chain_)))return;
        factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
        if(hdr_){UINT support=0;auto cs=DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;if(FAILED(chain_->CheckColorSpaceSupport(cs,&support))||!(support&DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)||FAILED(chain_->SetColorSpace1(cs))){
            hdr_=false;chain_.Reset();sc.Reset();context_->Flush();format_=desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
            if(FAILED(factory->CreateSwapChainForHwnd(device_.Get(),window,&desc,nullptr,nullptr,&sc))||FAILED(sc.As(&chain_)))return;
        }}
        static const char* shader=R"(
            cbuffer Options : register(b0) {float hdr;float peak;float2 padding;}
            Texture2D image : register(t0);SamplerState imageSampler : register(s0);
            struct Out {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};
            Out vertex(uint id:SV_VertexID){Out o;o.uv=float2((id<<1)&2,id&2);o.pos=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
            float4 pixel(Out i):SV_TARGET {
                float3 c=image.Sample(imageSampler,i.uv).rgb;
                if(hdr>0.5){float luminance=dot(c,float3(0.2126,0.7152,0.0722));float highlights=smoothstep(0.45,1.0,luminance);c*=lerp(203.0,peak,highlights)/80.0;}
                else {c=lerp(12.92*c,1.055*pow(max(c,0.0),1.0/2.4)-0.055,step(0.0031308,c));}
                return float4(c,1);
            })";
        ComPtr<ID3DBlob> vs,ps,error;
        if(FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vertex","vs_4_0",0,0,&vs,&error))||FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"pixel","ps_4_0",0,0,&ps,&error)))return;
        if(FAILED(device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vs_))||FAILED(device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ps_)))return;
        D3D11_SAMPLER_DESC sampling{};sampling.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampling.AddressU=sampling.AddressV=sampling.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampling.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sampling,&sampler_)))return;
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=16;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(FAILED(device_->CreateBuffer(&buffer,nullptr,&constants_)))return;valid_=true;
    }
    bool valid()const{return valid_;}bool hdr()const{return hdr_;}
    bool draw(const unsigned char* pixels,int w,int h,bool fullscreen=false){
        RECT rect;GetClientRect(window_,&rect);unsigned width=std::max(1L,rect.right),height=std::max(1L,rect.bottom);
        if(width!=width_||height!=height_){context_->OMSetRenderTargets(0,nullptr,nullptr);target_.Reset();if(FAILED(chain_->ResizeBuffers(0,width,height,format_,0)))return false;ComPtr<ID3D11Texture2D> back;if(FAILED(chain_->GetBuffer(0,IID_PPV_ARGS(&back)))||FAILED(device_->CreateRenderTargetView(back.Get(),nullptr,&target_)))return false;width_=width;height_=height;}
        if(unsigned(w)!=textureWidth_||unsigned(h)!=textureHeight_){view_.Reset();texture_.Reset();D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;if(FAILED(device_->CreateTexture2D(&desc,nullptr,&texture_))||FAILED(device_->CreateShaderResourceView(texture_.Get(),nullptr,&view_)))return false;textureWidth_=w;textureHeight_=h;}
        context_->UpdateSubresource(texture_.Get(),0,nullptr,pixels,w*4,0);
        float options[4]={hdr_?1.0f:0.0f,peak_,0,0};context_->UpdateSubresource(constants_.Get(),0,nullptr,options,0,0);
        auto target=target_.Get();context_->OMSetRenderTargets(1,&target,nullptr);D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context_->RSSetViewports(1,&viewport);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
        auto view=view_.Get();auto sampler=sampler_.Get();auto constants=constants_.Get();context_->PSSetShaderResources(0,1,&view);context_->PSSetSamplers(0,1,&sampler);context_->PSSetConstantBuffers(0,1,&constants);context_->Draw(3,0);return SUCCEEDED(chain_->Present(vsync_&&fullscreen?1:0,0));
    }
};
