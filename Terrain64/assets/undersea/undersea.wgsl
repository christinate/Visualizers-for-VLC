// SPDX-License-Identifier: MIT
struct ReefView { eye:vec3f, forward:vec3f, right:vec3f, up:vec3f, focal:f32 };
fn inCanyon()->bool{return g.view.z>.5;}
fn reefAxis(z:f32)->f32{return sin(z*.055)*select(1.15,3.0,inCanyon());}
fn reefView()->ReefView{
    let eye=g.camera.xyz+vec3f(reefAxis(g.camera.z),0,0);
    let ahead=eye.z+select(17.0,11.0,inCanyon());
    let gaze=vec3f(reefAxis(ahead)-g.camera.x*.28,select(2.72,eye.y+.17,inCanyon()),ahead);
    let forward=normalize(gaze-eye);
    let right=normalize(cross(vec3f(0,1,0),forward));
    return ReefView(eye,forward,right,cross(forward,right),g.camera.w);
}
fn swell(p:vec2f)->vec2f{
    let phase=g.reefCurrent.x-p.y*.067+p.x*.044;
    let first=sin(phase);
    let push=first*(.65+.35*abs(first));
    return vec2f(push,.32*sin(phase-.63))*g.reefCurrent.y*(.91+g.reefCurrent.z);
}
fn seed(i:u32)->f32{return hash(f32(i)*13.1+g.reef.x*.137);}
fn reefBound(ro:vec3f,rd:vec3f,center:vec3f,radii:vec3f,limit:f32)->bool{
    let relative=(ro-center)/radii;
    return dot(relative,relative)<1.0||ellipsoid(ro,rd,center,radii,limit)<limit;
}
fn reefNoise(p:vec2f)->f32{
    let cell=floor(p);
    let f=fract(p);
    let w=f*f*(3.0-2.0*f);
    let key=dot(cell,vec2f(13.17,57.31));
    return mix(mix(hash(key),hash(key+13.17),w.x),mix(hash(key+57.31),hash(key+70.48),w.x),w.y);
}
fn reefFloor(p:vec2f)->f32{
    let canyon=inCanyon();
    let width=select(4.75,g.reefTerrain.z,canyon)*(.96+.13*sin(p.y*.109)+.082*cos(p.y*.287));
    let bank=abs((p.x-reefAxis(p.y))/width);
    // The same curved ridge profile as Terrain, with a lower sandy channel.
    let ridge=pow(clamp(bank,0.0,1.56),1.35);
    let flank=smoothstep(.32,1.46,ridge);
    let key=g.reef.x*.023;
    let boulders=reefNoise(p*.47+key);
    let slabs=reefNoise(p*vec2f(.89,.64)+key*.4);
    let shelfs=reefNoise(p*vec2f(.36,.19)+key*1.14);
    let strata=sin(p.y*.39+p.x*.45+slabs*2.7);
    let chunks=pow(smoothstep(.22,.91,boulders),1.8);
    let bed=.07+.18*sin(p.x*.68+p.y*.31)+.32*reefNoise(p*.73);
    let height=select(4.28,g.reefTerrain.w,canyon);
    let ledge=flank*mix(.44,1.08,smoothstep(.12,.78,shelfs));
    let wall=select(ledge,flank*(.66+.42*shelfs),canyon);
    let outcrop=chunks*(.36+1.84*smoothstep(.19,.82,bank));
    return max(-.13,bed+wall*(height*(.76+.28*boulders)+.71*strata)+outcrop+slabs*.33);
}
fn floorHit(origin:vec3f,ray:vec3f,limit:f32)->f32{
    var last=.045;
    var distance=last;
    for(var step=0u;step<62u;step++){
        let p=origin+ray*distance;
        let gap=p.y-reefFloor(p.xz);
        // Finish a grazing hit within roughly one pixel rather than letting
        // it skip to the next exposed shelf when the march budget runs out.
        if(gap<=.015+distance*.00056){
            if(gap>=0.0){return distance;}
            var near=last;
            var far=distance;
            for(var j=0u;j<5u;j++){
                let mid=(near+far)*.5;
                let q=origin+ray*mid;
                if(q.y>reefFloor(q.xz)){near=mid;}else{far=mid;}
            }
            return far;
        }
        last=distance;
        distance+=clamp(gap*.43,.050,2.85);
        if(distance>=limit){break;}
    }
    return limit;
}
fn floorColor(p:vec3f,ray:vec3f)->vec3f{
    let e=vec2f(.08,0);
    let slope=vec2f(reefFloor(p.xz+e.xy)-reefFloor(p.xz-e.xy),
                    reefFloor(p.xz+e.yx)-reefFloor(p.xz-e.yx));
    let n=normalize(vec3f(-slope.x,.16,-slope.y));
    let coarse=reefNoise(p.xz*vec2f(1.9,2.6)+p.y*.55);
    let grit=reefNoise(p.xz*11.3+p.y*6.0);
    let seams=1.0-smoothstep(.023,.31,abs(sin(p.y*9.3+p.z*1.03+coarse*5.9)));
    let growth=reefNoise(p.xz*1.25+p.y*vec2f(-.51,.97));
    let deposit=smoothstep(.43,.72,growth)*smoothstep(.16,.64,p.y);
    let stone=mix(vec3f(.032,.077,.092),vec3f(.19,.231,.214),coarse);
    let crust=mix(vec3f(.110,.122,.039),vec3f(.27,.094,.054),reefNoise(p.xz*.76+p.y*1.3));
    let reef=mix(stone*(.57+.55*grit-.20*seams),crust*(.67+.56*grit),deposit*.83);
    let sand=mix(vec3f(.096,.181,.198),vec3f(.29,.38,.31),grit*.49+coarse*.51);
    let bank=smoothstep(.35,1.05,p.y)+.51*(1.0-n.y);
    let diffuse=.42+.72*max(0.0,dot(n,normalize(vec3f(-.24,.82,.42))));
    return mix(sand,reef,clamp(bank,0.0,1.0))*diffuse+
        .09*vec3f(.12,.20,.24)*pow(1.0-abs(dot(n,ray)),3.0);
}
fn caustics(p:vec2f)->f32{
    let flow=swell(p)*g.water.z*1.74;
    let s=p-flow;
    let q=s+vec2f(.49*sin(s.y*1.39)+.27*sin(s.x*1.78+s.y*.71),
                 .54*cos(s.x*1.17)+.23*sin(s.y*1.46-s.x*.57));
    let a=sin(q.x*2.08+sin(q.y*1.44)*.91);
    let b=sin(q.y*2.79+cos(q.x*1.64)*.68);
    let c=sin(q.x*1.67+q.y*1.51)*cos(q.x*.87-q.y*1.02);
    let junction=abs(a+b*.81+c*.53);
    let mesh=1.0-smoothstep(.012,.23,junction);
    let shimmer=.31+.48*abs(a-b)+.13*sin(q.x*.64-q.y*.87);
    return mesh*shimmer*g.water.y;
}
fn glassSurface(origin:vec3f,ray:vec3f,distance:f32)->vec3f{
    let p=(origin+ray*distance).xz;
    let s=p-swell(p)*g.water.z*1.22;
    let a=dot(s,vec2f(1.12,.55));
    let b=dot(s,vec2f(-.61,1.63))+.42*sin(a*.69);
    let c=dot(s,vec2f(2.08,-.94))+.62*cos(b*.7);
    let slope=vec2f(1.12,.55)*cos(a)*.16+vec2f(-.61,1.63)*cos(b)*.12+
        vec2f(2.08,-.94)*cos(c)*.046;
    let normal=normalize(vec3f(slope.x*g.reefCurrent.w,-1,slope.y*g.reefCurrent.w));
    let transmitted=refract(ray,normal,1.333);
    let reflective=pow(1.0-clamp(dot(-normal,ray),0.0,1.0),3.6);
    let lens=abs(sin(a+sin(c)*.54)+sin(b)*.62+cos(c)*.27);
    let sheen=1.0-smoothstep(.023,.43,lens);
    let bands=.5+.5*sin(dot(s,vec2f(.63,.76))+sin(b*.74)*.53);
    let sun=normalize(vec3f(-.27,.48,.83));
    let bright=exp(-length(ray-sun)*4.82);
    let folds=sheen*(.27+.43*bands)+caustics(p*.63)*.61;
    let mirror=mix(vec3f(.013,.108,.164),vec3f(.034,.304,.425),bands*.63)+
        vec3f(.24,.69,.79)*folds*(.35+bright)+vec3f(.16,.40,.42)*bright;
    if(dot(transmitted,transmitted)<.5){return mirror;}
    let glint=exp(-length(transmitted-normalize(vec3f(-.36,.73,.58)))*5.2);
    let sky=mix(vec3f(.067,.26,.38),vec3f(.37,.79,1.04),sqrt(max(0.0,transmitted.y)));
    let highlights=vec3f(.68,.83,.80)*(glint*.94+folds*.47);
    return mix(sky+highlights,mirror,reflective*.82);
}
fn beam(ro:vec3f,rd:vec3f,a:vec3f,b:vec3f,r:f32,color:vec3f,hit:MarineHit)->MarineHit{
    let axis=b-a;
    let lengthAxis=length(axis);
    let forward=axis/max(.001,lengthAxis);
    let side=normalize(cross(forward,select(vec3f(0,1,0),vec3f(1,0,0),abs(forward.y)>.92)));
    let up=cross(forward,side);
    let q=ro-(a+b)*.5;
    return marinePart(hit,vec3f(dot(q,forward),dot(q,side),dot(q,up)),
        vec3f(dot(rd,forward),dot(rd,side),dot(rd,up)),vec3f(0),
        vec3f(max(.001,lengthAxis*.63+r*.56),r,r),color,0);
}
fn reefRow(i:u32)->f32{
    let base=select(0.0,floor((g.camera.z+5.4)/4.35)-2.0,inCanyon());
    return base+f32(i/2u);
}
fn siteKey(i:u32)->u32{
    let row=i32(reefRow(i));
    let key=select(-2*row-1,2*row,row>=0);
    return u32(key)*2u+i%2u;
}
fn coralBase(i:u32)->vec3f{
    let side=select(-1.0,1.0,i%2u==1u);
    let j=siteKey(i);
    let row=reefRow(i);
    let z=select(-4.45+row*4.6,row*4.35,inCanyon())+side*.93+(seed(j+70u)-.5)*1.08;
    let shore=select(3.38,g.reefTerrain.z*.91,inCanyon());
    let x=reefAxis(z)+side*(shore+.72*sin(z*.27+side*1.71)+.81*seed(j+40u));
    return vec3f(x,reefFloor(vec2f(x,z))-.11,z);
}
fn rotateBranch(p:vec3f,a:f32)->vec3f{
    let fork=rotate2(p.xz,a);
    return vec3f(fork.x,p.y,fork.y);
}
fn coral(ro:vec3f,rd:vec3f,j:u32,base:vec3f,scale:f32,initial:MarineHit)->MarineHit{
    let form=seed(j+121u);
    let kind=(j+u32(seed(j+5u)*4.0))%5u;
    let size=(.91+seed(j+12u)*1.04)*scale;
    if(!reefBound(ro,rd,base+vec3f(0,size*.70,0),vec3f(size*1.42,size*1.1,size*1.25),initial.distance)){
        return initial;
    }
    let colors=array<vec3f,7>(vec3f(1.14,.237,.055),vec3f(.51,.097,.81),vec3f(1.17,.56,.14),
        vec3f(.17,.71,.58),vec3f(1.04,.284,.37),vec3f(.78,.69,.42),vec3f(.22,.378,1.04));
    let color=colors[(j+u32(form*7.0))%7u];
    var hit=initial;
    let yaw=form*6.283185;
    if(kind==3u){
        // Lobes share a low base, with winding grooves across their surfaces.
        for(var k=0u;k<5u;k++){
            let a=yaw+f32(k)*2.39996;
            let s=.31+.31*seed(j*11u+k+680u);
            let center=base+vec3f(cos(a)*s*.71,.24+s*.52,sin(a)*s*.72)*size;
            hit=marinePart(hit,ro,rd,center,vec3f(s+.19,s*.82,s+.09)*size,color*(.73+.48*s),0);
        }
    }else{
        let branches=select(select(6u,9u,kind==1u),11u,kind==2u);
        for(var k=0u;k<branches;k++){
            let t=f32(k)/f32(branches-1u);
            let r=seed(j*53u+k*5u+101u);
            let angle=f32(k)*2.39996+yaw;
            let spread=vec3f(cos(angle),0,sin(angle));
            let fan=rotateBranch(vec3f(2.0*t-1.0,0,(r-.5)*.26),yaw);
            let plan=mix(spread*(.46+.39*r),fan,select(0.0,1.0,kind==2u));
            let height=size*(.55+.65*r+select(.16,.4*sin(t*3.14159),kind==2u));
            let foot=base+size*plan*select(.15,.045,kind==2u);
            let knee=mix(foot,base+plan*size,.56)+vec3f(0,height*.51,0);
            let top=base+plan*size*(.82+.27*r)+vec3f(0,height,0);
            let thickness=size*mix(.058,.115,r)*select(1.0,.61,kind==2u);
            if(kind==4u){
                let lip=knee+vec3f(0,height*.41,0);
                hit=beam(ro,rd,foot,lip,thickness*1.64,color*.83,hit);
                hit=marinePart(hit,ro,rd,lip,vec3f(1.48,.39,1.38)*thickness,color*1.13,0);
                hit=marinePart(hit,ro,rd,lip+vec3f(0,.30*thickness,0),
                    vec3f(.99,.15,.9)*thickness,color*.105,0);
            }else{
                hit=beam(ro,rd,foot,knee,thickness,color*.79,hit);
                hit=beam(ro,rd,knee,top,thickness*.68,color,hit);
                let junction=mix(knee,top,.43);
                let fork=rotateBranch(plan,select(-1.08,1.08,k%2u==0u));
                let tip=junction+fork*size*(.29+.22*r)+vec3f(0,height*(.32+.19*r),0);
                hit=beam(ro,rd,junction,tip,thickness*.44,color*1.18,hit);
                if(g.reefTerrain.x>.26&&kind!=2u){
                    let sideTip=mix(junction,tip,.42)-fork*size*.12+
                        rotateBranch(fork,1.51)*size*.19+vec3f(0,height*.24,0);
                    hit=beam(ro,rd,mix(junction,tip,.42),sideTip,thickness*.32,color*1.31,hit);
                }
                if(kind==2u&&k>0u){
                    let beside=rotateBranch(vec3f(2.0*(t-1.0/f32(branches-1u))-1.0,0,0),yaw);
                    let weave=base+beside*size*.68+vec3f(0,height*.71,0);
                    hit=beam(ro,rd,mix(knee,top,.29),weave,thickness*.38,color*.91,hit);
                }
            }
        }
    }
    if(hit.distance<initial.distance){
        let p=(ro+rd*hit.distance)-base;
        let pores=reefNoise(p.xz*26.4+p.y*vec2f(17.0,-24.0));
        let polyps=smoothstep(.56,.91,pores);
        let grooves=1.0-smoothstep(.035,.29,abs(sin(p.x*19.0+p.y*12.0+
            sin(p.z*15.7+sin(p.y*11.0)*1.7)*2.48)));
        let tissue=select(.84+.33*polyps,.88-.49*grooves+.27*polyps,kind==3u);
        let near=1.0-smoothstep(11.0,24.0,hit.distance);
        hit.color*=mix(1.0,tissue,near*g.reefTerrain.x);
        hit.color+=polyps*near*vec3f(.085,.066,.045)*g.reefTerrain.x;
    }
    return hit;
}
fn reefConnection(ro:vec3f,rd:vec3f,i:u32,base:vec3f,initial:MarineHit)->MarineHit{
    let key=siteKey(i);
    if(i+2u>=u32(g.reef.w)||seed(key+963u)>=g.reefTerrain.y){return initial;}
    let next=coralBase(i+2u);
    let mid=mix(base,next,.5);
    if(!reefBound(ro,rd,mid,vec3f(1.65,2.9,3.0),initial.distance)){return initial;}
    var hit=initial;
    let crust=mix(vec3f(.40,.17,.29),vec3f(.49,.34,.13),seed(key+13u));
    var a=base;
    for(var k=1u;k<=4u;k++){
        let t=f32(k)*.25;
        let p=mix(base.xz,next.xz,t)+vec2f(sin(t*3.141592)*(seed(key+533u)-.5)*.46,0);
        let b=vec3f(p.x,reefFloor(p)+.05,p.y);
        hit=beam(ro,rd,a,b,.085+.065*seed(key*3u+k),crust,hit);
        let tip=b+vec3f((seed(key+k+86u)-.5)*.43,.22+seed(key*5u+k)*.44,.15);
        hit=beam(ro,rd,b,tip,.084,crust*1.64,hit);
        hit=beam(ro,rd,mix(b,tip,.54),tip+vec3f(.17,.11,-.14),.059,crust*1.52,hit);
        a=b;
    }
    return hit;
}
fn kelpRoot(i:u32)->vec3f{
    let side=select(-1.0,1.0,i%2u==1u);
    let row=reefRow(i);
    let key=siteKey(i);
    let z=select(-3.9+row*4.6,row*4.35,inCanyon())+side*1.36+seed(key+272u)*2.28;
    let margin=select(3.32,g.reefTerrain.z*.83,inCanyon());
    let x=reefAxis(z)+side*(margin+.83*seed(key+240u));
    return vec3f(x,reefFloor(vec2f(x,z))-.04,z);
}
fn kelpShape(root:vec3f,key:u32,h:f32,height:f32,age:f32)->vec3f{
    let flow=swell(root.xz+vec2f(-h*2.1,h*5.7));
    let arc=vec2f(sin(f32(key)*.92+h*2.1),cos(f32(key)*.63-h*1.8))*.075;
    let bend=(arc+flow*(g.water.x*.36))*h*h*height;
    let stretch=1.0-.075*abs(flow.x)*h-.30*max(0.0,age);
    return vec3f(bend.x,h*height*stretch,bend.y);
}
fn kelpBlade(ro:vec3f,rd:vec3f,a:vec3f,b:vec3f,width:f32,twist:f32,color:vec3f,hit:MarineHit)->MarineHit{
    let delta=b-a;
    let axis=normalize(delta);
    let side=normalize(cross(axis,vec3f(0,1,.02)));
    let normal=cross(side,axis);
    let wide=side*cos(twist)+normal*sin(twist);
    let up=cross(wide,axis);
    let q=ro-.5*(a+b);
    return marinePart(hit,vec3f(dot(q,axis),dot(q,wide),dot(q,up)),
        vec3f(dot(rd,axis),dot(rd,wide),dot(rd,up)),vec3f(0),
        vec3f(length(delta)*.62,width,max(width*.24,.006)),color,.19);
}
fn kelpPlant(ro:vec3f,rd:vec3f,root:vec3f,i:u32,growth:f32,age:f32,initial:MarineHit)->MarineHit{
    let key=siteKey(i);
    let height=min(select(11.6-root.y,7.0,age>=0.0),3.35+seed(key+291u)*3.4)*growth;
    if(height<.035){return initial;}
    let broad=height*(.48+g.water.x*g.reefCurrent.y*.43)+.46;
    if(!reefBound(ro,rd,root+vec3f(0,height*.45,0),vec3f(broad,height*.71,broad),initial.distance)){
        return initial;
    }
    var hit=initial;
    var a=root;
    let verdigris=mix(vec3f(.19,.31,.049),vec3f(.062,.45,.22),seed(key+408u));
    let flow=swell(root.xz);
    for(var n=1u;n<=10u;n++){
        let h=f32(n)/10.0;
        let b=root+kelpShape(root,key,h,height,age);
        hit=beam(ro,rd,a,b,mix(.076,.023,h)*sqrt(growth),verdigris*.76,hit);
        if(n>1u){
            let s=select(-1.0,1.0,n%2u==0u);
            let bias=sin(f32(key)*.74+f32(n)*1.91);
            let leaf=vec3f(s*(.62+bias*.19)+flow.x*.33,.33+.15*bias,bias*.54+flow.y*.41)
                *(1.26-.73*h)*sqrt(growth)*height*.36;
            let middle=b+leaf*.59+vec3f(0,.07,0);
            let point=b+leaf+vec3f(0,-.23*growth,bias*.14);
            let twist=1.04*bias+.42*flow.x;
            let width=(.127+.058*bias)*growth;
            hit=kelpBlade(ro,rd,b,middle,width,twist,verdigris*(1.0+h*.42),hit);
            hit=kelpBlade(ro,rd,middle,point,width*.68,twist+.36,verdigris*.97,hit);
        }
        a=b;
    }
    return hit;
}
fn school(ro:vec3f,rd:vec3f,i:u32,base:vec3f,initial:MarineHit)->MarineHit{
    if(!reefBound(ro,rd,base+vec3f(0,1.85,.46),vec3f(5.2,3.6,4.3),initial.distance)){
        return initial;
    }
    var hit=initial;
    let key=siteKey(i);
    let side=select(-1.0,1.0,i%2u==1u);
    let flow=swell(base.xz)*select(.42,.81,inCanyon());
    let t=g.marine.z;
    for(var k=0u;k<u32(g.reef.z);k++){
        let chance=seed(key*23u+k+500u);
        let stagger=f32(k)*.44+chance*.18;
        let phase=t*(.62+f32(key%3u)*.09)+f32(key%79u)*1.9-stagger;
        let radius=1.23+chance*1.52;
        var p=base+vec3f(cos(phase)*radius-side*.79+flow.x,
            1.39+chance*1.66+sin(phase*1.42)*.30,sin(phase)*radius*.8+.65+flow.y);
        p.y=max(p.y,reefFloor(p.xz)+.43);
        let heading=vec3f(-sin(phase)+.16*flow.x,cos(phase*1.42)*.14,cos(phase));
        let next=marineAnimal(ro,rd,p,heading,.23+.37*chance,(key*2u+k/2u)%6u,t*4.7+stagger,hit.distance);
        if(next.distance<hit.distance){hit=next;}
    }
    if(i<u32(g.marine.w)){
        let phase=g.viewport.z*.37+f32(key%89u)*3.5;
        var p=base+vec3f(cos(phase)*.82+flow.x*.32,.96+sin(phase*.91)*.34,sin(phase)*.97-.39);
        p.y=max(p.y,reefFloor(p.xz)+.37);
        let next=marineAnimal(ro,rd,p,vec3f(-sin(phase),.035,cos(phase)),.52,9u,phase*6.0,hit.distance);
        if(next.distance<hit.distance){hit=next;}
    }
    return hit;
}
fn timedAnimal(v:ReefView,ray:vec3f,initial:MarineHit)->MarineHit{
    let e=u32(g.marine.x);
    let phase=g.marine.y;
    var hit=initial;
    if(e==2u){
        for(var k=0u;k<3u;k++){
            let j=f32(k);
            let p=vec3f(mix(-20.0,21.0,phase)-j*2.7,5.1+sin(phase*7.3+j)*.83,16.0+j*3.35);
            let next=marineAnimal(v.eye,ray,p,vec3f(1.0,cos(phase*7.3+j)*.19,.18),1.38+j*.09,
                6u,g.viewport.z*3.6-j*.9,hit.distance);
            if(next.distance<hit.distance){hit=next;}
        }
    }else if(e==3u){
        let p=vec3f(mix(-108.0,108.0,phase),12.6+sin(phase*5.0)*1.5,39.0);
        let next=marineAnimal(v.eye,ray,p,vec3f(1.0,.024,.05),44.0,7u,phase*12.0,hit.distance);
        if(next.distance<hit.distance){hit=next;}
    }else if(e==4u){
        let approach=smoothstep(0.0,.76,phase);
        let rise=smoothstep(.68,1.0,phase);
        let p=vec3f(mix(3.0,v.eye.x+.16,approach)+rise*1.25,
            mix(5.2,v.eye.y-.42,approach)+rise*10.0,mix(67.0,v.eye.z+2.5,approach)+rise*1.4);
        let heading=normalize(vec3f(.56+rise*.28,.07+rise*1.35,-.83+rise*.39));
        let next=marineAnimal(v.eye,ray,p,heading,2.27,8u,g.viewport.z*2.65,hit.distance);
        if(next.distance<hit.distance){hit=next;}
    }
    return hit;
}
fn bubbles(v:ReefView,uv:vec2f,origin:vec3f,key:f32,limit:f32)->f32{
    if(g.water.w<=0.0){return 0.0;}
    let cycle=g.viewport.z*g.water.w+key*1.89;
    var sum=0.0;
    for(var i=0u;i<4u;i++){
        let clock=cycle+f32(i)*.25;
        let r=hash(floor(clock)+key+f32(i)*3.49);
        let age=fract(clock);
        let world=origin+vec3f(sin(age*7.0+key)*.22,age*(1.88+1.3*r),cos(age*3.8+key)*.31);
        let rel=world-v.eye;
        let depth=dot(rel,v.forward);
        if(depth<=.14||depth>=limit){continue;}
        let point=vec2f(dot(rel,v.right),dot(rel,v.up))*v.focal/depth;
        let radius=max(.0012,(.029+.061*r)*v.focal/depth);
        let a=(uv-point)/radius;
        let border=exp(-abs(length(a)-.82)*6.4);
        let light=exp(-dot(a-vec2f(-.32,.33),a-vec2f(-.32,.33))*9.0);
        sum+=(border*.28+light*.59)*sin(age*3.14159)*exp(-depth*.033);
    }
    return sum;
}
@fragment fn fragment(@builtin(position) position:vec4f)->@location(0) vec4f{
    let uv=(position.xy/g.viewport.xy*2.0-1.0)*vec2f(g.viewport.w,-1);
    let v=reefView();
    let ray=normalize(v.forward*v.focal+v.right*uv.x+v.up*uv.y);
    let t=g.viewport.z;
    let teal=mix(vec3f(.01,.18,.30),tint(g.output.x)*.29,.19);
    let sunlight=exp(-length(ray-vec3f(-.14,.32,.94))*4.1);
    let shift=swell(v.eye.xz+ray.xz*9.0);
    let shafts=pow(max(0.0,.48+.52*sin(uv.x*8.6+uv.y*3.5-shift.x*.85)),6.0);
    var color=vec3f(.005,.033,.066)+teal*(.46+sunlight*1.47+shafts*max(0.0,uv.y+.37)*.24);
    let surfaceDist=(12.75-v.eye.y)/max(.001,ray.y);
    let viewDistance=104.0;
    let waterLimit=min(viewDistance,surfaceDist);
    if(surfaceDist<viewDistance){
        let glass=glassSurface(v.eye,ray,surfaceDist);
        color=mix(color,glass,exp(-surfaceDist*.023)*smoothstep(.045,.28,ray.y));
    }
    let floorDepth=floorHit(v.eye,ray,waterLimit);
    var hit=MarineHit(floorDepth,color);
    if(floorDepth<waterLimit){
        let p=v.eye+ray*floorDepth;
        hit.color=floorColor(p,ray);
    }
    var bubbleLight=0.0;
    for(var i=0u;i<u32(g.reef.w);i++){
        let base=coralBase(i);
        let key=siteKey(i);
        hit=coral(v.eye,ray,key,base,1.0,hit);
        if(g.reefTerrain.x>.2){
            let bank=sign(base.x-reefAxis(base.z));
            let offset=vec2f(-bank*(.66+1.54*seed(key+673u)),.75+1.21*seed(key+908u));
            let p=base.xz+offset;
            let root=vec3f(p.x,reefFloor(p)-.07,p.y);
            hit=coral(v.eye,ray,key+2447u,root,.67+.21*seed(key+195u),hit);
        }
        hit=reefConnection(v.eye,ray,i,base,hit);
        hit=school(v.eye,ray,i,base,hit);
    }
    if(!inCanyon()&&g.reefTerrain.x>.2){
        for(var i=0u;i<min(6u,u32(g.reef.w));i++){
            let side=select(-1.0,1.0,i%2u==1u);
            let z=-3.24+f32(i/2u)*3.58+side*.91+seed(i+4261u)*.77;
            let x=reefAxis(z)+side*(.74+seed(i+5683u)*.92);
            let p=vec2f(x,z);
            hit=coral(v.eye,ray,i+6377u,vec3f(x,reefFloor(p)-.08,z),.59+seed(i+970u)*.29,hit);
        }
    }
    for(var i=0u;i<u32(g.reef.y);i++){
        let root=kelpRoot(i);
        hit=kelpPlant(v.eye,ray,root,i,g.kelp[i/4u][i%4u],-1.0,hit);
    }
    let detached=g.detachedKelp.y;
    if(detached>=0.0){
        let i=u32(g.detachedKelp.x);
        let origin=kelpRoot(i)+vec3f(detached*4.4,detached*13.0,detached*3.8);
        hit=kelpPlant(v.eye,ray,origin,i,1.0,detached,hit);
    }
    hit=timedAnimal(v,ray,hit);
    if(hit.distance<waterLimit){
        let p=v.eye+ray*hit.distance;
        let light=caustics(p.xz+vec2f(p.y*.16,-p.y*.13));
        let transmission=exp(-hit.distance*vec3f(.066,.030,.020));
        let refracted=hit.color*(.93+light*.77)+vec3f(.048,.11,.105)*light*.50;
        color=mix(color,refracted,transmission);
    }
    if(g.water.w>0.0){
        let limit=hit.distance*dot(v.forward,ray);
        for(var i=0u;i<u32(g.reef.y);i+=2u){
            let root=kelpRoot(i)+vec3f(0,1.6*g.kelp[i/4u][i%4u],0);
            bubbleLight+=bubbles(v,uv,root,f32(i)+.7,limit);
        }
        for(var i=0u;i<u32(g.reef.w);i+=2u){
            let a=g.marine.z*.82+f32(i)*1.9;
            let animal=coralBase(i)+vec3f(cos(a)*1.55,1.86,sin(a)*1.23+.65);
            bubbleLight+=bubbles(v,uv,animal,f32(i)+13.0,limit);
        }
        if(u32(g.marine.x)==2u){
            let dolphin=vec3f(mix(-20.0,21.0,g.marine.y),5.2+sin(g.marine.y*7.3)*.83,16.0);
            bubbleLight+=bubbles(v,uv,dolphin,31.2,limit);
        }
    }
    color+=min(bubbleLight,2.0)*vec3f(.39,.83,.84);
    let moteGrid=(uv+vec2f(t*.006,-t*.027))*vec2f(82.0,72.0);
    let mote=hash(floor(moteGrid.x)+floor(moteGrid.y)*113.0);
    color+=smoothstep(.966,1.0,mote)*(1.0-smoothstep(.03,.21,length(fract(moteGrid)-.5)))*
        vec3f(.045,.16,.17);
    let rgb=displayColor(color*(1.0-.19*clamp(dot(uv,uv)*.26,0.0,1.0)),g.output);
    return vec4f(modeBadge(rgb.rgb,position.xy,g.viewport.xy,g.view.y),1);
}
