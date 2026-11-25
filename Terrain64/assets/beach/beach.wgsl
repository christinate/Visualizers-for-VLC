// SPDX-License-Identifier: MIT
// Coastal surfaces, world-space scenery, breakers and precipitation.
struct View { eye:vec3f, forward:vec3f, right:vec3f, up:vec3f, focal:f32 };
fn turnCamera(front:vec3f,aim:vec3f,blend:f32)->vec3f{
    let angle=atan2(front.x,front.z);
    let turn=atan2(aim.x,aim.z)-angle;
    let heading=angle+atan2(sin(turn),cos(turn))*blend;
    let tilt=mix(-asin(clamp(front.y,-1.0,1.0)),-asin(clamp(aim.y,-.98,.98)),blend);
    return vec3f(sin(heading)*cos(tilt),-sin(tilt),cos(heading)*cos(tilt));
}
fn view()->View{
    let yaw=g.view.x;let pitch=g.view.y;
    var front=vec3f(sin(yaw)*cos(pitch),-sin(pitch),cos(yaw)*cos(pitch));
    var eye=g.camera.xyz;
    let bank=eye.x-shore(eye.z);
    if(bank>0.0){eye.y=max(eye.y,duneSand(bank,eye.z)+.48);}
    let follow=g.view.w;
    if(follow>0.0){
        let turtle=turtlePose(u32(g.beachFollow.x));
        let swim=normalize(vec3f(turtle.heading.x,0,turtle.heading.z));
        let side=vec3f(-swim.z,0,swim.x);
        let distance=g.beachFollow.y;
        var chase=turtle.center+distance*(-swim*.64+side*.77);
        chase.x=min(chase.x,shore(chase.z)-4.25);
        let cameraLift=mix(1.13,.43,smoothstep(.15,1.42,turtle.depth));
        chase.y=max(turtle.center.y+cameraLift,-bottomDepth(chase.xz)+1.12);
        let focus=turtle.center+swim*.34+vec3f(0,-.17,0);
        front=turnCamera(front,normalize(focus-chase),follow);
        eye=mix(eye,chase,follow);
    }
    let preview=g.beachDolphin;
    if(preview.w>0.0){
        let phase=g.wildlife.x;
        let path=dolphinTrack(.5+(phase-.5)*.67);
        let focus=vec3f(path.x,.73+1.16*sin(phase*3.14159),path.y);
        let camera=vec3f(preview.x+5.8,3.65,preview.y-1.34*preview.z);
        front=turnCamera(front,normalize(focus-camera),preview.w);
        eye=mix(eye,camera,preview.w);
    }
    let right=normalize(cross(vec3f(0,1,0),front));
    return View(eye,front,right,cross(front,right),mix(mix(g.camera.w,1.08,follow),1.56,preview.w));
}
fn project(v:View,world:vec3f)->vec3f{
    let d=world-v.eye;
    let depth=dot(d,v.forward);
    return vec3f(vec2f(dot(d,v.right),dot(d,v.up))*v.focal/max(.01,depth),depth);
}
fn shore(z:f32)->f32{
    return g.features.z*(sin(z*.12)+.65*sin(z*.041+.42));
}
fn sandY(x:f32)->f32{return clamp(x*.065,0.0,.7);}
fn duneSand(x:f32,z:f32)->f32{
    let belt=smoothstep(1.35,4.1,x)*(1.0-.86*smoothstep(8.2,16.0,x));
    let drift=sin(z*.31)*.72+sin(z*.097+.83)*1.16;
    let crest=.5+.5*sin(x*.83+z*.115+drift);
    let secondary=.5+.5*sin(x*.32-z*.49+.62*sin(z*.27));
    return sandY(x)+g.beachDetail.y*belt*(.76*pow(crest,2.25)+.24*secondary);
}
fn curve(p:vec2f,a:vec2f,b:vec2f)->f32{
    let d=b-a;
    return length(p-a-d*clamp(dot(p-a,d)/max(.000001,dot(d,d)),0.0,1.0));
}
fn mask(d:f32,a:f32)->f32{return 1.0-smoothstep(-a,a,d);}
fn feature(index:u32)->bool{return (u32(g.features.x)&(1u<<index))!=0u;}
fn waveSample(z:f32)->f32{
    let phase=fract(z*.026);
    let bin=u32(phase*32.0);
    let next=(bin+1u)%32u;
    return mix(g.waveform[bin/4u][bin%4u],g.waveform[next/4u][next%4u],fract(phase*32.0));
}
fn sky(direction:vec3f)->vec3f{
    let h=max(0.0,direction.y);
    let bearing=atan2(direction.x,direction.z+.000001);
    let lateral=.5+.5*sin(bearing+g.output.x*5.4);
    let horizon=exp(-abs(direction.y)*5.2);
    let pastel=mix(vec3f(.35,.055,.27),vec3f(.17,.21,.72),lateral);
    let amber=mix(vec3f(.72,.20,.115),vec3f(.061,.39,.51),lateral);
    let zenith=mix(vec3f(.047,.022,.29),vec3f(.025,.18,.48),.5+.5*sin(bearing-.73));
    let chroma=tint(g.output.x+.13+direction.x*.15);
    var color=mix(zenith,pastel,horizon*.79);
    color+=amber*exp(-abs(direction.y-.032)*13.5)*.64;
    let cloudPlane=direction.xz*4.75+vec2f(h*2.4,h*11.0);
    let drift=vec2f(g.viewport.z*.031,-g.viewport.z*.012);
    let cloud=stoneNoise(cloudPlane+drift)*.75+stoneNoise(cloudPlane*2.8-drift)*.25;
    let haze=smoothstep(.48,.77,cloud)*smoothstep(.015,.15,h)*(1.0-smoothstep(.36,.84,h));
    color+=haze*(mix(vec3f(.39,.077,.29),vec3f(.054,.29,.56),lateral)+chroma*.17);
    color=mix(vec3f(.016,.025,.057)+chroma*.048,color,g.beachDetail.z*.84);
    let sun=normalize(vec3f(-.22,.095,1));
    let sunD=length(direction-sun);
    let cut=select(.34,1.0,fract(direction.y*176.0)>.24||direction.y>.104);
    color+=(1.0-smoothstep(.072,.078,sunD))*cut*tint(g.output.x+.43)*1.3;
    color+=exp(-sunD*8.0)*tint(g.output.x+.42)*.32;
    let point=direction.xy*196.0;
    let star=hash(floor(point.x)+317.0*floor(point.y)+g.features.y);
    color+=smoothstep(.981,1.0,star)*smoothstep(.21,.79,h)*mask(length(fract(point)-.5)-.15,.06)*.38;
    return color;
}
fn foam(x:f32,z:f32)->vec2f{
    var crest=0.0;
    var spray=0.0;
    let waveform=waveSample(z)*g.geometry.w;
    for(var index=0u;index<24u;index++){
        let wave=g.waves[index];
        if(wave.x<0.0){continue;}
        let outgoing=wave.x>1.0;
        let approach=clamp(wave.x,0.0,1.0);
        let retreat=clamp((wave.x-1.0)*g.detail.x/g.weather.w,0.0,1.0);
        var xCrest=mix(-g.geometry.x,g.geometry.y,approach*(2.0-approach));
        if(outgoing){xCrest=g.geometry.y-(g.geometry.y+g.detail.y)*retreat;}
        let lift=(wave.z*.5+g.audio.x*.5)*g.features.w;
        xCrest+=lift*approach*(1.0-retreat);
        xCrest+=(waveform+sin(z*.18+wave.w*6.28319)*.58)*(.27+.73*approach);
        let width=g.geometry.z*(.52+.75*approach+.55*retreat);
        let fade=(.27+.73*approach)*(1.0-retreat)*(1.0-retreat)*wave.y;
        let distance=abs(x-xCrest)/width;
        crest=max(crest,exp(-distance*distance*3.4)*fade);
        spray=max(spray,exp(-distance*.9)*fade);
    }
    let pulse=g.effects.x;
    if(pulse>0.0){
        let phase=1.0-g.audio.y;
        let impact=clamp((phase-.18)*1.4,0.0,1.0);
        let breakerX=mix(-10.0,2.0,smoothstep(0.0,.25,phase))-impact*9.0;
        let bank=exp(-abs(z-g.effects.w)*(.14+impact*.15));
        let d=abs(x-breakerX-waveform*.54);
        crest=max(crest,exp(-d*d*.95)*pulse*bank*1.3);
        spray=max(spray,exp(-d*.44)*pulse*bank);
    }
    return vec2f(crest,spray);
}
fn hashGrid(p:vec2f)->f32{
    return hash(dot(p,vec2f(71.9,157.3))+g.features.y*.61);
}
fn stoneNoise(p:vec2f)->f32{
    let cell=floor(p);
    let f=fract(p);
    let s=f*f*(3.0-2.0*f);
    return mix(mix(hashGrid(cell),hashGrid(cell+vec2f(1,0)),s.x),
               mix(hashGrid(cell+vec2f(0,1)),hashGrid(cell+vec2f(1)),s.x),s.y);
}
fn sandMaterial(p:vec2f,depth:f32)->vec3f{
    let shade=stoneNoise(p*.23);
    let grain=(stoneNoise(p*8.0)*.65+hashGrid(floor(p*25.0))*.35)-.5;
    let sand=vec3f(.408,.285,.193)+tint(g.output.x+.35)*.064;
    let pattern=select(g.beachDetail.w,.29,depth>.05);
    return sand*(.83+.32*shade+.23*grain*pattern)*mix(1.0,.63,smoothstep(0.0,5.0,depth));
}
fn shorelineFoam(p:vec2f,coastX:f32)->vec3f{
    let x=coastX;
    let wave=foam(x,p.y);
    let aqua=mix(vec3f(.13,.42,.44),tint(g.output.x-.03),.32);
    let salt=vec3f(.65,.91,1.0);
    let drift=vec2f(g.viewport.z*.39,-g.viewport.z*.15);
    let bubbles=stoneNoise(p*vec2f(8.9,4.1)+drift);
    let filaments=sin(p.y*9.4+sin(p.x*12.4)+bubbles*6.0)*sin(p.x*16.6-p.y*2.3);
    let lace=.31+.69*smoothstep(-.22,.66,filaments+bubbles*.43);
    var impactFoam=0.0;
    for(var i=0u;i<4u;i++){
        let impact=g.impacts[i];
        if(impact.y<0.0||impact.y>=1.0){continue;}
        let a=impact.y;
        let offset=vec2f(x+.6+a*3.7,p.y-impact.x);
        let d=length(offset*vec2f(1.0,.75))/(.9+a*4.8);
        let gaps=.45+.55*smoothstep(-.5,.65,filaments);
        impactFoam+=exp(-d*d*2.3)*(1.0-a)*impact.z*gaps*(1.0-smoothstep(.7,3.4,x));
    }
    let wash=exp(-abs(x-.08-bubbles*.28)*3.5);
    return aqua*.30*wave.y+salt*(wave.x*lace*.89+impactFoam*(.23+.54*lace)+wash*lace*.14);
}
struct PropHit { distance:f32, color:vec3f };
fn mountainHeight(x:f32,z:f32)->f32{
    let inland=x-shore(z)-g.mountains.x;
    let span=g.mountains.z;
    let crest=smoothstep(0.0,span*.55,inland)*(1.0-.59*smoothstep(span*.66,span,inland));
    let phase=z*.12;
    let peaks=.69+.20*sin(phase)+.15*sin(phase*2.42+1.6)+.09*sin(phase*5.56-.61);
    let rumble=g.mountains.w*(g.audio.x*.76+.24)*(
        sin(z*.24+g.viewport.z*.88-inland*.17)*.065+
        sin(z*.49-g.viewport.z*1.19+inland*.21)*.032);
    let flank=sin(inland*.29+phase*1.7)*sin(phase*3.15-inland*.49)*.10;
    return .69+crest*g.mountains.y*max(.08,peaks+rumble+flank);
}
fn mountainRange(v:View,dir:vec3f,limit:f32,background:vec3f)->PropHit{
    let height=g.mountains.y;
    if(height<=0.0){return PropHit(limit,background);}
    let startX=g.mountains.x-1.65*g.features.z;
    let endX=g.mountains.x+g.mountains.z+1.65*g.features.z;
    if((v.eye.x<startX&&dir.x<=.0001)||(v.eye.x>endX&&dir.x>=-.0001)){return PropHit(limit,background);}
    let safeX=select(min(dir.x,-.0001),max(dir.x,.0001),dir.x>=0.0);
    let a=(startX-v.eye.x)/safeX;let b=(endX-v.eye.x)/safeX;
    var t=max(.5,min(a,b));
    let stop=min(min(limit,max(a,b)),g.land.w*1.4);
    if(t>=stop){return PropHit(limit,background);}
    if(dir.y>=0.0&&v.eye.y+dir.y*t>height*1.36+.7){return PropHit(limit,background);}
    var previous=t;
    for(var i=0u;i<37u;i++){
        if(t>stop){break;}
        let p=v.eye+dir*t;
        let inland=p.x-shore(p.z)-g.mountains.x;
        let elevation=mountainHeight(p.x,p.z);
        let d=p.y-elevation;
        if(d<=.035&&inland>=0.0&&inland<=g.mountains.z){
            var lo=previous;var hi=t;
            for(var j=0u;j<5u;j++){
                let mid=(lo+hi)*.5;
                let q=v.eye+dir*mid;
                if(q.y>mountainHeight(q.x,q.z)){lo=mid;}else{hi=mid;}
            }
            let point=v.eye+dir*hi;
            let e=.35;
            let nx=mountainHeight(point.x-e,point.z)-mountainHeight(point.x+e,point.z);
            let nz=mountainHeight(point.x,point.z-e)-mountainHeight(point.x,point.z+e);
            let n=normalize(vec3f(nx,2.0*e,nz));
            let lit=.29+.71*max(0.0,dot(n,normalize(vec3f(-.57,.71,.41))));
            let clefts=stoneNoise(point.xz*.48+point.y*.32);
            let bands=.5+.5*sin(point.y*1.91+point.z*.37+clefts*2.1);
            let rock=mix(tint(g.output.x-.13),tint(g.output.x+.46),bands*.79);
            let rim=pow(1.0-abs(dot(n,dir)),2.8);
            let stone=vec3f(.024,.024,.051)+rock*(.27*lit+.16*rim)*
                mix(1.0,.72+.56*clefts,g.beachDetail.x);
            return PropHit(hi,mix(background,stone,exp(-hi/g.land.w*.82)));
        }
        previous=t;
        t+=clamp(d*.44,.66,5.8);
    }
    return PropHit(limit,background);
}
fn litProp(base:vec3f,n:vec3f,accent:vec3f)->vec3f{
    let diffuse=max(0.0,dot(n,normalize(vec3f(-.56,.76,.45))));
    return base*(.39+.61*diffuse)+accent*pow(1.0-abs(n.y),3.0)*.14;
}
fn solidBox(hit:PropHit,origin:vec3f,dir:vec3f,inv:vec3f,center:vec3f,halfSize:vec3f,color:vec3f,accent:vec3f)->PropHit{
    let a=(center-halfSize-origin)*inv;
    let b=(center+halfSize-origin)*inv;
    let lo=min(a,b);let hi=max(a,b);
    let t=max(lo.x,max(lo.y,lo.z));
    if(t<=.25||t>=hit.distance||t>min(hi.x,min(hi.y,hi.z))){return hit;}
    let n=-sign(dir)*vec3f(select(0.0,1.0,lo.x==t),select(0.0,1.0,lo.y==t),select(0.0,1.0,lo.z==t));
    return PropHit(t,litProp(color,n,accent));
}
fn solidOval(hit:PropHit,origin:vec3f,dir:vec3f,center:vec3f,radii:vec3f,color:vec3f,accent:vec3f)->PropHit{
    let relative=(origin-center)/radii;
    let ray=dir/radii;
    let a=dot(ray,ray);let b=dot(relative,ray);
    let span=b*b-a*(dot(relative,relative)-1.0);
    if(span<=0.0){return hit;}
    let t=(-b-sqrt(span))/a;
    if(t<=.25||t>=hit.distance){return hit;}
    let p=origin+dir*t-center;
    return PropHit(t,litProp(color,normalize(p/(radii*radii)),accent));
}
struct Clip { near:f32, far:f32, normal:vec3f };
fn clipPlane(clip:Clip,eye:vec3f,ray:vec3f,n:vec3f,offset:f32)->Clip{
    let gap=offset-dot(n,eye);
    let heading=dot(n,ray);
    if(abs(heading)<.00001){
        if(gap<0.0){return Clip(1.0,0.0,n);}
        return clip;
    }
    let t=gap/heading;
    if(heading>0.0){return Clip(clip.near,min(clip.far,t),clip.normal);}
    if(t>clip.near){return Clip(t,clip.far,n);}
    return clip;
}
fn solidRock(hit:PropHit,eye:vec3f,ray:vec3f,center:vec3f,size:vec3f,seed:f32)->PropHit{
    let a=seed*6.283185;
    let variety=u32(seed*6.0);
    let delta=(eye-center);
    let q=rotate2(delta.xz,-a);
    let d=rotate2(ray.xz,-a);
    let origin=vec3f(q.x,delta.y,q.y)/size;
    let direction=vec3f(d.x,ray.y,d.y)/size;
    // Test a conservative rounded bound before clipping the facets.
    let aa=dot(direction,direction);
    let bb=dot(origin,direction);
    let cc=dot(origin,origin)-3.0;
    let discriminant=bb*bb-aa*cc;
    if(discriminant<0.0){return hit;}
    if(bb>=0.0&&cc>0.0){return hit;}
    let near=(-bb-sqrt(discriminant))/aa;
    if(near>=hit.distance){return hit;}
    let planes=array<vec4f,11>(
        vec4f(1,.09,.13,.93),vec4f(-1,.22,-.09,.85),vec4f(.16,.23,1,.90),
        vec4f(-.17,.31,-1,.88),vec4f(.19,1,.10,.94),vec4f(0,-1,0,.80),
        vec4f(.69,.84,-.38,.97),vec4f(-.63,.84,.51,1.02),vec4f(.52,.54,.77,1.0),
        vec4f(-.47,.32,-.87,1.0),vec4f(-.71,-.21,.72,1.05));
    var clip=Clip(-1.0,hit.distance,vec3f(0,1,0));
    if(variety==1u||variety==4u){
        let radius=select(vec3f(1,.90,1),vec3f(1.14,.75,.87),variety==4u);
        let t=ellipsoid(origin,direction,vec3f(0),radius,hit.distance);
        if(t>=hit.distance){return hit;}
        clip=Clip(t,hit.distance,(origin+direction*t)/(radius*radius));
    }else{
        for(var i=0u;i<11u;i++){
            let plane=planes[i];
            let jitter=fract(seed*13.73+f32(i)*.348)-.5;
            let face=select(plane.xyz,plane.xyz+vec3f(jitter*.44,0,-jitter*.34),variety==3u||variety==5u);
            let cut=select(select(.27,.59,variety>=3u),.105,variety==2u);
            let shelf=select(select(0.0,-.24*max(plane.y,0.0),variety==2u),
                              .19*plane.y-.16*plane.z,variety==5u);
            clip=clipPlane(clip,origin,direction,face,plane.w+jitter*cut+shelf);
            if(clip.near>=clip.far){return hit;}
        }
    }
    if(clip.near<=.018){return hit;}
    let p=eye+ray*clip.near;
    let local=origin+direction*clip.near;
    let normal=clip.normal/size;
    let world=rotate2(normal.xz,a);
    let n=normalize(vec3f(world.x,normal.y,world.y));
    let grain=stoneNoise(local.xz*12.0+local.y*5.6+seed*9.0);
    let cracks=stoneNoise(local.xy*4.1+local.z*vec2f(-2,4));
    let strata=abs(sin(local.y*(16.0+12.0*seed)+local.x*(9.0*seed-2.0)+cracks*2.6));
    let pits=smoothstep(.12,.54,grain);
    let strataWeight=select(.27,.53,variety==2u);
    let surfaceGrain=mix(1.0,.51+pits*.43+smoothstep(.025,.24,strata)*strataWeight,g.beachDetail.x);
    let wet=1.0-smoothstep(-.22,.57,p.y);
    let deposits=array<vec3f,6>(vec3f(.33,.19,.104),vec3f(.18,.23,.235),vec3f(.26,.265,.27),
        vec3f(.264,.129,.082),vec3f(.39,.356,.264),vec3f(.105,.108,.15));
    let mineral=deposits[variety]*(.77+.40*cracks);
    let vein=mix(vec3f(.21,.14,.080),tint(g.output.x+.18)*.12,.34)*cracks;
    let algae=smoothstep(.43,.82,cracks)*(.16+.33*wet)*max(n.y,0.0);
    let gleam=pow(max(0.0,dot(reflect(ray,n),normalize(vec3f(-.46,.72,.44)))),36.0);
    let stone=(mineral+vein)*surfaceGrain*(1.0-.34*wet)+vec3f(.060,.18,.12)*algae;
    let baseShade=mix(.67,1.0,smoothstep(-.35,.12,local.y));
    return PropHit(clip.near,litProp(stone*baseShade,n,vein)+vec3f(.19,.44,.49)*gleam*wet);
}
fn gable(hit:PropHit,eye:vec3f,ray:vec3f,center:vec3f,size:vec3f,crosswise:bool,color:vec3f,edge:vec3f)->PropHit{
    let q=(eye-center)/size;
    let d=ray/size;
    let origin=select(q,q.zyx,crosswise);
    let direction=select(d,d.zyx,crosswise);
    let faces=array<vec4f,5>(vec4f(1,1,0,1),vec4f(-1,1,0,1),vec4f(0,-1,0,0),
                            vec4f(0,0,1,1),vec4f(0,0,-1,1));
    var clip=Clip(-1.0,hit.distance,vec3f(0,1,0));
    for(var i=0u;i<5u;i++){
        clip=clipPlane(clip,origin,direction,faces[i].xyz,faces[i].w);
        if(clip.near>=clip.far){return hit;}
    }
    if(clip.near<=.25){return hit;}
    let n=normalize(select(clip.normal,clip.normal.zyx,crosswise)/size);
    let p=origin+direction*clip.near;
    let tile=fract(p.z*19.0+.31*floor(p.y*10.0));
    let seams=smoothstep(.02,.09,tile)*(1.0-smoothstep(.87,.97,tile));
    let rows=.69+.31*smoothstep(.06,.18,fract(p.y*10.0));
    let finish=mix(1.0,(.78+.22*seams)*rows,g.beachDetail.x);
    return PropHit(clip.near,litProp(color*finish,n,edge));
}
fn palmLink(hit:PropHit,eye:vec3f,dir:vec3f,a:vec3f,b:vec3f,width:f32,thickness:f32,color:vec3f,edge:vec3f)->PropHit{
    let axis=normalize(b-a);
    let tangent=normalize(cross(axis,select(vec3f(0,1,0),vec3f(0,0,1),abs(axis.y)>.92)));
    let basis=transpose(mat3x3f(tangent,axis,cross(tangent,axis)));
    return solidOval(hit,basis*(eye-(a+b)*.5),basis*dir,vec3f(0),
                     vec3f(width,length(b-a)*.59,thickness),color,edge);
}
fn palmFrond(hit:PropHit,eye:vec3f,ray:vec3f,crown:vec3f,radial:vec3f,arch:f32,droop:f32,color:vec3f)->PropHit{
    let lateral=normalize(vec3f(-radial.z,0,radial.x));
    var nearest=hit;
    var a=crown;
    for(var part=0u;part<3u;part++){
        let segment=min(1.0,f32(part+1u)/3.0);
        let b=crown+radial*segment+vec3f(0,arch*sin(segment*3.141593)-droop*segment*segment,0);
        let axis=b-a;
        let normal=normalize(cross(lateral,axis));
        let facing=dot(ray,normal);
        if(abs(facing)>.0002){
            let t=dot(a-eye,normal)/facing;
            if(t>.025&&t<nearest.distance){
                let local=eye+ray*t-a;
                let along=dot(local,axis)/dot(axis,axis);
                if(along>=0.0&&along<=1.0){
                    let u=(f32(part)+along)/3.0;
                    let width=(.058+.26*pow(sin(u*3.14159),.83))*(.98-u*.38);
                    let offset=dot(local,lateral);
                    let v=abs(offset)/width;
                    let serration=fract(u*24.0-v*.83+select(0.0,.17,offset>0.0));
                    let rib=1.0-smoothstep(.03,.11,v);
                    let distanceFade=smoothstep(9.0,24.0,t);
                    let tipWidth=mix(.39,.17,v);
                    if(v<1.0&&(abs(serration-.5)<tipWidth||rib>.05||distanceFade>.58)){
                        let vein=1.0-smoothstep(.045,.17,abs(serration-.5));
                        let sideShade=select(.81,1.06,offset>0.0);
                        let leaf=color*sideShade*(.67+.30*u+.18*vein)+vec3f(.17,.145,.057)*rib;
                        nearest=PropHit(t,litProp(leaf,normal,color));
                    }
                }
            }
        }
        a=b;
    }
    return nearest;
}
fn sling(hit:PropHit,eye:vec3f,ray:vec3f,a:vec3f,b:vec3f,width:f32,color:vec3f)->PropHit{
    let span=b-a;
    let normal=normalize(vec3f(-span.y,span.x,0));
    let face=dot(ray,normal);
    if(abs(face)<.0001){return hit;}
    let distance=dot(a-eye,normal)/face;
    if(distance<=.17||distance>=hit.distance){return hit;}
    let p=eye+ray*distance-a;
    let along=dot(p,span)/dot(span,span);
    if(along<0.0||along>1.0||abs(p.z)>width){return hit;}
    let band=.70+.30*smoothstep(.13,.33,fract(along*8.0));
    return PropHit(distance,litProp(color*band,normal,color));
}
fn beachChair(v:View,dir:vec3f,initial:PropHit)->PropHit{
    if(g.beachVisitor.w<.5){return initial;}
    let z=g.beachVisitor.x;
    let x=4.52;
    let center=vec3f(shore(z)+x,duneSand(x,z),z);
    if(ellipsoid(v.eye,dir,center+vec3f(0,.55,0),vec3f(1.57,1.05,.64),initial.distance)>=initial.distance){return initial;}
    let relative=v.eye-center;
    let yaw=-.20+g.beachVisitor.z*.70;
    let p=rotate2(relative.xz,yaw);
    let ray=rotate2(dir.xz,yaw);
    let eye=vec3f(p.x,relative.y,p.y);
    let d=vec3f(ray.x,dir.y,ray.y);
    var hit=initial;
    let wood=vec3f(.44,.251,.106);
    let canvas=mix(vec3f(.055,.47,.51),tint(g.beachVisitor.z+g.output.x)*.5,.46);
    let skin=vec3f(.70,.385,.232);
    let shadedSkin=vec3f(.58,.292,.165);
    let shirt=mix(vec3f(.84,.592,.50),tint(g.beachVisitor.z+.12),.30);
    let shorts=vec3f(.082,.142,.235);
    let seat=vec3f(-.44,.43,0);
    let fold=vec3f(.25,.37,0);
    let back=vec3f(.81,1.15,0);
    hit=sling(hit,eye,d,seat,fold,.297,canvas);
    hit=sling(hit,eye,d,fold,back,.297,canvas);
    for(var i=0u;i<2u;i++){
        let sign=select(-1.0,1.0,i==1u);
        let rail=vec3f(0,0,sign*.305);
        let frame=array<vec3f,8>(seat+rail,fold+rail,fold+rail,back+rail,
            vec3f(-.38,.03,sign*.305),vec3f(.58,.84,sign*.305),
            vec3f(.65,.03,sign*.305),vec3f(-.37,.57,sign*.305));
        for(var n=0u;n<4u;n++){
            hit=palmLink(hit,eye,d,frame[n*2u],frame[n*2u+1u],.044,.038,wood,canvas);
        }
        hit=palmLink(hit,eye,d,vec3f(-.38,.66,sign*.322),vec3f(.49,.71,sign*.322),.055,.037,wood,canvas);
    }
    hit=palmLink(hit,eye,d,vec3f(.55,.88,-.309),vec3f(.55,.88,.309),.039,.031,wood,canvas);
    hit=palmLink(hit,eye,d,vec3f(.10,.48,0),vec3f(.50,.91,0),.209,.119,shirt,shirt);
    hit=solidOval(hit,eye,d,vec3f(.075,.49,0),vec3f(.224,.111,.172),shorts,shirt);
    for(var i=0u;i<2u;i++){
        let sign=select(-1.0,1.0,i==1u);
        let legZ=sign*.098;
        let knee=vec3f(-.57,.64+sign*.071,legZ);
        let ankle=vec3f(-1.13,.31+sign*.127,legZ-.092*sign);
        hit=palmLink(hit,eye,d,vec3f(.08,.48,legZ),mix(vec3f(.08,.48,legZ),knee,.48),.111,.109,shorts,shirt);
        hit=palmLink(hit,eye,d,mix(vec3f(.08,.48,legZ),knee,.38),knee,.079,.082,skin,shadedSkin);
        hit=palmLink(hit,eye,d,knee,ankle,.063,.068,skin,shadedSkin);
        hit=solidOval(hit,eye,d,ankle+vec3f(-.063,-.015,0),vec3f(.124,.047,.059),shadedSkin,skin);
        let shoulder=vec3f(.46,.83,sign*.15);
        let elbow=vec3f(.27,.62,sign*.308);
        let hand=vec3f(-.22,.715,sign*.309);
        hit=palmLink(hit,eye,d,shoulder,elbow,.066,.067,skin,shadedSkin);
        hit=palmLink(hit,eye,d,elbow,hand,.049,.049,skin,shadedSkin);
        hit=solidOval(hit,eye,d,hand,vec3f(.078,.032,.047),skin,shadedSkin);
    }
    let neck=vec3f(.51,.885,0);
    let bob=g.beachVisitor.y;
    let head=neck+vec3f(.075-bob*.37,.175-bob*.20,0);
    hit=palmLink(hit,eye,d,neck,head,.055,.052,skin,shadedSkin);
    let hair=vec3f(.204,.106,.060);
    hit=solidOval(hit,eye,d,head+vec3f(.053,-.028,0),vec3f(.124,.168,.141),hair,hair);
    hit=solidOval(hit,eye,d,head-vec3f(.036,.016,0),vec3f(.109,.134,.111),skin,shadedSkin);
    let hat=vec3f(.82,.79,.635);
    hit=solidOval(hit,eye,d,head+vec3f(.01,.115,0),vec3f(.172,.022,.158),hat,canvas);
    hit=solidOval(hit,eye,d,head+vec3f(.033,.147,0),vec3f(.113,.062,.115),hat,canvas);
    let lenses=vec3f(.052,.077,.091);
    hit=solidOval(hit,eye,d,head+vec3f(-.097,.022,.074),vec3f(.043,.031,.046),lenses,canvas);
    hit=solidOval(hit,eye,d,head+vec3f(-.097,.022,-.074),vec3f(.043,.031,.046),lenses,canvas);
    return hit;
}
fn shoreRocks(initial:PropHit,eye:vec3f,ray:vec3f,z:f32,edge:f32,random:f32,count:u32)->PropHit{
    let spacing=g.land.x;
    let cellDepth=min(spacing*.48,3.15);
    let outcrop=hash(floor(z/(spacing*3.0))+g.features.y+131.9);
    var hit=initial;
    for(var n=0u;n<count;n++){
        let seed=select(hash(random*19.7+f32(n)*9.11),random,n==0u);
        let radius=.12+.55*pow(seed,1.41)*g.beachDetail.x;
        var scale=vec3f(radius*(.7+.97*outcrop),radius*(.50+.72*seed),radius);
        var x=.94+3.5*hash(seed*33.1);
        var shift=cellDepth*(hash(seed*71.4)*1.67-.835);
        if(n==0u){
            x=.79;
            shift=0.0;
            scale=select(vec3f(1.03+random*.49,.50+outcrop*1.36,cellDepth*(.30+.28*outcrop)),
                vec3f(1.78,.51+outcrop*.36,cellDepth*.81),seed>=.34&&seed<.5);
        }
        if(n==1u){
            x=-.16;
            shift=cellDepth*.41;
            scale=vec3f(.67+seed*.35,.36+seed*.53,.43+outcrop*.69);
        }
        if(n>=7u){
            x=4.94+seed*1.38;
            shift=select(-.63,.68,n==8u)*cellDepth*.62;
            scale*=1.08+outcrop*.74;
        }
        let ground=seatedRock(vec2f(edge+x,z+shift),scale);
        hit=solidRock(hit,eye,ray,ground,scale,seed);
    }
    return hit;
}
fn sceneProps(v:View,dir:vec3f,limit:f32,start:vec3f)->PropHit{
    var hit=PropHit(min(limit,g.land.w),start);
    let spacing=g.land.x;
    let base=floor(v.eye.z/spacing)*spacing;
    let excite=g.audio.x*g.land.z;
    if(g.land.y<=0.0||g.features.x<.5){return beachChair(v,dir,hit);}
    let inv=1.0/select(vec3f(-1),vec3f(1),dir>=vec3f(0))/max(abs(dir),vec3f(.00001));
    let leaf=tint(g.output.x+.63);
    let neon=tint(g.output.x+.43);
    for(var index=0u;index<23u;index++){
        // Interleave near and far rows. This culls distant assets after finding
        // an opaque object and supports both directions across the coastline.
        let ahead=select(-1.0,1.0,dir.z>=0);
        let side=select(ahead,-ahead,index%2u==0u);
        let z=base+(f32((index+1u)/2u)*side+.45)*spacing;
        let cellDepth=min(spacing*.48,3.15);
        let slabA=(z-cellDepth-v.eye.z)*inv.z;
        let slabB=(z+cellDepth-v.eye.z)*inv.z;
        let enter=max(.25,min(slabA,slabB));
        let leave=min(hit.distance,max(slabA,slabB));
        if(enter>=leave){continue;}
        let edge=shore(z);
        let xa=v.eye.x+dir.x*enter-edge;let xb=v.eye.x+dir.x*leave-edge;
        let left=min(xa,xb);let right=max(xa,xb);
        if(right<-1.65||left>18.0){continue;}
        let random=hash(floor(z/spacing)+g.features.y);
        let variant=u32(hash(random*921.0)*4.0);
        let populated=random<=g.land.y;
        if(!populated&&!feature(3u)){continue;}
        let tide=sin(g.buildings.z+z*.12)*g.buildings.w;
        if(feature(3u)&&left<7.0&&right>-1.65){
            hit=shoreRocks(hit,v.eye,dir,z,edge,random,select(2u,9u,populated&&g.beachDetail.x>=.05));
        }
        if(!populated){continue;}
        if(feature(4u)&&left<4.9&&right>1.9){
            for(var blade=0u;blade<5u;blade++){
                if(blade>=3u&&g.beachDetail.x<.45){break;}
                let n=f32(blade);
                let x=2.40+n*.36+tide*.11;
                let seed=hash(n+random*89.0);
                let h=.20+.48*seed;
                let stem=vec3f(edge+x,duneSand(x,z+(n-2.0)*.32),z+(n-2.0)*.32);
                let tip=stem+vec3f((seed-.4)*.41+tide*.32,h,cos(n*3.81)*.18);
                hit=palmLink(hit,v.eye,dir,stem,tip,.035+.037*seed,.031,leaf*(.48+excite*.16),leaf);
                if(g.beachDetail.x>.3){
                    let bend=mix(stem,tip,.57);
                    hit=palmLink(hit,v.eye,dir,bend,bend+vec3f(.19+tide*.21,.15,-.16),.031,.023,leaf*.52,leaf);
                }
            }
        }
        if(feature(0u)&&left<5.25&&right>1.7){
            let x=2.93+random*.38;
            let bird=variant==1u;
            let turtle=variant==2u;
            let bodyHeight=select(select(.22,.29,turtle),.57,bird);
            let y=duneSand(x,z)+bodyHeight;
            let animalColor=mix(vec3f(.38,.205,.126),leaf,select(.06,.51,turtle));
            hit=solidOval(hit,v.eye,dir,vec3f(edge+x,y,z),
                          vec3f(select(.37,.23,bird),select(.15,.21,bird),select(.23,.35,turtle)),animalColor,neon);
            hit=solidOval(hit,v.eye,dir,vec3f(edge+x+.25,y+select(.06,.19,bird),z+.21),vec3f(.17,.13,.14),animalColor*.81,neon);
            hit=solidOval(hit,v.eye,dir,vec3f(edge+x-.33,y-.12,z-.25),vec3f(.17,.07,.18),animalColor*.73,neon);
            if(g.beachDetail.x>.28){
                let body=vec3f(edge+x,y,z);
                if(bird){
                    let beak=body+vec3f(.42,.19,.16);
                    hit=palmLink(hit,v.eye,dir,beak,beak+vec3f(.20,-.041,.027),.049,.035,vec3f(.72,.30,.075),neon);
                    for(var foot=0u;foot<2u;foot++){
                        let d=f32(foot)-.5;
                        let leg=body+vec3f(-.06,-.10,d*.18);
                        hit=palmLink(hit,v.eye,dir,leg,leg+vec3f(d*.18,-.42,.05),.023,.025,animalColor*.58,neon);
                    }
                }else{
                    for(var foot=0u;foot<4u;foot++){
                        let side=select(-1.0,1.0,foot%2u==0u);
                        let offset=vec3f(select(-.22,.22,foot<2u),-.10,side*.29);
                        let end=body+offset+vec3f(-.11,-.055,side*.17);
                        hit=palmLink(hit,v.eye,dir,body+offset*.45,end,.068,.045,animalColor*.64,neon);
                    }
                }
            }
        }
        if(feature(2u)&&left<8.8&&right>3.5){
            let x=5.85+random*.42;
            let h=2.67+random*1.62;
            let y=duneSand(x,z);
            let root=vec3f(edge+x,y,z);
            let bend=vec3f((random-.5)*.69+tide*.15,0,(.46-random)*.48);
            let crown=root+vec3f(0,h,0)+bend;
            let wood=mix(vec3f(.294,.162,.104),neon,.08);
            let before=hit.distance;
            var start=root-vec3f(0,.12,0);
            for(var section=0u;section<3u;section++){
                let f=f32(section+1u)/3.0;
                let next=root+vec3f(0,h*f,0)+bend*f*f;
                let trunkRadius=mix(.176,.101,f);
                hit=palmLink(hit,v.eye,dir,start,next,trunkRadius,trunkRadius,wood,neon);
                start=next;
            }
            if(hit.distance<before){
                let p=v.eye+dir*hit.distance-root;
                let yr=p.y/h;
                let grain=stoneNoise(vec2f(atan2(p.x,p.z)*4.5,p.y*13.0));
                let scars=fract(p.y*(8.5+2.1*random)+grain*.37);
                hit.color*=mix(1.0,.63+.26*smoothstep(.035,.25,scars)+grain*.33,g.beachDetail.x);
            }
            let canopy=mix(leaf,vec3f(.20,.51,.105),.76)*(.87+tide*.10);
            for(var frond=0u;frond<9u;frond++){
                let j=f32(frond);
                let seed=hash(j+random*31.0);
                let yaw=j*2.39996+random*6.28319+tide*.12;
                let reach=.94+seed*.89+f32(variant)*.075;
                let radial=vec3f(cos(yaw)*reach,0,sin(yaw)*reach);
                let droop=select(.38+seed*.83,-.22-seed*.29,frond>=7u)+tide*.17;
                let arch=.29+seed*.46;
                let leafColor=mix(canopy,vec3f(.38,.31,.094),select(.08,.52,seed<.24));
                hit=palmFrond(hit,v.eye,dir,crown,radial,arch,droop,leafColor);
            }
            hit=solidOval(hit,v.eye,dir,crown-vec3f(0,.11,0),vec3f(.249,.184,.221),wood,neon);
        }
        if(feature(1u)&&left<18.0&&right>6.8){
            let style=min(5u,u32(hash(random*413.38+5.7)*6.0));
            // Cottage, watchtower, bungalow, duplex, chalet and split-level.
            let sizes=array<vec4f,6>(vec4f(1.48,1.72,1.19,.71),vec4f(.94,3.62,1.02,.46),
                vec4f(2.04,1.10,1.37,.24),vec4f(1.78,2.47,1.32,.49),
                vec4f(1.48,1.54,1.55,1.26),vec4f(1.91,2.04,1.48,.77));
            let design=sizes[style];
            let vary=hash(random*78.71+7.2);
            let swing=g.buildings.y*select(1.0,-1.0,style%2u==0u);
            let baseX=9.73+vary*2.36;
            let x=baseX+swing;
            let scale=g.buildings.x;
            let height=design.y*scale;
            let width=design.x*scale;
            let depth=min(cellDepth*.90,design.z*scale*(.83+random*.41));
            let y=duneSand(baseX,z);
            let hue=random*1.73+g.output.x*.39+g.viewport.z*(.025+vary*.013)+.067*sin(g.viewport.z*.071+z*.057);
            let wallLight=tint(hue);
            let trim=mix(vec3f(.43,.38,.30),tint(hue+.13),.35);
            let lamps=mix(vec3f(.91,.59,.25),tint(hue+.48),.58);
            let wall=vec3f(.085,.103,.121)+wallLight*(.31+vary*.12);
            let before=hit.distance;
            hit=solidBox(hit,v.eye,dir,inv,vec3f(edge+x,y+height*.5,z),vec3f(width,height*.5,depth),wall,trim);
            if(hit.distance<before){
                let p=(v.eye+dir*hit.distance-vec3f(edge+x,y,z))/scale;
                let panel=max(abs(fract((p.x+p.z)*1.16)-.5)-.26,abs(fract(p.y*1.48)-.5)-.25);
                let walls=step(.18,p.y)*step(p.y,height/scale-.12);
                let boards=1.0-.24*g.beachDetail.x*mask(abs(fract(p.y*7.8)-.5)-.06,.035);
                let mullion=smoothstep(.025,.067,abs(fract((p.x+p.z)*1.16)-.5));
                hit.color=hit.color*boards+lamps*walls*mask(panel,.027)*mullion*(.47+excite*.31+.24*g.audio.y);
            }
            let roof=mix(vec3f(.134,.092,.085),tint(hue+.27),.53);
            if(style==0u||style>=4u){
                hit=gable(hit,v.eye,dir,vec3f(edge+x,y+height-.022,z),
                    vec3f(width+.24,design.w*scale,depth+.19),style==5u,roof,trim);
            }else{
                hit=solidBox(hit,v.eye,dir,inv,vec3f(edge+x,y+height+.079*scale,z),vec3f(width+.22,.093,depth+.18),roof,trim);
                if(style==1u){
                    let lantern=vec3f(edge+x-.07,y+height+.32*scale,z);
                    hit=solidBox(hit,v.eye,dir,inv,lantern,vec3f(width*.63,.24*scale,depth*.71),lamps*.48,trim);
                    hit=gable(hit,v.eye,dir,lantern+vec3f(0,.23*scale,0),vec3f(width*.79,.49*scale,depth*.82),false,roof,trim);
                }else{
                    let annex=vec3f(edge+x+.15,y+height+.26*scale,z-.34);
                    hit=solidBox(hit,v.eye,dir,inv,annex,vec3f(width*.44,.22*scale,depth*.58),wall*.85,roof);
                    hit=solidBox(hit,v.eye,dir,inv,annex+vec3f(0,.26*scale,0),vec3f(width*.52,.055,depth*.65),roof*.84,trim);
                }
            }
            if(g.beachDetail.x>.2){
                let porch=vec3f(edge+x-width-.25,y+.27*scale,z);
                hit=solidBox(hit,v.eye,dir,inv,porch,vec3f(.37,.075,depth*.82),roof*.84,trim);
                hit=solidBox(hit,v.eye,dir,inv,porch+vec3f(-.32,.5*scale,0),vec3f(.043,.052,depth*.81),trim*.7,roof);
                for(var i=0u;i<3u;i++){
                    let post=porch+vec3f(-.32,.25*scale,(f32(i)-1.0)*depth*.76);
                    hit=solidBox(hit,v.eye,dir,inv,post,vec3f(.047,.28*scale,.047),wall*.86,roof);
                }
            }
        }
    }
    hit=beachChair(v,dir,hit);
    if(hit.distance<min(limit,g.land.w)){
        hit.color=mix(start,hit.color,exp(-hit.distance/g.land.w*1.34));
    }
    return hit;
}

fn splash(v:View,uv:vec2f,visibleDepth:f32)->vec3f{
    var foam=0.0;
    var rainbow=vec3f(0);
    for(var burst=0u;burst<4u;burst++){
        let impact=g.impacts[burst];
        let age=impact.y;
        if(age<0.0||age>=1.0){continue;}
        let origin=vec3f(shore(impact.x)+.40,.82,impact.x);
        let start=project(v,origin);
        let flick=project(v,origin+vec3f(-g.spray.y*.83,g.spray.y*1.58,0));
        if(max(start.z,flick.z)<.45){continue;}
        let bounds=min(start.xy,flick.xy)-vec2f(7.0)*v.focal/max(.7,start.z);
        let edge=max(start.xy,flick.xy)+vec2f(7.0)*v.focal/max(.7,start.z);
        if(any(uv<bounds)||any(uv>edge)){continue;}
        let power=min(1.55,impact.z);
        let height=g.spray.y*(.69+.31*power);
        let envelope=(1.0-age)*(1.0-age)*smoothstep(0.0,.14,age);
        let plume=project(v,origin+vec3f(-height*.73*age,height*2.7*age*(1.0-age),0));
        let fog=exp(-max(0.0,start.z)*.014);
        if(plume.z>.45&&plume.z<visibleDepth+.28){
            let scale=v.focal/plume.z;
            let tuft=(uv-plume.xy)/(scale*vec2f(.74+1.9*age,1.12+1.35*age));
            let grain=.73+.23*sin(tuft.x*25.0+sin(tuft.y*32.0)*2.0+age*37.0)+.13*sin(tuft.y*39.0-tuft.x*3.0);
            let vapor=(1.0-smoothstep(.12,1.16,length(tuft)/max(.35,grain)))*envelope*fog*power;
            foam+=vapor*.89;
            if(impact.w>=1.0){
                let bow=length((tuft-vec2f(-.12,-.45))*vec2f(.75,1.0));
                let spectral=exp(-pow((bow-.85)*5.3,2.0))*smoothstep(-.4,.24,tuft.y);
                let hue=clamp((bow-.6)*1.9,0.0,.72);
                rainbow+=tint(hue)*spectral*vapor*2.7;
            }
        }
        for(var i=0u;i<128u;i++){
            if(i>=u32(g.spray.z)){break;}
            let seed=f32(i)+84.0*fract(impact.w);
            let r=hash(seed+2.7);let spread=hash(seed+43.3);let life=.60+.40*hash(seed+69.1);
            let secondary=i%5u<=1u;
            let t=(age-select(0.0,.12,secondary)-.095*hash(seed+93.1))/life;
            if(t<0.0||t>=1.0){continue;}
            let jet=height*(.45+1.23*r)*select(1.0,.52,secondary);
            let lift=jet*(3.82*t-4.19*t*t);
            let lateral=(spread-.5)*(1.2+t*6.2)*select(1.0,1.55,secondary);
            let undertow=sin(t*3.1416)*(.5-r)*1.5;
            let water=origin+vec3f(-(1.8+4.8*r)*t+undertow-.15,lift,lateral);
            let point=project(v,water);
            if(point.z<.55||point.z>visibleDepth+.2){continue;}
            let radius=clamp(v.focal*(.018+.067*hash(seed+12.2))/point.z,.0014,.085);
            let delta=(uv-point.xy)*vec2f(1.0,.47+.50*r);
            let blob=exp(-dot(delta,delta)/(radius*radius));
            foam+=blob*(1.0-t)*(.62+.45*r)*fog*power;
        }
    }
    return min(foam,3.2)*mix(vec3f(.78,.94,1.16),tint(g.output.x+.048),.17)+rainbow;
}

fn offshoreHeight(p:vec2f)->f32{
    let amount=smoothstep(g.ocean.y,g.ocean.y+24.0,shore(p.y)-p.x);
    let phase=(p.x+.16*p.y+g.viewport.z*g.ocean.w)*6.283185/g.ocean.z;
    let roll=.5+.5*sin(phase+.28*sin(p.y*.09));
    let crossRoll=.5+.5*sin(phase*.48-p.y*.15+g.viewport.z*.23);
    return g.ocean.x*amount*(pow(roll,3.7)*(.69+.31*crossRoll)-.19);
}
fn offshoreIntersection(v:View,ray:vec3f,limit:f32)->f32{
    let shoreward=1.65*g.features.z-g.ocean.y;
    if(g.ocean.x<=0.0||min(v.eye.x,v.eye.x+ray.x*limit)>shoreward){return limit;}
    let stepSize=min(4.0,.26*g.ocean.z);
    var low=max(1.0,select(1.0,(v.eye.x-shoreward)/max(.03,-ray.x),v.eye.x>shoreward));
    low=max(low,select(1.0,(v.eye.y-g.ocean.x)/max(.02,-ray.y),ray.y<0.0));
    for(var i=0u;i<56u;i++){
        let high=low+stepSize;
        if(high>=limit){break;}
        let p=v.eye+ray*high;
        if(p.y<offshoreHeight(p.xz)){
            var a=low;
            var b=high;
            for(var j=0u;j<5u;j++){
                let m=(a+b)*.5;
                let q=v.eye+ray*m;
                if(q.y>offshoreHeight(q.xz)){a=m;}else{b=m;}
            }
            return (a+b)*.5;
        }
        low=high;
    }
    return limit;
}
// The surface carries height, horizontal gradient and curvature in world units.
fn component(phase:f32,k:vec2f,amplitude:f32)->vec4f{
    let h=sin(phase)*amplitude;
    return vec4f(h,k*(cos(phase)*amplitude),-dot(k,k)*h);
}
fn baseWater(p:vec2f)->vec4f{
    let t=g.viewport.z;
    let pulse=waveSample(p.y)*(.29+g.audio.x*.38);
    let k1=vec2f(1.47,.34);
    let k2=vec2f(2.68,-1.03);
    let k3=vec2f(.63,2.95);
    var w=component(dot(p,k1)+t*2.1+pulse,k1,.073);
    w+=component(dot(p,k2)+t*2.65+.5*sin(p.y*.51+t*.64),k2,.039);
    w+=component(dot(p,k3)-t*2.84,k3,.025);
    w*=g.beachWater.z;
    let h=offshoreHeight(p);
    if(p.x<shore(p.y)-g.ocean.y+.3){
        let step=.25;
        w.y+=(offshoreHeight(p+vec2f(step,0))-h)/step;
        w.z+=(offshoreHeight(p+vec2f(0,step))-h)/step;
    }
    w.x+=h;
    return w;
}
fn wavePacket(p:vec2f,center:vec2f,age:f32,speed:f32,frequency:f32,power:f32,damping:f32)->vec4f{
    if(age<0.0||age>2.30){return vec4f(0);}
    let offset=p-center;
    let front=.07+speed*age;
    let width=6.0/frequency;
    if(any(abs(offset)>vec2f(front+width))){return vec4f(0);}
    let radius=length(offset);
    let phase=(radius-front)*frequency;
    if(abs(phase)>6.0){return vec4f(0);}
    let lifetime=smoothstep(0.0,.07,age)*exp(-age*damping);
    let amplitude=power*lifetime*exp(-.105*phase*phase)/(.75+.36*radius);
    let c=cos(phase);
    let s=sin(phase);
    let h=amplitude*c;
    let slope=amplitude*frequency*(-s-.21*phase*c);
    let curvature=amplitude*frequency*frequency*((.0441*phase*phase-1.21)*c+.42*phase*s);
    return vec4f(h,offset*slope/max(radius,.08),curvature+slope/max(radius,.18));
}
fn dolphinTrack(phase:f32)->vec2f{
    return g.beachDolphin.xy+vec2f(1.14,7.3*g.beachDolphin.z)*(phase-.5);
}
fn surfaceImpulses(p:vec2f,coastX:f32)->vec4f{
    if(g.beachWater.w<=0.0){return vec4f(0);}
    var w=vec4f(0);
    for(var i=0u;i<32u;i++){
        let drop=g.rain[i];
        let style=g.rainStyle[i];
        let age=drop.w-g.weather.x;
        if(style.z<.5||age<0.0||age>=g.weather.y){continue;}
        let fade=1.0-smoothstep(.65,1.0,age/g.weather.y);
        let size=clamp(style.x,.12,1.2);
        w+=wavePacket(p,drop.xz,age,.67+size*1.75,27.0/(.54+size),
            (.004+size*.046)*style.y*fade,2.7);
    }
    for(var i=0u;i<4u;i++){
        let impact=g.impacts[i];
        if(impact.y<0.0){continue;}
        let z=impact.x;
        let x=shore(z);
        let age=impact.y*g.spray.x;
        let power=min(1.9,impact.z);
        w+=wavePacket(p,vec2f(x-.35,z),age,3.4,10.5,.17*power,1.55);
        // The falling spray puts a second pulse farther seaward.
        let returnAge=age-.62*g.spray.x;
        w+=wavePacket(p,vec2f(x-3.45,z+.3*fract(impact.w)),returnAge,1.8,15.0,.091*power,1.85);
    }
    if(g.wildlife.x>=0.0){
        let age=g.wildlife.x*g.wildlife.w;
        w+=wavePacket(p,dolphinTrack(.08),age-.08*g.wildlife.w,2.5,11.0,.092,1.65);
        w+=wavePacket(p,dolphinTrack(.88),age-.88*g.wildlife.w,2.1,13.0,.11,1.8);
    }
    return w*g.beachWater.w*(1.0-smoothstep(-.45,.32,coastX));
}
fn bottomDepth(p:vec2f)->f32{
    let ocean=max(0.0,shore(p.y)-p.x);
    let shoals=stoneNoise(p*.19)*.49+sin(p.y*.27+p.x*.31)*.19;
    return max(.12,.18+ocean*.385+smoothstep(.1,4.0,ocean)*shoals);
}
fn beachGround(p:vec2f)->f32{
    let bank=p.x-shore(p.y);
    if(bank<0.0){return -bottomDepth(p);}
    return duneSand(bank,p.y);
}
fn seatedRock(p:vec2f,size:vec3f)->vec3f{
    let ground=beachGround(p);
    // The seaward side of a rock can overhang a steep part of the seabed.
    let downslope=beachGround(p-vec2f(size.x*.51,0));
    let embed=min(size.y*.52,max(0.0,ground-downslope)*.56);
    return vec3f(p.x,ground+size.y*.29-embed,p.y);
}
fn seabedIntersection(origin:vec3f,ray:vec3f,limit:f32)->f32{
    var lo=0.0;
    var distance=.028;
    let slope=max(.18,-ray.y+.59*abs(ray.x)+.45*abs(ray.z));
    for(var step=0u;step<29u;step++){
        if(distance>=limit){break;}
        let p=origin+ray*distance;
        let gap=p.y+bottomDepth(p.xz);
        if(gap<=0.0){
            var hi=distance;
            for(var iteration=0u;iteration<5u;iteration++){
                let mid=.5*(lo+hi);
                let q=origin+ray*mid;
                if(q.y+bottomDepth(q.xz)>0.0){lo=mid;}else{hi=mid;}
            }
            return hi;
        }
        lo=distance;
        distance+=clamp(gap/slope,.038,4.3);
    }
    return limit;
}
fn causticLight(p:vec2f,n:vec3f,depth:f32,curvature:f32)->f32{
    let t=g.viewport.z;
    let refraction=p+n.xz*(.26+.23*depth);
    let q=refraction+vec2f(
        .38*sin(refraction.y*1.34+t*.63)+.19*cos(refraction.x*1.94-t*1.17),
        .45*sin(refraction.x*.92-t*.78)+.15*cos(refraction.y*2.42+t*.88));
    let a=sin(q.x*3.55+sin(q.y*2.44+t*.83)*.81-t*.71);
    let b=sin(q.y*4.06+cos(q.x*1.82-t*.41)*.90+t*.46);
    let c=sin((q.x-q.y)*2.62+t*.76);
    let fold=abs(a*.78+b+c*.38);
    let light=(1.0-smoothstep(.018,.24,fold))*(.42+.43*abs(a-b));
    let focus=clamp(1.0+curvature*.068,.59,1.42);
    return light*focus*g.beachWater.y*exp(-max(0.0,depth)*.31);
}
struct TurtlePose { center:vec3f, heading:vec3f, size:f32, stroke:f32, depth:f32 };
fn turtlePose(index:u32)->TurtlePose{
    let k=f32(index);
    let dive=g.beachTurtles[index];
    let phase=dive.z*.39;
    // The path depends on the coastline clock, so moving the camera never
    // displaces the turtle it is following.
    let z=g.camera.z+12.0+sin(phase)*8.6+(k-1.0)*3.44;
    let x=shore(z)-8.8-k*2.69+cos(phase)*2.34;
    let p=vec2f(x,z);
    let depth=min(dive.x,max(0.0,bottomDepth(p)-.92));
    let crest=.14+(waveSample(z)*.19+sin(dive.z*.8)*.095)*exp(-depth);
    let heading=vec3f(-sin(phase)*.36,dive.y+.044,cos(phase)*.94);
    return TurtlePose(vec3f(x,baseWater(p).x+crest-depth,z),heading,.91+.17*k,dive.z*1.49,depth);
}
struct TurtleSurface { distance:f32, normal:vec3f, uv:vec2f, material:u32 };
fn rotateLobe(p:vec3f,angles:vec2f)->vec3f{
    let xz=rotate2(p.xz,angles.x);
    let yz=rotate2(vec2f(p.y,xz.y),angles.y);
    return vec3f(xz.x,yz);
}
fn turtleLobe(previous:TurtleSurface,eye:vec3f,ray:vec3f,center:vec3f,radii:vec3f,
              angles:vec2f,material:u32)->TurtleSurface{
    let ro=rotateLobe(eye-center,angles);
    let rd=rotateLobe(ray,angles);
    let t=ellipsoid(ro,rd,vec3f(0),radii,previous.distance);
    if(t>=previous.distance){return previous;}
    let p=(ro+rd*t)/radii;
    let normal=p/radii;
    let yz=rotate2(normal.yz,-angles.y);
    let xz=rotate2(vec2f(normal.x,yz.y),-angles.x);
    return TurtleSurface(t,normalize(vec3f(xz.x,yz.x,xz.y)),p.xz+vec2f(.19,-.28)*p.y,material);
}
fn scutes(uv:vec2f)->vec3f{
    let cell=floor(uv);
    let point=fract(uv);
    var first=9.0;
    var second=9.0;
    var stain=0.0;
    for(var j=-1;j<=1;j++){
        for(var i=-1;i<=1;i++){
            let offset=vec2f(f32(i),f32(j));
            let key=dot(cell+offset,vec2f(23.07,79.67));
            let jitter=vec2f(hash(key),hash(key+48.9));
            let d=offset+.24+.53*jitter-point;
            let distance=dot(d,d);
            if(distance<first){
                second=first;
                first=distance;
                stain=jitter.y;
            }else{
                second=min(second,distance);
            }
        }
    }
    return vec3f(smoothstep(.032,.115,second-first),stain,sqrt(first));
}
fn turtleModel(origin:vec3f,ray:vec3f,pose:TurtlePose,index:u32,limit:f32)->MarineHit{
    let radius=pose.size*1.56;
    if(ellipsoid(origin,ray,pose.center,vec3f(radius),limit)>=limit&&distance(origin,pose.center)>radius){
        return MarineHit(limit,vec3f(0));
    }
    let front=normalize(pose.heading);
    let side=normalize(cross(front,vec3f(0,1,0)));
    let up=cross(side,front);
    let offset=(origin-pose.center)/pose.size;
    let eye=vec3f(dot(offset,front),dot(offset,up),dot(offset,side));
    let direction=vec3f(dot(ray,front),dot(ray,up),dot(ray,side))/pose.size;
    var hit=TurtleSurface(limit,vec3f(0,1,0),vec2f(0),0u);
    let still=vec2f(0);
    hit=turtleLobe(hit,eye,direction,vec3f(-.084,.067,0),vec3f(.74,.345,.507),still,0u);
    hit=turtleLobe(hit,eye,direction,vec3f(-.094,-.120,0),vec3f(.745,.093,.514),still,1u);
    hit=turtleLobe(hit,eye,direction,vec3f(-.035,-.171,0),vec3f(.66,.130,.441),still,2u);
    hit=turtleLobe(hit,eye,direction,vec3f(.52,-.067,0),vec3f(.325,.121,.114),still,3u);
    hit=turtleLobe(hit,eye,direction,vec3f(.817,.016,0),vec3f(.194,.146,.142),still,4u);
    hit=turtleLobe(hit,eye,direction,vec3f(.956,-.009,0),vec3f(.111,.081,.104),still,4u);
    let flap=sin(pose.stroke);
    for(var n=0u;n<2u;n++){
        let s=select(-1.0,1.0,n==1u);
        let stroke=select(flap,sin(pose.stroke+.41),n==0u);
        let wing=vec3f(.13-.092*stroke,-.18+.19*stroke,s*(.64+.063*stroke));
        let bend=vec2f(s*(.61+.24*stroke),s*(.12+.58*stroke));
        hit=turtleLobe(hit,eye,direction,wing,vec3f(.19,.061,.425),bend,3u);
        hit=turtleLobe(hit,eye,direction,wing+vec3f(-.24,.062*stroke,s*.275),
            vec3f(.093,.037,.22),bend+vec2f(s*.21,0),3u);
        hit=turtleLobe(hit,eye,direction,vec3f(-.64,-.16-.065*stroke,s*.36),
            vec3f(.139,.053,.252),vec2f(s*.77,s*.14),3u);
        hit=turtleLobe(hit,eye,direction,vec3f(.849,.070,s*.117),vec3f(.073,.065,.046),still,3u);
        hit=turtleLobe(hit,eye,direction,vec3f(.868,.072,s*.149),vec3f(.038,.034,.023),still,5u);
    }
    hit=turtleLobe(hit,eye,direction,vec3f(-.775,-.153,0),vec3f(.182,.055,.062),still,3u);
    if(hit.distance>=limit){return MarineHit(limit,vec3f(0));}
    let shell=hit.material==0u;
    let skin=hit.material==3u||hit.material==4u;
    let marks=scutes(hit.uv*select(8.3,3.2,shell)+f32(index)*2.8);
    let shells=array<vec3f,3>(vec3f(.266,.177,.086),vec3f(.159,.225,.118),vec3f(.340,.179,.075));
    let grain=.84+.23*marks.y+.12*sin(marks.z*19.0+marks.y);
    var base=mix(vec3f(.196,.221,.084),vec3f(.073,.108,.051),marks.x)*(.78+grain*.42);
    if(shell){base=mix(vec3f(.46,.322,.17),shells[index]*grain,marks.x);}
    if(hit.material==1u){base=mix(vec3f(.48,.358,.186),vec3f(.272,.184,.069),marks.x);}
    if(hit.material==2u){base=mix(vec3f(.341,.243,.10),vec3f(.66,.536,.291),marks.x);}
    if(hit.material==4u){
        let seam=1.0-smoothstep(.01,.052,abs(hit.uv.y+.10*sin(hit.uv.x*4.7)+.18));
        base*=1.0-.48*seam*smoothstep(-.20,.70,hit.uv.x);
    }
    if(hit.material==5u){base=vec3f(.012,.025,.021);}
    let n=normalize(front*hit.normal.x+up*hit.normal.y+side*hit.normal.z);
    let sun=normalize(vec3f(-.39,.81,.44));
    let diffuse=.42+.66*max(0.0,dot(sun,n));
    let shine=pow(max(0.0,dot(normalize(sun-ray),n)),select(32.0,74.0,hit.material==5u));
    let edge=pow(1.0-abs(dot(ray,n)),3.0);
    let color=base*diffuse+vec3f(.68,.91,.85)*shine*select(.30,.13,skin)+vec3f(.064,.11,.097)*edge;
    return MarineHit(hit.distance,color);
}
fn turtleHit(initial:MarineHit,origin:vec3f,ray:vec3f)->MarineHit{
    if(g.wildlife.z<=0.0){return initial;}
    var result=initial;
    for(var i=0u;i<3u;i++){
        let hit=turtleModel(origin,ray,turtlePose(i),i,result.distance);
        if(hit.distance<result.distance){result=hit;}
    }
    return result;
}
fn reefStones(origin:vec3f,ray:vec3f,samplePoint:vec2f,initial:PropHit)->PropHit{
    if(!feature(3u)){return initial;}
    var hit=initial;
    // Render the same shoreline outcrops on both sides of the waterline.
    if(g.land.y>0.0&&abs(samplePoint.x-shore(samplePoint.y))<3.7){
        let spacing=g.land.x;
        let row=floor(samplePoint.y/spacing);
        for(var i=-1;i<=1;i++){
            let index=row+f32(i);
            let z=(index+.45)*spacing;
            hit=shoreRocks(hit,origin,ray,z,shore(z),hash(index+g.features.y),2u);
        }
    }
    if(g.beachDetail.x<=.02){return hit;}
    let cellSize=4.1;
    let cell=floor(samplePoint/cellSize);
    let tiles=array<vec2f,5>(vec2f(0,0),vec2f(-1,0),vec2f(1,0),vec2f(0,-1),vec2f(0,1));
    for(var i=0u;i<5u;i++){
        let grid=cell+tiles[i];
        let seed=hashGrid(grid);
        if(seed<.16){continue;}
        let p=(grid+vec2f(.24+.49*seed,.67-.30*seed))*cellSize;
        if(p.x>shore(p.y)-.55){continue;}
        let radius=.40+1.20*seed;
        let size=vec3f(radius*1.37,radius*.62,radius);
        hit=solidRock(hit,origin,ray,seatedRock(p,size),size,seed);
        if(seed>.56){
            let shifted=p+vec2f(.86,-1.17);
            let smaller=vec3f(.57,.39,.68)*radius;
            if(shifted.x<shore(shifted.y)){
                hit=solidRock(hit,origin,ray,seatedRock(shifted,smaller),smaller,fract(seed+.78));
            }
        }
    }
    return hit;
}
fn waterColor(ray:vec3f,p:vec3f,coastX:f32,surface:vec4f)->vec3f{
    let n=normalize(vec3f(-surface.y,1,-surface.z));
    let facing=clamp(dot(-ray,n),0.0,1.0);
    let fresnel=.025+.975*pow(1.0-facing,5.0);
    let reflected=sky(reflect(ray,n));
    let sapphire=mix(vec3f(.018,.18,.26),tint(g.output.x-.055)*.19,.24);
    var liquid=sapphire;
    if(g.beachWater.x>.002&&coastX>-46.0&&ray.y<-.025){
        let through=refract(ray,n,.7501875);
        let floorDist=seabedIntersection(p,through,29.0);
        let bed=p+through*floorDist;
        let bedColor=select(sapphire,sandMaterial(bed.xz,floorDist)*.92,floorDist<29.0);
        let rock=reefStones(p,through,bed.xz,PropHit(floorDist,bedColor));
        let turtle=turtleHit(MarineHit(rock.distance,rock.color),p,through);
        let point=p+through*turtle.distance;
        let caustics=causticLight(point.xz,n,p.y-point.y,surface.w);
        let floor=turtle.color*(.65+caustics*.71)+vec3f(.19,.60,.63)*caustics;
        let absorption=exp(-vec3f(.39,.139,.087)*turtle.distance*(1.65-g.beachWater.x*1.22));
        liquid=mix(sapphire,floor,absorption*g.beachWater.x);
    }
    let sun=normalize(vec3f(-.46,.72,.44));
    let shine=pow(max(0.0,dot(n,normalize(sun-ray))),49.0);
    let glitter=vec3f(.64,.96,.92)*shine*(.48+.52*facing);
    let folded=clamp(surface.w*.061,-.12,.19);
    let crest=offshoreHeight(p.xz);
    let whitecap=smoothstep(.42,.83,max(0.0,crest)/max(.01,g.ocean.x))*
        smoothstep(.42,.75,stoneNoise(p.xz*vec2f(3.7,1.71)+vec2f(g.viewport.z*.49,0)));
    return mix(liquid,reflected,fresnel*.86)+glitter*.66+vec3f(.37,.69,.70)*whitecap*.45+
        vec3f(.09,.23,.29)*folded;
}
fn dolphinOval(p:vec3f,radii:vec3f)->f32{
    let scaled=p/radii;
    let k=length(scaled);
    return k*(k-1.0)/max(length(scaled/radii),.001);
}
fn blendDolphin(a:f32,b:f32,radius:f32)->f32{
    let overlap=max(0.0,radius-abs(a-b));
    return min(a,b)-overlap*overlap*(.25/radius);
}
fn dolphinFin(p:vec3f,span:f32,chord:f32,sweep:f32,curve:f32,thickness:f32)->f32{
    let t=clamp(p.y/span,0.0,1.0);
    let center=-sweep*t*(.65+.35*t);
    let width=max(.018,chord*(1.0-pow(t,.83))*(1.0+.49*sin(t*3.14159)));
    let height=thickness*(1.0-.83*t);
    let section=(length(vec2f((p.x-center)/width,(p.z-curve*t*t)/height))-1.0)
        *mix(height,width,.42);
    return max(section,max(-p.y-.028,p.y-span));
}
fn dolphinShape(p:vec3f,stroke:f32)->f32{
    let taper=smoothstep(.06,1.44,-p.x);
    let body=p-vec3f(0,taper*(-.26+.16*stroke),0);
    let trunk=dolphinOval(body-vec3f(-.18,.003,0),vec3f(.92,.236,.216));
    let peduncle=dolphinOval(body-vec3f(-.89,0,0),vec3f(.48,.108,.115));
    let brow=dolphinOval(p-vec3f(.47,.034,0),vec3f(.33,.198,.183));
    let rostrum=dolphinOval(p-vec3f(.78,-.068,0),vec3f(.28,.060,.105));
    let jaw=dolphinOval(p-vec3f(.70,-.108,0),vec3f(.327,.049,.112));
    let torso=blendDolphin(blendDolphin(trunk,peduncle,.103),brow,.104);
    let skull=blendDolphin(torso,blendDolphin(rostrum,jaw,.025),.055);
    let dorsal=dolphinFin(p-vec3f(-.22,.158,0),.44,.24,.39,.039,.055);
    let shoulder=vec3f(p.x-.23,p.y+.108,abs(p.z)-.13);
    let flipper=dolphinFin(vec3f(shoulder.x,-.59*shoulder.y+.807*shoulder.z,
        .807*shoulder.y+.59*shoulder.z),.56,.159,.35,.074-.047*stroke,.057);
    let fluke=dolphinFin(vec3f(body.x+1.26,abs(body.z),body.y+.028),.50,.157,.218,.057,.048);
    return blendDolphin(blendDolphin(blendDolphin(skull,dorsal,.055),flipper,.047),fluke,.035);
}
fn dolphinModel(eye:vec3f,ray:vec3f,phase:f32,limit:f32)->MarineHit{
    let path=dolphinTrack(phase);
    let leap=sin(phase*3.14159);
    let center=vec3f(path.x,baseWater(path).x+leap*3.0-.61,path.y);
    let size=1.60;
    let radius=1.61*size;
    let offset=eye-center;
    let facing=dot(offset,ray);
    let bound=facing*facing-dot(offset,offset)+radius*radius;
    if(bound<=0.0){return MarineHit(limit,vec3f(0));}
    let reach=sqrt(bound);
    var distance=max(.025,-facing-reach);
    let stop=min(limit,-facing+reach);
    if(distance>=stop){return MarineHit(limit,vec3f(0));}
    let front=normalize(vec3f(.156,cos(phase*3.14159)*1.29,g.beachDolphin.z));
    let stroke=sin(phase*12.5664);
    let horizontal=normalize(cross(front,vec3f(0,1,0)));
    let upright=cross(horizontal,front);
    let roll=(.34+.16*stroke)*g.beachDolphin.z;
    let side=horizontal*cos(roll)+upright*sin(roll);
    let up=cross(side,front);
    let localEye=vec3f(dot(offset,front),dot(offset,up),dot(offset,side))/size;
    let localRay=vec3f(dot(ray,front),dot(ray,up),dot(ray,side))/size;
    var p=localEye+localRay*distance;
    var found=false;
    for(var step=0u;step<47u;step++){
        p=localEye+localRay*distance;
        let depth=dolphinShape(p,stroke);
        if(depth<=.0028){found=true;break;}
        distance+=max(.004,depth*size*.56);
        if(distance>=stop){break;}
    }
    if(!found){return MarineHit(limit,vec3f(0));}
    let e=vec2f(.0035,0);
    let gradient=vec3f(
        dolphinShape(p+e.xyy,stroke)-dolphinShape(p-e.xyy,stroke),
        dolphinShape(p+e.yxy,stroke)-dolphinShape(p-e.yxy,stroke),
        dolphinShape(p+e.yyx,stroke)-dolphinShape(p-e.yyx,stroke));
    let normal=normalize(front*gradient.x+up*gradient.y+side*gradient.z);
    let countershade=p.y+.040*sin(3.6*p.x)+.045*smoothstep(.56,1.02,-p.x);
    let flank=mix(vec3f(.052,.111,.172),vec3f(.195,.286,.346),1.0-smoothstep(.037,.22,countershade));
    let belly=1.0-smoothstep(-.178,-.052,countershade);
    let fin=smoothstep(.28,.53,abs(p.z))*(1.0-smoothstep(-.86,-.57,p.x));
    var skin=mix(mix(flank,vec3f(.56,.65,.64),belly*.88),vec3f(.080,.153,.201),fin*.81);
    let lip=-.096+.034*pow(clamp((p.x-.61)/.43,0.0,1.0),2.0);
    let mouth=smoothstep(.59,.72,p.x)*(1.0-smoothstep(.008,.020,abs(p.y-lip)));
    skin*=1.0-.69*mouth;
    let eyePosition=(p.xy-vec2f(.574,.045))/vec2f(.043,.030);
    let eyeMask=smoothstep(.09,.13,abs(p.z))*(1.0-smoothstep(.71,1.02,length(eyePosition)));
    let highlight=1.0-smoothstep(.11,.36,length(eyePosition-vec2f(.20,.28)));
    skin=mix(skin,vec3f(.008,.015,.020)+highlight*vec3f(.32,.40,.41),eyeMask);
    let sun=normalize(vec3f(-.43,.72,.53));
    let specular=pow(max(0.0,dot(normal,normalize(sun-ray))),58.0);
    let rim=pow(1.0-clamp(dot(-ray,normal),0.0,1.0),4.0);
    let light=.42+.65*max(0.0,dot(sun,normal))+.15*max(0.0,normal.y);
    let color=skin*light+vec3f(.78,.88,.89)*specular*.66+
        mix(vec3f(.054,.118,.145),tint(g.output.x)*.15,.38)*rim;
    return MarineHit(distance,color);
}
fn oceanWildlife(v:View,ray:vec3f,initial:PropHit)->PropHit{
    var result=MarineHit(initial.distance,initial.color);
    let z=v.eye.z+v.forward.z*17.0;
    let coast=shore(z);
    let facingOcean=smoothstep(.15,.9,-v.forward.x);
    let beachRock=mix(-7.5,-9.0,facingOcean);
    let dolphin=g.wildlife.x;
    if(dolphin>=0.0){
        let hit=dolphinModel(v.eye,ray,dolphin,result.distance);
        if(hit.distance<result.distance){result=hit;}
    }
    let fish=g.wildlife.y;
    if(fish>=0.0){
        for(var i=0u;i<5u;i++){
            let k=f32(i);
            let phase=clamp(fish*1.25-k*.06,0.0,1.0);
            let p=vec3f(coast+beachRock+k*.69+phase*2.1,
                sin(phase*3.14159)*(1.12+k*.11)-.24,z+(k-2.0)*1.28+phase*1.9);
            let newHit=marineAnimal(v.eye,ray,p,vec3f(.65,cos(phase*3.14159)*.78,.69),
                .36+.035*k,i,phase*21.0,result.distance);
            if(newHit.distance<result.distance){result=newHit;}
        }
    }
    result=turtleHit(result,v.eye,ray);
    if(result.distance<initial.distance){
        let mist=1.0-exp(-result.distance*.021);
        return PropHit(result.distance,mix(result.color*1.72,initial.color,mist*.64));
    }
    return initial;
}
fn dolphinSpray(v:View,uv:vec2f,limit:f32)->vec3f{
    let phase=g.wildlife.x;
    if(phase<0.0){return vec3f(0);}
    let contact=select(.08,.88,phase>=.88);
    let age=(phase-contact)*g.wildlife.w;
    if(age<0.0||age>1.03){return vec3f(0);}
    let spot=dolphinTrack(contact);
    let water=baseWater(spot).x;
    var glints=0.0;
    for(var i=0u;i<23u;i++){
        let seed=f32(i)*13.17+contact;
        let angle=hash(seed)*6.28318;
        let radius=.32+hash(seed+1.7)*1.48;
        let height=(1.1+hash(seed+3.4)*2.5)*age-3.7*age*age;
        if(height<0.0){continue;}
        let p=vec3f(spot.x+cos(angle)*radius*age,water+height,spot.y+sin(angle)*radius*age);
        let screen=project(v,p);
        if(screen.z<.25||screen.z>=limit){continue;}
        let size=(.014+.025*hash(seed+.3))*v.focal/screen.z;
        let delta=(uv-screen.xy)*vec2f(1,.73);
        glints+=(1.0-smoothstep(size*.18,size+.52/g.viewport.y,length(delta)))*(1.0-smoothstep(.38,1.02,age));
    }
    return vec3f(.52,.77,.83)*min(glints,.9);
}
fn rain(v:View,uv:vec2f,visibleDepth:f32)->vec3f{
    var color=vec3f(0);
    for(var index=0u;index<32u;index++){
        let drop=g.rain[index];
        if(drop.w<0.0){continue;}
        let style=g.rainStyle[index];
        let ocean=style.z>.5;
        var landing=drop.xyz;
        if(ocean){landing.y=baseWater(drop.xz).x;}
        let life=drop.w/g.weather.x;
        let opacity=style.y*exp(-length(drop.xyz-v.eye)*.012);
        if(life<1.0){
            let height=max(0.0,g.detail.w-landing.y)*(1.0-life*life);
            let a=project(v,landing+vec3f(0,height,0));
            let b=project(v,landing+vec3f(-.12,height+g.weather.z,.06));
            if(min(a.z,b.z)>.8&&a.z<visibleDepth+.13){
                let width=max(.0009,.013/a.z);
                let gap=curve(uv,a.xy,b.xy)/width;
                if(gap<6.0){color+=vec3f(.21,.63,.92)*exp(-gap)*opacity;}
            }
        }else{
            let age=clamp((drop.w-g.weather.x)/g.weather.y,0.0,1.0);
            if(age>.62){continue;}
            let arc=4.0*age*(.62-age);
            let width=style.x*(.05+age*.56);
            let jet=project(v,landing+vec3f(0,arc*style.x,0));
            if(jet.z>.8&&jet.z<visibleDepth+.19){
                let diff=(uv-jet.xy)*vec2f(.84,1.72);
                let radius=max(.0015,v.focal*width/jet.z);
                if(dot(diff,diff)<radius*radius*12.0){
                    let burst=exp(-dot(diff,diff)/(radius*radius));
                    color+=vec3f(.43,.78,1)*burst*opacity*(1.0-age/.62)*select(.52,.78,ocean);
                }
            }
        }
    }
    return color;
}
fn landIntersection(eye:vec3f,ray:vec3f,waterPlane:f32)->f32{
    let top=.73+g.beachDetail.y;
    var lo=max(.04,(eye.y-top)/-ray.y);
    if(max(eye.x,eye.x+ray.x*waterPlane)<-1.65*g.features.z){return waterPlane;}
    let stride=(waterPlane-lo)/12.0;
    if(stride<=0.0){return waterPlane;}
    for(var step=0u;step<=12u;step++){
        let hi=min(waterPlane,lo+stride);
        let p=eye+ray*hi;
        let x=p.x-shore(p.z);
        if(x>=0.0&&p.y<=duneSand(x,p.z)){
            var inside=hi;
            for(var j=0u;j<5u;j++){
                let mid=(inside+lo)*.50;
                let q=eye+ray*mid;
                let u=q.x-shore(q.z);
                if(u>=0.0&&q.y<=duneSand(u,q.z)){inside=mid;}else{lo=mid;}
            }
            return inside;
        }
        lo=hi;
    }
    return waterPlane;
}
fn aboveWater(v:View,uv:vec2f,direction:vec3f)->vec3f{
    let back=sky(direction);
    let far=g.land.w*1.4;
    var travel=far;
    var color=back;
    var surfacePoint=vec2f(0);
    var coastDistance=0.0;
    var plane=far;
    if(direction.y<-.001&&v.eye.y>0.0){
        plane=min(far,max(.04,-v.eye.y/direction.y));
        let step=landIntersection(v.eye,direction,plane);
        let p=v.eye+direction*step;
        let coast=p.x-shore(p.z);
        if(step<far&&coast>=0.0){
            travel=step;
            surfacePoint=p.xz;
            coastDistance=coast;
            let wet=smoothstep(.12,3.1,coast);
            let slope=vec2f(duneSand(coast+.21,p.z)-duneSand(coast-.21,p.z),
                             duneSand(coast,p.z+.21)-duneSand(coast,p.z-.21));
            let n=normalize(vec3f(-slope.x,.42,-slope.y));
            let sun=normalize(vec3f(-.62,.61,.48));
            let diffuse=.59+.53*max(0.0,dot(sun,n));
            let leeward=1.0-.32*smoothstep(.07,.40,-slope.x);
            color=sandMaterial(p.xz,0.0)*mix(.51,1.12,wet)*diffuse*leeward;
        }
    }
    var waterDepth=offshoreIntersection(v,direction,min(far,min(plane,travel)));
    if(waterDepth<far&&waterDepth<=travel){
        var p=v.eye+direction*waterDepth;
        var coast=p.x-shore(p.z);
        if(coast<.04){
            let surface=baseWater(p.xz)+surfaceImpulses(p.xz,coast);
            let gradient=direction.y-dot(surface.yz,direction.xz);
            let error=p.y-surface.x;
            // A bounded correction keeps steep, near-camera ripples from
            // sending a grazing ray to the other side of the coastline.
            let offset=clamp(error/max(.17,-gradient),-.80,.80);
            let corrected=max(.10,waterDepth+offset);
            let displaced=v.eye+direction*corrected;
            let bank=displaced.x-shore(displaced.z);
            if(bank<.04&&corrected<=travel){
                waterDepth=corrected;
                p=displaced;
                coast=bank;
            }
            surfacePoint=p.xz;
            coastDistance=coast;
            color=waterColor(direction,p,coast,surface);
            travel=waterDepth;
        }
    }
    if(travel<far){
        let mist=.87*(1.0-exp(-travel/g.land.w*1.72));
        color=mix(color+shorelineFoam(surfacePoint,coastDistance),back,mist);
    }
    let ridge=mountainRange(v,direction,travel,color);
    let scenery=oceanWildlife(v,direction,sceneProps(v,direction,ridge.distance,ridge.color));
    let visibleDepth=scenery.distance*dot(direction,v.forward);
    return scenery.color+rain(v,uv,visibleDepth)+splash(v,uv,visibleDepth)+dolphinSpray(v,uv,visibleDepth);
}
fn reefCoral(origin:vec3f,ray:vec3f,samplePoint:vec2f,initial:PropHit)->PropHit{
    if(!feature(3u)){return initial;}
    let size=3.4;
    let center=floor(samplePoint/size);
    let neighbors=array<vec2f,4>(vec2f(0,0),vec2f(1,0),vec2f(-1,0),vec2f(0,1));
    var result=initial;
    for(var i=0u;i<4u;i++){
        let cell=center+neighbors[i];
        let seed=hashGrid(cell+vec2f(37,11));
        if(seed<.38){continue;}
        let p=(cell+vec2f(.27+seed*.35,.73-seed*.47))*size;
        if(p.x>shore(p.y)-2.3){continue;}
        let floor=-bottomDepth(p);
        let color=mix(vec3f(.23,.18,.47),vec3f(.79,.32,.13),fract(seed*7.23));
        let root=vec3f(p.x,floor+.08,p.y);
        let height=.33+seed*.61;
        let bound=vec3f(height*.70,height*.87,height*.70);
        if(ellipsoid(origin,ray,root+vec3f(0,height*.38,0),bound,result.distance)>=result.distance){continue;}
        for(var n=0u;n<6u;n++){
            let j=f32(n);
            let angle=j*2.39996+seed*6.28;
            let strand=hash(seed*72.4+j);
            let reach=height*(.19+.28*strand);
            let a=root+vec3f(cos(angle)*reach*.16,height*.21,sin(angle)*reach*.16);
            let tip=root+vec3f(cos(angle)*reach,height*(.52+.57*strand),sin(angle)*reach);
            if(seed>.79){
                let fan=tip-vec3f(0,.11*height,0);
                result=solidOval(result,origin,ray,fan,vec3f(reach*.87,.043+reach*.19,reach*.91),color*(.81+.39*strand),color);
            }else{
                result=palmLink(result,origin,ray,a,tip,.062+.046*strand,.060,color*(.83+.32*strand),color);
            }
        }
    }
    return result;
}
fn belowWater(v:View,ray:vec3f,uv:vec2f)->vec3f{
    let waterHeight=baseWater(v.eye.xz).x;
    var ceiling=35.0;
    var background=vec3f(.031,.23,.32)+vec3f(.032,.12,.14)*max(0.0,ray.y);
    if(ray.y>.029){
        let guess=clamp((waterHeight-v.eye.y)/ray.y,.025,ceiling);
        let q=v.eye+ray*guess;
        let plane=baseWater(q.xz)+surfaceImpulses(q.xz,q.x-shore(q.z));
        ceiling=clamp((plane.x-v.eye.y)/ray.y,.025,ceiling);
        let surfacePoint=v.eye+ray*ceiling;
        let normal=normalize(vec3f(plane.y,-1,plane.z));
        let outRay=refract(ray,normal,1.333);
        let mirror=causticLight(surfacePoint.xz*.77,normal,0.0,plane.w);
        let reflected=vec3f(.045,.26,.32)+vec3f(.13,.45,.52)*mirror;
        background=reflected;
        // Above the critical angle, refraction returns a zero vector.
        // Only evaluate the sky for rays that can leave the water.
        if(dot(outRay,outRay)>.50){
            background=mix(reflected,sky(normalize(outRay))*vec3f(.37,.75,.77),.83);
        }
    }
    let floorDist=seabedIntersection(v.eye,ray,ceiling);
    let bed=v.eye+ray*floorDist;
    let sand=sandMaterial(bed.xz,1.0)*.84;
    let samplePoint=(v.eye+ray*min(floorDist,7.2)).xz;
    let reef=reefStones(v.eye,ray,samplePoint,PropHit(floorDist,sand));
    let coral=reefCoral(v.eye,ray,samplePoint,reef);
    let turtle=turtleHit(MarineHit(coral.distance,coral.color),v.eye,ray);
    var color=background;
    if(turtle.distance<ceiling){
        let point=v.eye+ray*turtle.distance;
        let local=baseWater(point.xz);
        let normal=normalize(vec3f(-local.y,1,-local.z));
        let light=causticLight(point.xz,normal,local.x-point.y,local.w);
        let shaft=.66+.33*smoothstep(-.31,.58,sin(point.x*.58+waterHeight*3.0+point.z*.31));
        let lit=turtle.color*(.81+light*.97)*shaft+vec3f(.24,.73,.66)*light*.88;
        let transmittance=exp(-vec3f(.188,.092,.064)*turtle.distance*(1.57-.74*g.beachWater.x));
        color=mix(vec3f(.021,.204,.288),lit,transmittance);
    }
    let motes=(uv+vec2f(-g.viewport.z*.008,g.viewport.z*.013))*vec2f(111,96);
    let grain=hashGrid(floor(motes));
    let speck=smoothstep(.968,.999,grain)*(1.0-smoothstep(.05,.22,length(fract(motes)-.5)));
    return color+vec3f(.12,.36,.36)*speck*.29;
}
@fragment fn fragment(@builtin(position) position:vec4f)->@location(0) vec4f{
    let output=g.output;
    let uv=(position.xy/g.viewport.xy*2.0-1.0)*vec2f(g.viewport.w,-1);
    let v=view();
    let direction=normalize(v.forward*v.focal+v.right*uv.x+v.up*uv.y);
    var immersion=0.0;
    if(g.view.w>0.0&&v.eye.y<.30&&v.eye.x<shore(v.eye.z)-.27){
        let water=baseWater(v.eye.xz+direction.xz*.26).x;
        immersion=1.0-smoothstep(-.14,.11,v.eye.y+direction.y*.16-water);
    }
    var color=vec3f(0);
    if(immersion<1.0){color=aboveWater(v,uv,direction);}
    if(immersion>0.0){color=mix(color,belowWater(v,direction,uv),immersion);}
    let vignette=1.0-.22*clamp(dot(uv,uv)*.27,0.0,1.0);
    let rgb=displayColor(max(vec3f(0),color)*vignette,output);
    return vec4f(modeBadge(rgb.rgb,position.xy,g.viewport.xy,g.view.z),1);
}
