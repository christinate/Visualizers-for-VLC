// SPDX-License-Identifier: MIT
// Audio-shaped solid terrain with carved tunnels and visible undersides.
struct Globals {size:vec2f,time:f32,head:f32,eye:vec4f,look:vec4f,music:vec4f,color:vec4f,alternateEye:vec4f,alternateLook:vec4f};
@group(0) @binding(0) var<uniform> g:Globals;
@group(0) @binding(1) var history:texture_2d<f32>;
@group(0) @binding(2) var historySampler:sampler;
fn hash(p:vec3f)->f32 {return fract(sin(dot(p,vec3f(127.1,311.7,74.7)))*43758.5453);}
fn noise(p:vec3f)->f32{
 let i=floor(p);let f=fract(p);let u=f*f*(3.0-2.0*f);
 return mix(mix(mix(hash(i),hash(i+vec3f(1,0,0)),u.x),mix(hash(i+vec3f(0,1,0)),hash(i+vec3f(1,1,0)),u.x),u.y),
 mix(mix(hash(i+vec3f(0,0,1)),hash(i+vec3f(1,0,1)),u.x),mix(hash(i+vec3f(0,1,1)),hash(i+vec3f(1,1,1)),u.x),u.y),u.z);
}
fn audioAt(p:vec3f)->vec4f{return textureSampleLevel(history,historySampler,vec2f(clamp(p.x/48.0+.5,0.0,1.0),fract(g.head-p.z*.008)),0);}
fn field(world:vec3f)->f32{
 var p=world;
 let axis=sin(p.z*.055)*3.0;
 if(g.look.w>0.0){p.x=axis+(p.x-axis)/(1.0+g.look.w);}
 if(g.eye.w>0.0 && p.y > -2.5){p.y=-2.5+(p.y+2.5)/(1.0+g.eye.w);}
 let a=audioAt(p);let center=sin(p.z*.055)*3.0;let width=5.0+abs(a.r)*7.0;let depth=1.5+abs(a.b)*4.0;
 let ridge=pow(abs((p.x-center)/width),1.35)*3.0;
 let rough=noise(p*.17)*1.5+noise(p*.43)*.6;
 let top=-2.5+ridge+sin(p.z*(.09+depth*.016)+a.r*2.0)*1.4+rough+a.g*5.0;
 let bottom=-7.0-noise(p*.1)*3.0;
 var rock=max(max(p.y-top,bottom-p.y),(abs(p.x)-25.0)*.7);
 let tunnel=length(vec2f((p.x-center)*.9,p.y-2.0-sin(p.z*.04)*.5))-3.8;
 rock=max(rock,-tunnel);
 return rock*.48;
}
fn normal(p:vec3f)->vec3f{let e=.035;return normalize(vec3f(field(p+vec3f(e,0,0))-field(p-vec3f(e,0,0)),field(p+vec3f(0,e,0))-field(p-vec3f(0,e,0)),field(p+vec3f(0,0,e))-field(p-vec3f(0,0,e))));}
fn hue(h:f32)->vec3f{return .5+.5*cos(6.2831853*(h+vec3f(0,.3333,.6667)));}
fn shade(origin:vec3f,direction:vec3f)->vec3f{
 var distance=0.0;var hit=false;
 for(var step=0;step<104;step++){let d=field(origin+direction*distance);if(d<.035){hit=true;break;}distance+=max(.025,d);if(distance>135.0){break;}}
 let sky=mix(vec3f(.008,.013,.035),vec3f(.09,.16,.24),clamp(direction.y*.5+.5,0.0,1.0));
 if(!hit){return sky;}
 let p=origin+direction*distance;let n=normal(p);let sun=normalize(vec3f(-.4,.8,-.35));let diffuse=max(0.0,dot(n,sun));
 var occlusion=0.0;for(var i=1;i<=4;i++){let h=f32(i)*.4;occlusion+=(h-field(p+n*h))*.13;}
 let ao=clamp(1.0-occlusion,.15,1.0);
 let a=audioAt(p);let bands=.5+.5*sin(p.y*2.1+a.r*4.0);
 let color=mix(hue(g.color.x),hue(g.color.x+.17),bands);
 let rockColor=color*(.16+.84*diffuse)*ao;
 let rim=pow(1.0-max(0.0,dot(-direction,n)),3.0);
 var value=rockColor+color*rim*.3+color*max(0.0,a.g-.3)*.35;
 let fog=1.0-exp(-distance*.018);value=mix(value,sky,fog);
 return value;
}
@vertex fn vertex(@builtin(vertex_index) index:u32)->@builtin(position) vec4f{
 let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[index],0,1);
}
fn cameraColor(eye:vec3f,look:vec3f,uv:vec2f)->vec3f{
 let forward=normalize(look-eye);let right=normalize(cross(forward,vec3f(0,1,0)));let up=cross(right,forward);
 return shade(eye,normalize(forward*1.65+right*uv.x+up*uv.y));
}
fn modeBadge(color:vec3f,pixel:vec2f,size:vec2f,state:f32)->vec3f{
    if(state==0.0){return color;}
    let span=clamp(min(size.x,size.y)*.075,18.0,36.0);
    let p=(pixel-(size-vec2f(span*.76)))/span;
    let panel=1.0-smoothstep(.40,.47,length(max(abs(p)-vec2f(.04),vec2f(0))));
    let body=1.0-smoothstep(.20,.247,max(abs(p.x)*.91,abs(p.y-.149)*1.29));
    let open=state<0.0;
    let q=p-vec2f(select(0.0,.105,open),-.092);
    let ring=abs(length(vec2f(q.x,min(q.y,0.0)))-.159);
    let legs=q.y<.105&&(q.y<.009||!open||q.x>0.0);
    let shackle=select(0.0,1.0-smoothstep(.023,.057,ring),legs);
    let keyhole=1.0-smoothstep(.024,.055,length((p-vec2f(0,.137))*vec2f(1,.76)));
    let glyph=max(body*(1.0-keyhole),shackle);
    let contrast=mix(color,vec3f(.022,.033,.052),panel*.85);
    let mark=mix(vec3f(.85,.98,1.0),vec3f(1.0,.85,.53),select(0.0,1.0,open));
    return mix(color,mix(contrast,mark,glyph),abs(state));
}
@fragment fn fragment(@builtin(position) position:vec4f)->@location(0) vec4f{
 // Snapshot output controls before the ray tracer's divergent returns. This
 // avoids a reproduced software-Vulkan edge-lane artifact on clipped triangles.
 let outputSettings=g.color;
 let uv=(position.xy/g.size*2.0-1.0)*vec2f(g.size.x/g.size.y,-1);
 var color=cameraColor(g.eye.xyz,g.look.xyz,uv);
 if(g.alternateEye.w>0.0){color=mix(color,cameraColor(g.alternateEye.xyz,g.alternateLook.xyz,uv),g.alternateEye.w);}
 if(outputSettings.y>.5){color*=outputSettings.z;}else{color=pow(color/(vec3f(1)+color),vec3f(1.0/2.2));}
 return vec4f(modeBadge(max(vec3f(0),color),position.xy,g.size,g.alternateLook.w),1);
}
