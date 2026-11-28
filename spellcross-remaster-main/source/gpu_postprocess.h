#pragma once
// Windows D3D11 compute backend. No changes to the game simulation or wx HWND.
// The output is read back into the existing wx bitmap until direct presentation
// can be integrated with all native HUD and dialog overlays.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "advanced_rendering.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace spellcross_gpu
{
    // All coordinates are top-left origin; packed colors are R,G,B in low bytes.
    inline bool Supports(int mode) { return mode==0||mode==1||mode==2||mode==9||mode==10||mode==11||mode==13||mode==14||mode==17||mode==18||mode==19||mode==20; }
    struct Frame
    {
        int width=0,height=0,sw=0,sh=0,nw=0,nh=0,zoom=100,dpi=100;
        int smoothDpi=0,mode=0,dedither=0,sharpen=0,lighting=0,material=0,sprites=0,night=0;
        int worldX=0,worldY=0,strength=0,gradePercent=100;
        int materialV2=0,waterStrength=0,waterPhase=0,waterModeDirection=0;
        int atmosphere=0,atmosphereIntensity=0,atmospherePhase=0,weatherV2=0;
        int bloom=0,contrastCurve=0,vignette=0,grain=0;
    };
    static_assert(sizeof(Frame)==128);
    inline constexpr char Shader[] = R"HLSL(
cbuffer Params : register(b0) {
 int width,height,sw,sh;
 int nw,nh,zoom,dpi;
 int smoothDpi,mode,dedither,sharpen;
 int lighting,material,sprites,night;
 int worldX,worldY,strength,gradePercent;
 int materialV2,waterStrength,waterPhase,waterModeDirection;
 int atmosphere,atmosphereIntensity,atmospherePhase,weatherV2;
 int bloom,contrastCurve,vignette,grain;
};
Texture2D<uint> nativeIdx : register(t0);
Texture2D<uint> logicalIdx : register(t1);
Texture2D<uint> palette : register(t2);
Texture2D<uint> tags : register(t3);
Texture2D<int> hud : register(t4);
Texture2D<uint> previous : register(t5);
Texture2D<uint> atlas : register(t6);
RWTexture2D<uint> outputTex : register(u0);

int3 Unpack(uint c) { return int3(c&255,(c>>8)&255,(c>>16)&255); }
uint Pack(int3 c) { uint3 v=(uint3)clamp(c,0,255); return v.x|(v.y<<8)|(v.z<<16)|0xff000000; }
int3 Pal(uint i) { return Unpack(palette.Load(int3(i,0,0))); }
uint N(int2 p) { return nativeIdx.Load(int3(p,0)); }
uint S(int2 p) { return logicalIdx.Load(int3(p,0)); }
bool Protected(uint i) { return i==0||i>=228; }
bool Reserved(uint i) { return i==0||i>=240; }
bool Inside(int2 p) { return all(p>0)&&p.x<nw-1&&p.y<nh-1; }
int Luma(int3 c) { return (77*c.x+150*c.y+29*c.z+128)/256; }
int Dist(int3 a,int3 b) { int3 d=abs(a-b); return d.x+d.y+d.z; }
int3 Blend(int3 a,int3 b,int w) { return (a*(256-w)+b*w+128)/256; }
int3 P(int2 p) { return Unpack(previous.Load(int3(p,0))); }

[numthreads(16,16,1)]
void Prepare(uint3 id:SV_DispatchThreadID)
{
 int2 p=int2(id.xy);
 if(p.x>=nw||p.y>=nh) return;
 uint a=N(p); int3 c=Pal(a);
 if(dedither!=0 && Inside(p) && !Protected(a))
 {
  uint b=N(p+int2(-1,0));
  if(a!=b && !Protected(b) && N(p+int2(1,0))==b && N(p+int2(0,-1))==b && N(p+int2(0,1))==b)
  {
   int diagonal=0;
   [unroll] for(int y=-1;y<=1;y+=2) [unroll] for(int x=-1;x<=1;x+=2) diagonal+=(N(p+int2(x,y))==a)?1:0;
   int3 d=Pal(b); int distance=Dist(c,d);
   if(diagonal>=3&&distance>=12&&distance<=144&&abs(Luma(c)-Luma(d))<=48) c=(c+d+1)/2;
  }
 }
 outputTex[p]=Pack(c);
}
[numthreads(16,16,1)]
void Sharpen(uint3 id:SV_DispatchThreadID)
{
 int2 p=int2(id.xy);
 if(p.x>=nw||p.y>=nh) return;
 int3 c=P(p);
 if(Inside(p)&&!Protected(N(p))&&!Protected(N(p+int2(-1,0)))&&!Protected(N(p+int2(1,0)))&&
     !Protected(N(p+int2(0,-1)))&&!Protected(N(p+int2(0,1))))
 {
  int3 l=P(p+int2(-1,0)),r=P(p+int2(1,0)),u=P(p+int2(0,-1)),d=P(p+int2(0,1));
  int lc=Luma(c),ll=Luma(l),lr=Luma(r),lu=Luma(u),ld=Luma(d);
  int contrast=max(lc,max(max(ll,lr),max(lu,ld)))-min(lc,min(min(ll,lr),min(lu,ld)));
  if(contrast>=3&&contrast<=96)
  {
   int3 lo=min(c,min(min(l,r),min(u,d))),hi=max(c,max(max(l,r),max(u,d)));
   int3 allowance=int3(hi.x!=lo.x?4:0,hi.y!=lo.y?4:0,hi.z!=lo.z?4:0);
   c=clamp(c+(4*c-l-r-u-d)*sharpen*min(64,96-contrast)/25600,max(0,lo-allowance),min(255,hi+allowance));
  }
 }
 outputTex[p]=Pack(c);
}
uint Tag(int2 p)
{
 if(lighting==0&&material==0&&waterStrength==0&&atmosphere==0&&bloom==0&&contrastCurve==0&&vignette==0&&grain==0) return 0;
 uint t=tags.Load(int3(p,0))&0x1ffffu;
 return t!=0&&(t&255)==S(p)&&p.y<hud.Load(int3(p.x,0,0))?t:0;
}
int3 Base(int2 p,bool lit)
{
 int3 c=Pal(S(p));
 if(!lit||lighting==0) return c;
 uint t=Tag(p); if(t==0) return c;
 int shade=int((t>>8)&63)-32;
 int l=(77*c.x+150*c.y+29*c.z)/256;
 int grade=clamp(gradePercent,0,100);
 int3 chroma=l+(c-l)*(100+(night!=0?8:22)*grade/100)/100;
 int3 contrast=chroma+(l-112)*(night!=0?5:12)*grade/10000;
 int3 tint=night!=0?int3(0,0,0):int3(shade/3,0,-shade/3)*grade/100;
 c=clamp(contrast*(100+shade*(night!=0?1:2))/100+tint,0,255);
 return night!=0?int3(0,c.y,0):c;
}
void LogicalAxis(int v,int extent,out int a,out int b,out int w)
{
 int n=(2*v+1)*100-dpi,den=2*dpi;
 int fixedPos=n<=0?0:(n*256+den/2)/den;
 fixedPos=clamp(fixedPos,0,(extent-1)*256);
 a=fixedPos/256;b=min(a+1,extent-1);w=fixedPos%256;
}
int3 LogicalColor(int2 p,bool lit,out uint tag)
{
 int2 q=min(p*100/dpi,int2(sw-1,sh-1));
 tag=Tag(q);
 if(smoothDpi==0||dpi!=125) return Base(q,lit);
 int ax,bx,wx,ay,by,wy;
 LogicalAxis(p.x,sw,ax,bx,wx); LogicalAxis(p.y,sh,ay,by,wy);
 int2 a=int2(ax,ay),b=int2(bx,ay),c=int2(ax,by),d=int2(bx,by);
 tag=Tag(a);
 uint m=(tag>>14)&7,t1=Tag(b),t2=Tag(c),t3=Tag(d);
 if(t1==0||t2==0||t3==0||((t1>>14)&7)!=m||((t2>>14)&7)!=m||((t3>>14)&7)!=m) tag=0;
 return ((Base(a,lit)*(256-wx)+Base(b,lit)*wx)*(256-wy)+
         (Base(c,lit)*(256-wx)+Base(d,lit)*wx)*wy+32768)/65536;
}
float Cubic(float x,float b,float c)
{
 x=abs(x);
 if(x<1) return (((12-9*b-6*c)*x+(-18+12*b+6*c))*x*x+6-2*b)/6;
 if(x<2) return (((-b-6*c)*x+(6*b+30*c))*x*x+(-12*b-48*c)*x+8*b+24*c)/6;
 return 0;
}
int3 SampleFilter(float2 p,int2 nearest)
{
 int3 center=P(nearest);
 if(mode==0||mode==1) return center;
 int2 b=int2(floor(p)); float2 f=p-b;
 if(mode==9||mode==19)
 {
  if(zoom*dpi<=10000||!Inside(nearest)) return center;
  int2 direction=int2(p.x<nearest.x?-1:1,p.y<nearest.y?-1:1);
  int2 qa=nearest+int2(direction.x,0),qb=nearest+int2(0,direction.y);
  int2 qo=nearest-int2(direction.x,0),qr=nearest-int2(0,direction.y);
  uint a=N(qa),c=N(qb),oa=N(qo),ob=N(qr);
  if(mode==9)
    return a==c&&a!=oa&&c!=ob&&!Protected(a)?P(qa):center;
  if(Protected(a)||Protected(c)||Protected(oa)||Protected(ob)) return center;
  int3 ca=P(qa),cb=P(qb);
  if(Dist(ca,cb)>32||Dist(ca,center)<48||Dist(cb,center)<48||
     Dist(ca,P(qo))<40||Dist(cb,P(qr))<40) return center;
  int2 phase=clamp(int2((p-nearest+0.5)*256),0,255);
  int coverage=clamp((abs(phase.x-128)+abs(phase.y-128)-100)*3,0,192);
  return Blend(center,(ca+cb+1)/2,coverage);
 }
 if(mode==13||mode==14)
 {
  float B=mode==13?0.262014512:0.0,C=mode==13?0.368992744:0.75;
  float4 wx,wy;
  [unroll] for(int i=0;i<4;i++){wx[i]=Cubic(f.x-i+1,B,C);wy[i]=Cubic(f.y-i+1,B,C);}
  wx/=dot(wx,float4(1,1,1,1));wy/=dot(wy,float4(1,1,1,1));
  float3 sum=0; int3 lo=255,hi=0;
  [unroll] for(int y=0;y<4;y++) [unroll] for(int x=0;x<4;x++)
  {
   int2 q=clamp(b+int2(x-1,y-1),int2(0,0),int2(nw-1,nh-1));
   int3 c=Protected(N(q))?center:P(q);
   sum+=float3(c)*wx[x]*wy[y];
   if(x>=1&&x<=2&&y>=1&&y<=2){lo=min(lo,c);hi=max(hi,c);}
  }
  return clamp(int3(floor(sum+0.5)),lo,hi);
 }
 if(mode==2) f=saturate((f-0.5)*max(1.0,zoom*dpi/10000.0)+0.5);
 else if(mode==10) f=(1-cos(f*3.14159265358979))*0.5;
 else if(mode==11) f=f*f*f*(f*(6*f-15)+10);
 int2 limit=int2(nw-1,nh-1);
 int2 q0=b,q1=min(b+int2(1,0),limit),q2=min(b+int2(0,1),limit),q3=min(b+1,limit);
 int3 a=Protected(N(q0))?center:P(q0),c=Protected(N(q1))?center:P(q1),
      d=Protected(N(q2))?center:P(q2),e=Protected(N(q3))?center:P(q3);
 int2 uv=int2(floor(f*4096+0.5));
 int u=uv.x,v=uv.y,s=4096;
 if(mode==17)
 {
  int3 sum=0; int total=0;
  [unroll] for(int y=0;y<2;y++) [unroll] for(int x=0;x<2;x++)
  {
   int2 q=min(b+int2(x,y),limit);
   if(!Protected(N(q)))
   {
    int3 col=P(q);
    int w=(((x==0?s-u:u)*(y==0?s-v:v))>>12)*max(0,96-Dist(center,col));
    sum+=w*col;total+=w;
   }
  }
  return total!=0?(sum+total/2)/max(total,1):center;
 }
 if(mode==18)
 {
  if(Dist(a,e)<=Dist(c,d))
    return v<=u?(a*(s-u)+c*(u-v)+e*v+s/2)/s:(a*(s-v)+d*(v-u)+e*u+s/2)/s;
  return u+v<=s?(a*(s-u-v)+c*u+d*v+s/2)/s:(e*(u+v-s)+c*(s-v)+d*(s-u)+s/2)/s;
 }
 int3 result=int3(floor(lerp(lerp(float3(a),float3(c),f.x),lerp(float3(d),float3(e),f.x),f.y)+0.5));
 if(mode==20&&zoom*dpi>=20000)
 {
  int edge=abs(clamp(int((p.y-nearest.y+0.5)*256),0,255)-128);
  result=(result*(256-edge*edge/256)+128)/256;
 }
 return result;
}
int EdgeDistance(uint a,uint b)
{
 int3 x=Pal(a),y=Pal(b);
 if(Reserved(a)||Reserved(b)||max(x.x,max(x.y,x.z))<20||max(y.x,max(y.y,y.z))<20) return 255;
 return min(254,Dist(x,y));
}
int3 Sprite(int2 p,int2 fract)
{
 uint center=N(p); int3 c=Pal(center);
 if(!Inside(p)||Reserved(center)) return c;
 int dx=fract.x<128?-1:1,dy=fract.y<128?-1:1;
 int fx=abs(fract.x-128),fy=abs(fract.y-128);
 uint a=N(p+int2(dx,0)),b=N(p+int2(0,dy));
 if(a==center&&b==center) return c;
 int da=EdgeDistance(center,a),db=EdgeDistance(center,b);
 if(EdgeDistance(a,b)<=26&&da>=40&&da<255&&db>=40&&db<255&&
    EdgeDistance(a,N(p-int2(dx,0)))>35&&EdgeDistance(b,N(p-int2(0,dy)))>35&&EdgeDistance(a,N(p+int2(dx,dy)))<=48)
    return Blend(c,Blend(Pal(a),Pal(b),128),clamp((fx+fy-115)*3,0,224));
 if(da<=32) c=Blend(c,Pal(a),fx/2);
 if(db<=32) c=Blend(c,Pal(b),fy/2);
 return c;
}
int WaveTriangle(int phase) { return 64-abs((phase&127)-64); }
int3 WaterDetail(int3 color,int2 uv)
{
 if(waterStrength==0) return color;
 int a=WaveTriangle(uv.x/3+uv.y*2+waterPhase);
 int b=WaveTriangle(uv.x+uv.y/2-waterPhase*2);
 int crest=max(0,a+b-100);
 int gain=100+((a-32)+(b-32)/2+crest)*waterStrength/240;
 int3 glint=night!=0?int3(0,0,0):crest*waterStrength*int3(1,2,3)*min(color,96)/28800;
 int3 result=clamp(color*gain/100+glint,0,255);
 return night!=0?int3(color.x,result.y,color.z):result;
}
bool WaterPixel(int2 q)
{
 if(q.x<0||q.y<0||q.x>=sw||q.y>=sh) return false;
 uint t=Tag(q);
 return t!=0&&((t>>14)&7)==7;
}
int3 WaterDetailV2(int3 color,int2 uv,int2 p)
{
 if(waterStrength==0||all(color==0)) return color;
 int a=WaveTriangle(uv.x/2+uv.y+waterPhase);
 int b=WaveTriangle(uv.x-uv.y/3-waterPhase*2);
 int dx=WaveTriangle(uv.x/2+uv.y+waterPhase+4)-WaveTriangle(uv.x/2+uv.y+waterPhase-4);
 int dy=WaveTriangle(uv.x-uv.y/3-waterPhase*2+4)-WaveTriangle(uv.x-uv.y/3-waterPhase*2-4);
 int2 n=int2(dx*(64-a)/32+dy*(64-b)/64,dx*(64-a)/64-dy*(64-b)/32);
 int crest=(a+b)/2;
 int direction=waterModeDirection-1;
 int sx=(direction==0||direction==3)?1:-1,sy=direction<2?1:-1;
 int slope=clamp(n.x*sx+n.y*sy,-32,32);
 int gain=100+(slope*2+(crest-32)/2)*waterStrength/100;
 int spec=max(0,slope+crest/2-24);
 int2 l=min(p*100/dpi,int2(sw-1,sh-1));
 int2 q=l+int2(n.x>4?1:n.x<-4?-1:0,n.y>4?1:n.y<-4?-1:0);
 int3 refracted=color;
 if(WaterPixel(l)&&WaterPixel(q)&&WaterPixel(int2(q.x,l.y))&&WaterPixel(int2(l.x,q.y))) refracted=Base(q,true);
 int2 cell=abs((uv&63)-32);
 int radius=(waterPhase+((uv.x/64)^(uv.y/64))*13)&31;
 int dist=max(cell.x,cell.y)+min(cell.x,cell.y)/2;
 int ring=atmosphere==3&&abs(dist-radius)<=1?(32-radius)*atmosphereIntensity/100:0;
 int3 mixed=color+(refracted-color)*waterStrength/200;
 int3 highlight=(spec*spec*int3(2,3,4)/48+ring)*waterStrength/100;
 int3 result=clamp(mixed*gain/100+highlight*min(color,96)/96,0,255);
 return night!=0?int3(color.x,result.y,color.z):result;
}
int3 MaterialDetail(int3 color,uint tag,int2 p)
{
 if(tag==0) return color;
 int m=int((tag>>14)&7);
 int2 world=int2(worldX,worldY)*4+p*40000/(zoom*dpi);
 if(m==7) return waterModeDirection!=0?WaterDetailV2(color,world,p):WaterDetail(color,world);
 if(material==0||strength==0) return color;
 int2 uv=world&511;
 uint packed=atlas.Load(int3(uv.x,uv.y+m*512,0));
 int light=int(packed&255)-128,tint=int((packed>>8)&255)-128;
 if(materialV2!=0)
 {
  int2 broad=(int2(world.x/3+world.y/7+173,world.y/5-world.x/11+89))&511;
  uint second=atlas.Load(int3(broad.x,broad.y+m*512,0));
  int w=(m==4||m==6)?2:3;
  light=(light*(5-w)+(int(second&255)-128)*w)/5;
  tint=(tint*(5-w)+(int((second>>8)&255)-128)*w)/5;
 }
 int gain=100+light*strength/(night!=0?200:100);
 int v=tint*strength/100;
 int3 t=0;
 if(night==0)
 {
  if(m==2) t=int3(v/3,v,-v/3);
  else if(m==0||m==1||m==5) t=int3(v,v/2,-v/2);
  else if(m==3) t=int3(-v/2,0,v);
 }
 color=clamp(color*gain/100+t*min(color,96)/96,0,255);
 return night!=0?int3(0,color.y,0):color;
}
uint WeatherHash(uint x,uint y)
{
 uint h=x*0x9e3779b9u^y*0x85ebca6bu;
 h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;
}
int3 WeatherLayer(int3 color,int2 uv,int phase,int intensity)
{
 if(atmosphere<1||atmosphere>4||intensity==0) return color;
 int x=uv.x,y=uv.y,alpha=0;
 int3 tint=0;
 if(atmosphere<=2)
 {
  int a=WaveTriangle(x/13+y/21-phase/8);
  int b=WaveTriangle(x/31-y/11+phase/16);
  alpha=(8+(a+b)/4)*intensity/100;
  tint=atmosphere==1?int3(175,185,180):int3(188,151,98);
 }
 else
 {
  int cell=atmosphere==3?64:80;
  int vx=x+(atmosphere==3?y/4:y/8)-phase/2;
  int vy=y-phase*(atmosphere==3?5:1);
  int cx=vx/cell-(vx%cell<0?1:0),cy=vy/cell-(vy%cell<0?1:0);
  int lx=vx-cx*cell,ly=vy-cy*cell;
  uint h=WeatherHash((uint)cx,(uint)cy);
  int px=8+int((h&255u)%uint(cell-16)),py=8+int(((h>>8)&255u)%uint(cell-16));
  int dx=abs(lx-px),dy=abs(ly-py);
  if(atmosphere==3)
  {
   alpha=dx<=1&&dy<=7?(36-dy*3)*intensity/100:0;
   tint=int3(155,180,200);
  }
  else
  {
   alpha=dx+dy<=4?(65-(dx+dy)*10)*intensity/100:0;
   tint=int3(225,234,240);
  }
 }
 int3 result=(color*(100-alpha)+tint*alpha+50)/100;
 return night!=0?int3(color.x,result.y,color.z):result;
}
int3 WeatherDetail(int3 color,uint tag,int2 p,int2 logical)
{
 if(atmosphere==0||atmosphereIntensity==0) return color;
 int2 uv=int2(worldX,worldY)*4+p*40000/(zoom*dpi);
 if(weatherV2==0) return tag==0?color:WeatherLayer(color,uv,atmospherePhase,atmosphereIntensity);
 if(logical.y>=hud.Load(int3(logical.x,0,0))) return color;
 uint packed=tags.Load(int3(logical,0));
 if(Tag(logical)==0&&(packed&0x80000000u)==0) return color;
 color=WeatherLayer(color,uv,atmospherePhase,atmosphereIntensity*65/100);
 return WeatherLayer(color,uv/2+int2(137,-83),atmospherePhase*2/3+101,
    atmosphereIntensity*(atmosphere>=3?80:35)/100);
}
bool PostEligible(int2 q)
{
 if(any(q<0)||q.x>=sw||q.y>=sh) return false;
 if(q.y>=hud.Load(int3(q.x,0,0))) return false;
 return Tag(q)!=0||(tags.Load(int3(q,0))&0x80000000u)!=0;
}
int3 PostDetail(int3 color,int2 p,int2 logical)
{
 if(bloom==0&&contrastCurve==0&&vignette==0&&grain==0) return color;
 if(!PostEligible(logical)) return color;
 int3 glow=0;
 if(bloom!=0)
 {
  [unroll] for(int i=0;i<4;++i)
  {
   int2 q=logical+(i==0?int2(-2,0):i==1?int2(2,0):i==2?int2(0,-2):int2(0,2));
   if(PostEligible(q))
   {
    int3 bright=Base(q,true);
    int excess=max(0,Luma(bright)-160);
    glow+=bright*excess/95;
   }
  }
  glow/=4;
 }
 int nx=abs(2*p.x+1-width)*256/max(1,width);
 int ny=abs(2*p.y+1-height)*256/max(1,height);
 int edge=clamp((nx*nx+ny*ny-32768)/384,0,256);
 int noise=grain!=0?int(WeatherHash((uint)p.x,(uint)p.y)&31u)-15:0;
 int3 result=color;
 result+=(255-result)*glow*bloom/(255*400);
 result+=(result-128)*result*(255-result)*contrastCurve/(16384*200);
 result=result*(25600-edge*vignette/3)/25600;
 result+=noise*grain*min(result,255-result)/6400;
 result=clamp(result,0,255);
 return night!=0?int3(color.x,result.y,color.z):result;
}
[numthreads(16,16,1)]
void Render(uint3 id:SV_DispatchThreadID)
{
 int2 p=int2(id.xy);
 if(p.x>=width||p.y>=height) return;
 uint tag; int3 color=LogicalColor(p,true,tag);
 int2 l=min(p*100/dpi,int2(sw-1,sh-1));
 int top=sh;
 [unroll] for(int i=-2;i<=2;i++) top=min(top,hud.Load(int3(clamp(l.x+i,0,sw-1),0,0)));
 if(l.y+2<top && (sprites!=0||mode!=0||dedither!=0||sharpen!=0))
 {
  uint ignored; int3 raw=lighting!=0?LogicalColor(p,false,ignored):color;
  float2 pos=clamp((float2(p)+0.5)*10000.0/(zoom*dpi)-0.5,float2(0,0),float2(nw-1,nh-1));
  int2 nearPos=int2(floor(pos+0.5));
  if(sprites!=0)
  {
   int scale=zoom*dpi; int2 n=(2*p+1)*10000;
   int2 fixedPos=min((n/scale)*128+(n%scale)*128/scale,int2(nw*256-1,nh*256-1));
   int2 q=fixedPos/256;
   if(!Reserved(N(q))&&!Reserved(S(l))) color=clamp(Sprite(q,fixedPos%256)+color-raw,0,255);
  }
  if((mode!=0||dedither!=0||sharpen!=0)&&!Protected(S(l))&&!Protected(N(nearPos)))
  {
   int3 filtered=SampleFilter(pos,nearPos);
   color=mode!=0?clamp(filtered+color-raw,0,255):clamp(color+filtered-Pal(N(nearPos)),0,255);
  }
 }
 outputTex[p]=Pack(PostDetail(WeatherDetail(MaterialDetail(color,tag,p),tag,p,l),p,l));
}
)HLSL";
    using Microsoft::WRL::ComPtr;
    struct Texture
    {
        ComPtr<ID3D11Texture2D> tex;
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
        unsigned w=0,h=0;
        DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
        bool Ensure(ID3D11Device* device,unsigned width,unsigned height,DXGI_FORMAT fmt,unsigned bind,bool staging=false)
        {
            if(tex&&w==width&&h==height&&format==fmt) return true;
            *this=Texture{};
            D3D11_TEXTURE2D_DESC d{};
            d.Width=width;d.Height=height;d.MipLevels=1;d.ArraySize=1;d.Format=fmt;
            d.SampleDesc.Count=1;d.Usage=staging?D3D11_USAGE_STAGING:D3D11_USAGE_DEFAULT;
            d.BindFlags=bind;d.CPUAccessFlags=staging?D3D11_CPU_ACCESS_READ:0;
            if(FAILED(device->CreateTexture2D(&d,nullptr,tex.GetAddressOf()))) return false;
            if((bind&D3D11_BIND_SHADER_RESOURCE)&&FAILED(device->CreateShaderResourceView(tex.Get(),nullptr,srv.GetAddressOf()))) return false;
            if((bind&D3D11_BIND_UNORDERED_ACCESS)&&FAILED(device->CreateUnorderedAccessView(tex.Get(),nullptr,uav.GetAddressOf()))) return false;
            w=width;h=height;format=fmt;return true;
        }
    };
    class Backend
    {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ComPtr<ID3D11ComputeShader> prepare,sharpen,render;
        ComPtr<ID3D11Buffer> constants;
        Texture native,logical,palette,tag,hud,pre,post,atlas,out,readback;
        bool attempted=false,failed=false,atlasReady=false;
        std::string error;
        std::wstring adapter;
        bool Fail(const char* why) { error=why;failed=true;return false; }
        bool Compile(const char* entry,ComPtr<ID3D11ComputeShader>& result)
        {
            ComPtr<ID3DBlob> code,messages;
            if(FAILED(D3DCompile(Shader,sizeof(Shader)-1,nullptr,nullptr,nullptr,entry,"cs_5_0",
                D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.GetAddressOf(),messages.GetAddressOf())))
            {
                error=messages?std::string(static_cast<const char*>(messages->GetBufferPointer()),messages->GetBufferSize()):"Shader compilation failed";
                failed=true;return false;
            }
            return SUCCEEDED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,result.GetAddressOf()))||Fail("CreateComputeShader failed");
        }
        bool Init(bool warp)
        {
            if(attempted) return !failed;
            attempted=true;
            const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
            D3D_FEATURE_LEVEL actual{};
            if(FAILED(D3D11CreateDevice(nullptr,warp?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
                levels,1,D3D11_SDK_VERSION,device.GetAddressOf(),&actual,context.GetAddressOf()))) return Fail("No D3D11 hardware device");
            ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> a;DXGI_ADAPTER_DESC desc{};
            if(SUCCEEDED(device.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(a.GetAddressOf()))&&SUCCEEDED(a->GetDesc(&desc))) adapter=desc.Description;
            if(!Compile("Prepare",prepare)||!Compile("Sharpen",sharpen)||!Compile("Render",render)) return false;
            D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Frame);b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            if(FAILED(device->CreateBuffer(&b,nullptr,constants.GetAddressOf()))) return Fail("Constant buffer allocation failed");
            return true;
        }
        void Upload(Texture& t,const void* bytes,unsigned pitch)
        { context->UpdateSubresource(t.tex.Get(),0,nullptr,bytes,pitch,0); }
        void Dispatch(ID3D11ComputeShader* shader,Texture& target,ID3D11ShaderResourceView* input,unsigned w,unsigned h)
        {
            ID3D11ShaderResourceView* views[]={native.srv.Get(),logical.srv.Get(),palette.srv.Get(),tag.srv.Get(),hud.srv.Get(),input,atlas.srv.Get()};
            ID3D11Buffer* cb=constants.Get();ID3D11UnorderedAccessView* u=target.uav.Get();
            context->CSSetShader(shader,nullptr,0);context->CSSetConstantBuffers(0,1,&cb);
            context->CSSetShaderResources(0,7,views);context->CSSetUnorderedAccessViews(0,1,&u,nullptr);
            context->Dispatch((w+15)/16,(h+15)/16,1);
            ID3D11ShaderResourceView* empty[7]={};u=nullptr;
            context->CSSetUnorderedAccessViews(0,1,&u,nullptr);context->CSSetShaderResources(0,7,empty);
        }
    public:
        double readbackMs=0;
        const std::string& Error() const { return error; }
        const std::wstring& Adapter() const { return adapter; }
        template<class Consumer> bool Run(const Frame& f,const uint8_t* src,const uint8_t* screen,const uint8_t (*pal)[3],
                const uint32_t* tags,const int* hudTop,const Consumer& consume,bool warp=false)
        {
            if(!Supports(f.mode)||!src||!screen||!pal||!hudTop||f.width<=0||f.height<=0||f.sw<=0||f.sh<=0||f.nw<=0||f.nh<=0) return false;
            if(!Init(warp)) return false;
            constexpr unsigned srv=D3D11_BIND_SHADER_RESOURCE,both=srv|D3D11_BIND_UNORDERED_ACCESS;
            auto* d=device.Get();
            if(!native.Ensure(d,f.nw,f.nh,DXGI_FORMAT_R8_UINT,srv)||!logical.Ensure(d,f.sw,f.sh,DXGI_FORMAT_R8_UINT,srv)||
                !palette.Ensure(d,256,1,DXGI_FORMAT_R32_UINT,srv)||!hud.Ensure(d,f.sw,1,DXGI_FORMAT_R32_SINT,srv)||
                !out.Ensure(d,f.width,f.height,DXGI_FORMAT_R32_UINT,both)||
                !readback.Ensure(d,f.width,f.height,DXGI_FORMAT_R32_UINT,0,true)) return Fail("GPU surface allocation failed");
            const bool filter=f.mode!=0||f.dedither||f.sharpen;
            if(filter&&(!pre.Ensure(d,f.nw,f.nh,DXGI_FORMAT_R32_UINT,both)||
                (f.sharpen&&!post.Ensure(d,f.nw,f.nh,DXGI_FORMAT_R32_UINT,both)))) return Fail("GPU filter surface allocation failed");
            if(f.lighting||f.material||f.waterStrength||f.atmosphere||f.bloom||f.contrastCurve||f.vignette||f.grain)
            {
                if(!tags||!tag.Ensure(d,f.sw,f.sh,DXGI_FORMAT_R32_UINT,srv)) return Fail("GPU tag allocation failed");
                Upload(tag,tags,f.sw*4);
            }
            if(f.material&&!atlasReady)
            {
                using namespace spellcross_enhanced;
                if(!atlas.Ensure(d,textureSize,textureSize*static_cast<unsigned>(Material::Count),DXGI_FORMAT_R32_UINT,srv)) return Fail("GPU material atlas allocation failed");
                TextureBank bank;
                std::vector<uint32_t> packed;packed.reserve(textureSize*textureSize*static_cast<unsigned>(Material::Count));
                for(unsigned m=0;m<static_cast<unsigned>(Material::Count);++m)
                    for(auto v:bank.Get(static_cast<Material>(m)))
                        packed.push_back(uint32_t(int(v.light)+128)|(uint32_t(int(v.tint)+128)<<8));
                Upload(atlas,packed.data(),textureSize*4);atlasReady=true;
            }
            std::array<uint32_t,256> colors{};
            for(unsigned i=0;i<256;++i) colors[i]=uint32_t(pal[i][0])|(uint32_t(pal[i][1])<<8)|(uint32_t(pal[i][2])<<16);
            Upload(native,src,f.nw);Upload(logical,screen,f.sw);Upload(palette,colors.data(),1024);Upload(hud,hudTop,f.sw*4);
            context->UpdateSubresource(constants.Get(),0,nullptr,&f,0,0);
            ID3D11ShaderResourceView* input=nullptr;
            if(filter)
            {
                Dispatch(prepare.Get(),pre,nullptr,f.nw,f.nh);input=pre.srv.Get();
                if(f.sharpen){Dispatch(sharpen.Get(),post,input,f.nw,f.nh);input=post.srv.Get();}
            }
            Dispatch(render.Get(),out,input,f.width,f.height);
            const auto start=std::chrono::steady_clock::now();
            context->CopyResource(readback.tex.Get(),out.tex.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context->Map(readback.tex.Get(),0,D3D11_MAP_READ,0,&mapped))) return Fail("GPU readback failed");
            for(int y=0;y<f.height;++y)
                consume(y,reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch));
            context->Unmap(readback.tex.Get(),0);
            readbackMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(FAILED(device->GetDeviceRemovedReason())) return Fail("GPU device removed");
            return true;
        }
    };
}
#endif
