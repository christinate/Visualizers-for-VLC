// SPDX-License-Identifier: MIT
#pragma once

namespace terrain {
inline constexpr char MarineSource[]=R"(
struct MarineHit { distance:f32, color:vec3f };
fn ellipsoid(ro:vec3f,rd:vec3f,center:vec3f,radius:vec3f,limit:f32)->f32{
    let q=(ro-center)/radius;
    let d=rd/radius;
    let a=dot(d,d);
    let b=dot(q,d);
    let c=dot(q,q)-1.0;
    let discriminant=b*b-a*c;
    if(discriminant<0.0){return limit;}
    let root=sqrt(discriminant);
    let entry=(-b-root)/a;
    let exit=(-b+root)/a;
    let depth=select(exit,entry,entry>.01);
    return select(limit,depth,depth>.01&&depth<limit);
}
fn marinePart(hit:MarineHit,ro:vec3f,rd:vec3f,center:vec3f,radius:vec3f,
              color:vec3f,pattern:f32)->MarineHit{
    let depth=ellipsoid(ro,rd,center,radius,hit.distance);
    if(depth>=hit.distance){return hit;}
    let local=(ro+rd*depth-center)/radius;
    let n=normalize(local/radius);
    let light=.36+.61*max(0.0,dot(n,normalize(vec3f(-.25,.92,-.29))));
    let edge=pow(1.0-abs(dot(n,normalize(rd))),3.0);
    let marks=smoothstep(.13,.52,sin(local.x*9.0+local.y*2.5))*.42;
    return MarineHit(depth,color*(light*(1.0-pattern*marks))+vec3f(.13,.22,.24)*edge*.29);
}
fn marineAnimal(ro:vec3f,rd:vec3f,center:vec3f,heading:vec3f,size:f32,kind:u32,
                motion:f32,limit:f32)->MarineHit{
    let bound=select(select(1.25,1.65,kind==8u),2.0,kind==6u||kind==7u)*size;
    if(ellipsoid(ro,rd,center,vec3f(bound),limit)>=limit&&length(ro-center)>bound){
        return MarineHit(limit,vec3f(0));
    }
    let front=normalize(heading);
    let side=normalize(cross(front,vec3f(0,1,0)));
    let up=cross(side,front);
    let offset=(ro-center)/size;
    let eye=vec3f(dot(offset,front),dot(offset,up),dot(offset,side));
    let ray=vec3f(dot(rd,front),dot(rd,up),dot(rd,side))/size;
    var hit=MarineHit(limit,vec3f(0));
    let beat=sin(motion);
    if(kind==6u||kind==7u){
        let whale=kind==7u;
        let skin=select(vec3f(.20,.42,.50),vec3f(.09,.27,.36),whale);
        let bulk=select(.24,.44,whale);
        let lengthwise=select(.66,1.04,whale);
        hit=marinePart(hit,eye,ray,vec3f(.11,0,0),vec3f(lengthwise,bulk,bulk*.88),skin,0);
        hit=marinePart(hit,eye,ray,vec3f(.33,-bulk*.46,-.013),vec3f(lengthwise*.73,bulk*.65,bulk*.73),skin*.78+vec3f(.16,.18,.16),select(0.0,.55,whale));
        hit=marinePart(hit,eye,ray,vec3f(select(.77,1.0,whale),-.041,0),vec3f(select(.24,.34,whale),bulk*.47,bulk*.67),skin*.94,0);
        hit=marinePart(hit,eye,ray,vec3f(-.77,.11*beat,0),vec3f(.50,bulk*.46,bulk*.46),skin*.81,0);
        hit=marinePart(hit,eye,ray,vec3f(-1.18,.18*beat,-.15),vec3f(.22,.063,.28),skin,0);
        hit=marinePart(hit,eye,ray,vec3f(-1.18,.18*beat,.15),vec3f(.22,.063,.28),skin,0);
        hit=marinePart(hit,eye,ray,vec3f(-.23,bulk*1.04,0),vec3f(.20,bulk*.80,.052),skin,0);
        for(var k=0u;k<2u;k++){
            let sign=select(-1.0,1.0,k==1u);
            hit=marinePart(hit,eye,ray,vec3f(.03,-bulk*.65,sign*bulk*.94),vec3f(.24,.071,bulk*.89),skin*.74,0);
            hit=marinePart(hit,eye,ray,vec3f(select(.66,.93,whale),.025,sign*bulk*.51),vec3f(.035),vec3f(.012,.028,.032),0);
        }
    }else if(kind==8u){
        let olive=vec3f(.22,.38,.16);
        let neck=vec3f(.34,.49,.25);
        hit=marinePart(hit,eye,ray,vec3f(-.09,.06,0),vec3f(.73,.33,.53),olive,0);
        if(hit.distance<limit){
            let p=eye+ray*hit.distance;
            let plates=sin(p.x*16.0+sin(p.z*12.0))*sin(p.z*17.0+sin(p.x*9.0));
            hit.color*=.66+.34*smoothstep(-.28,.3,plates);
        }
        hit=marinePart(hit,eye,ray,vec3f(-.08,-.20,0),vec3f(.63,.15,.47),vec3f(.52,.47,.26),.4);
        hit=marinePart(hit,eye,ray,vec3f(.66,-.024,0),vec3f(.33,.16,.16),neck,0);
        for(var k=0u;k<2u;k++){
            let sign=select(-1.0,1.0,k==1u);
            hit=marinePart(hit,eye,ray,vec3f(.16,-.18+.16*beat,sign*.60),vec3f(.32,.086,.41),neck,.6);
            hit=marinePart(hit,eye,ray,vec3f(-.60,-.14-.06*beat,sign*.40),vec3f(.29,.071,.24),olive,.6);
            hit=marinePart(hit,eye,ray,vec3f(.78,.035,sign*.117),vec3f(.033),vec3f(.012,.021,.014),0);
        }
    }else if(kind==9u){
        let amber=vec3f(.88,.50,.19);
        hit=marinePart(hit,eye,ray,vec3f(-.08,-.02,0),vec3f(.28,.43,.15),amber,.72);
        hit=marinePart(hit,eye,ray,vec3f(.03,.38,0),vec3f(.25,.19,.13),amber,0);
        hit=marinePart(hit,eye,ray,vec3f(.26,.30,0),vec3f(.26,.076,.073),amber*.91,0);
        for(var k=0u;k<5u;k++){
            let a=f32(k)*.68;
            hit=marinePart(hit,eye,ray,vec3f(-.20-.19*cos(a),-.43-.20*sin(a),0),
                vec3f(.091-f32(k)*.009),amber*.85,0);
        }
        hit=marinePart(hit,eye,ray,vec3f(-.31,.06,.08*beat),vec3f(.14,.20,.036),amber*.85,0);
    }else{
        let species=array<vec3f,6>(vec3f(.03,.64,.79),vec3f(.96,.46,.12),vec3f(.62,.24,.73),
            vec3f(.78,.82,.24),vec3f(.37,.79,.67),vec3f(.54,.65,.73));
        let hue=species[kind%6u];
        let tall=.22+.042*f32(kind%3u);
        let width=select(.18,.095,(kind%3u)==1u);
        hit=marinePart(hit,eye,ray,vec3f(.07,0,0),vec3f(.50,tall,width),hue,select(.16,.94,(kind%2u)==1u));
        hit=marinePart(hit,eye,ray,vec3f(-.46,0,.08*beat),vec3f(.26,.074,.075),hue*.73,0);
        let tailZ=.19*beat;
        hit=marinePart(hit,eye,ray,vec3f(-.70,.13,tailZ),vec3f(.22,.14,.036),hue*.93,0);
        hit=marinePart(hit,eye,ray,vec3f(-.70,-.12,tailZ),vec3f(.22,.13,.036),hue*.82,0);
        hit=marinePart(hit,eye,ray,vec3f(-.10,tall*.84,0),vec3f(.17,.15,.036),hue*.77,0);
        for(var k=0u;k<2u;k++){
            let sign=select(-1.0,1.0,k==1u);
            hit=marinePart(hit,eye,ray,vec3f(.37,.055,sign*width*.75),vec3f(.063),vec3f(.025,.038,.052),0);
        }
    }
    return hit;
}
)";
} // namespace terrain
