// SPDX-License-Identifier: MIT
// Original implicit surfaces and animated joints for the Objects visualizer.
fn nearer(a:vec2f,b:vec2f)->vec2f{return select(a,b,b.x<a.x);}
fn ring(p:vec3f,radius:f32,tube:f32)->f32{
    return length(vec2f(length(p.xz)-radius,p.y))-tube;
}
fn box(p:vec3f,b:vec3f)->f32{
    let q=abs(p)-b;
    return length(max(q,vec3f(0)))+min(max(q.x,max(q.y,q.z)),0.0);
}
fn octa(p:vec3f,radius:f32)->f32{return (dot(abs(p),vec3f(1))-radius)*.57735;}
fn frame(p:vec3f)->vec3f{
    let a=vec3f(rotate2(p.xy,g.effects.w),p.z);
    let b=vec3f(a.x,rotate2(a.yz,g.geometry.z));
    let xz=rotate2(b.xz,g.geometry.w);
    let twisted=rotate2(xz,g.effects.x*sin(b.y*2.1)+g.effects.y*b.y);
    return vec3f(twisted.x,b.y,twisted.y);
}
fn surface(world:vec3f)->vec2f{
    let scale=g.geometry.y;
    let p=frame(world/scale);
    let detail=g.detail.y;
    var hit=vec2f(10,0);
    switch u32(g.geometry.x) {
        case 0u: {
            hit=vec2f(length(p)-.33,1);
            hit=nearer(hit,vec2f(ring(p,1.12,.062*detail),2));
            let x=vec3f(rotate2(p.xy,1.05+g.effects.x*2.9+.35*sin(g.geometry.w*1.6)),p.z);
            let z=vec3f(p.x,rotate2(p.yz,1.12+g.effects.y*3.1+.28*cos(g.geometry.z*2.3)));
            hit=nearer(hit,vec2f(ring(x,.96,.067*detail),3));
            hit=nearer(hit,vec2f(ring(z,.75,.081*detail),4));
            let rotor=g.effects.w*2.4;
            hit=nearer(hit,vec2f(length(x-vec3f(.96*cos(rotor),0,.96*sin(rotor)))-.12,1));
            hit=nearer(hit,vec2f(box(p,vec3f(.86,.029,.029)),2));
            hit=nearer(hit,vec2f(box(p,vec3f(.026,.86,.026)),3));
        }
        case 1u: {
            let q=p*(4.4+detail);
            let liquid=abs(dot(sin(q),cos(q.yzx)))-.31;
            let lattice=max(liquid/(13.0+detail),length(p)-1.19);
            hit=nearer(vec2f(lattice,3),vec2f(length(p)-.34,1));
            hit=nearer(hit,vec2f(ring(vec3f(p.x,rotate2(p.yz,.64+g.effects.y)),1.27,.046),2));
        }
        case 2u: {
            let angle=atan2(p.z,p.x);
            let section=vec2f(length(p.xz)-.82,p.y);
            let lane=1.5*angle+g.geometry.z*.55+g.effects.x*2.2+g.effects.y*sin(angle*4.0);
            let curl=vec2f(cos(lane),sin(lane))*.32;
            hit=vec2f((length(section-curl)-.155*detail)*.65,3);
            hit=nearer(hit,vec2f((length(section+curl)-.155*detail)*.65,4));
            hit=nearer(hit,vec2f(ring(p,1.29,.024),2));
            hit=nearer(hit,vec2f(length(p)-.28,1));
        }
        case 3u: {
            let core=max(octa(p,1.27),-octa(p,.91));
            let facets=max(core,-box(p,vec3f(.22,3.0,.22)));
            hit=vec2f(facets,3);
            let b=vec3f(rotate2(p.xy,.58+g.effects.x*2.1+g.geometry.w*.65),p.z);
            hit=nearer(hit,vec2f(octa(b,.47),1));
            hit=nearer(hit,vec2f(ring(b,1.0,.037*detail),2));
            hit=nearer(hit,vec2f(ring(vec3f(p.x,rotate2(p.yz,1.57+g.effects.y)),1.16,.04*detail),4));
        }
        default: {
            hit=vec2f(octa(p,.70),1);
            for(var n=0u;n<4u;n++){
                let turn=f32(n)*1.5707963+g.effects.y*2.2+g.geometry.w*.69;
                let q=rotate2(p.xz,turn);
                let arm=vec3f(q.x,p.y-sin(turn*2.0+g.effects.z)*.18,q.y);
                let socket=arm-vec3f(.96,0,0);
                let joint=vec3f(rotate2(socket.xy,turn+g.effects.x*3.4+g.geometry.z*1.9),socket.z);
                hit=nearer(hit,vec2f(box(arm-vec3f(.48,0,0),vec3f(.46,.033,.033)),2));
                hit=nearer(hit,vec2f(octa(joint,.32+.043*detail),3+f32(n%2u)));
                hit=nearer(hit,vec2f(ring(joint,.37,.021),4));
            }
        }
    }
    return vec2f(hit.x*scale,hit.y);
}
fn normal(p:vec3f)->vec3f{
    let e=.0035;
    let a=vec3f(1,-1,-1); let b=vec3f(-1,-1,1);
    let c=vec3f(-1,1,-1); let d=vec3f(1,1,1);
    return normalize(a*surface(p+a*e).x+b*surface(p+b*e).x+c*surface(p+c*e).x+d*surface(p+d*e).x);
}
fn sky(d:vec3f)->vec3f{
    let nebula=exp(-5.5*length(d.xy-vec2f(-.17,.13)));
    let j=hash(floor(d.x*306.0)+419.0*floor(d.y*267.0));
    let star=smoothstep(.994,1.0,j)*.32;
    return vec3f(.004,.008,.024)+g.detail.w*(vec3f(.046,.037,.12)*nebula+vec3f(star));
}
fn ground(origin:vec3f,dir:vec3f,background:vec3f)->vec3f{
    if(dir.y>=-.025){return background;}
    let distance=(-1.71-origin.y)/dir.y;
    let p=origin+dir*distance;
    let line=abs(fract(p.xz*.7+.5)-.5);
    let grid=1.0-smoothstep(.005,.022+distance*.001,min(line.x,line.y));
    let pad=exp(-.38*dot(p.xz,p.xz));
    let haze=exp(-distance*.13);
    let chips=.4+.6*hash(floor(p.x*12.0)+61.0*floor(p.z*12.0));
    return background+g.detail.w*(pad*.073*tint(g.output.x+.12)+haze*grid*chips*.16*vec3f(.2,.59,1));
}
fn light(p:vec3f,d:vec3f,material:f32,background:vec3f)->vec3f{
    let n=normal(p);
    let first=normalize(vec3f(-.5,.75,-.62));
    let second=normalize(vec3f(.7,.12,.51));
    let diffuse=max(dot(n,first),0.0);
    let fill=max(dot(n,second),0.0);
    let fresnel=pow(1.0-max(dot(n,-d),0.0),3.0);
    let specular=pow(max(dot(n,normalize(first-d)),0.0),38.0);
    let accent=pow(max(dot(n,normalize(second-d)),0.0),18.0);
    let ao=1.0-g.detail.z*clamp((.21-surface(p+n*.21).x)*3.3,0.0,1.0);
    let base=tint(g.output.x+material*.128)*(.19+.44*diffuse+.25*fill);
    let stripes=.5+.5*cos(p.y*38.0+p.x*5.0-g.viewport.z*1.1);
    let veins=select(0.0,pow(stripes,18.0)*(.045+.2*g.audio.z),material>=3.0);
    let hot=select(0.0,.24+.54*g.audio.y,material==1.0);
    return background*.26+g.features.x*ao*(base+fresnel*.31*tint(g.output.x+.38)
           +specular*vec3f(.84,.94,1.0)+accent*.45*tint(g.output.x+.68)
           +tint(g.output.x+material*.125)*(hot+veins));
}
@fragment fn fragment(@builtin(position) position:vec4f)->@location(0) vec4f{
    let output=g.output;
    let uv=(position.xy/g.viewport.xy*2.0-1.0)*vec2f(g.viewport.w,-1);
    let eye=vec3f(sin(g.camera.z)*g.camera.x,g.camera.y*g.camera.x,-cos(g.camera.z)*g.camera.x);
    let forward=normalize(-eye+vec3f(0,.02,0));
    let right=normalize(cross(forward,vec3f(0,1,0)));
    let up=cross(right,forward);
    let direction=normalize(forward*g.camera.w+right*uv.x+up*uv.y);
    var color=ground(eye,direction,sky(direction));
    let nearest=-dot(eye,direction);
    let radius=1.58*g.geometry.y;
    let entry=radius*radius-dot(eye,eye)+nearest*nearest;
    if(entry>0.0){
        let span=sqrt(entry);
        var travel=max(0.0,nearest-span);
        let end=nearest+span;
        for(var step=0u;step<104u;step++){
            if(step>=u32(g.detail.x)||travel>=end){break;}
            let p=eye+direction*travel;
            let hit=surface(p);
            if(hit.x<.003+.00032*travel){
                color=light(p,direction,hit.y,color);
                break;
            }
            travel+=max(.002,hit.x*.72);
        }
    }
    let vignette=1.0-.27*clamp(dot(uv,uv)*.24,0.0,1.0);
    let rgb=displayColor(color*vignette,output);
    return vec4f(modeBadge(rgb.rgb,position.xy,g.viewport.xy,g.view.z),1);
}
