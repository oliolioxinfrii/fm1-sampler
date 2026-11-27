/* Native transcription of engine/ui/StudyGraphic.inc (0.40).
 * Wide 280x48 design coordinates: retain the compact symbol scale, add
 * repeated elements, longer domains and semantic detail rather than stretch.
 * Only normalized parameters and measured signal data arrive over USB.
 */
#define SG_PI 3.14159265358979323846f
typedef struct {float x,y;} SGPoint;
static uint16_t sg_b,sg_v,sg_a,sg_g,sg_d,sg_f,sg_w;
static float sg_scale,sg_ox,sg_oy;
static void sg_init(void){
    sg_scale=35.f/48;sg_ox=12+(216-280*sg_scale)*.5f;sg_oy=54;
    sg_b=sv_trace[0];sg_g=sv_trace[1];sg_v=sv_trace[2];sg_a=sv_trace[3];
    sg_d=sv_rgb(ss.light?0xa2b3c7:0x9f91bb);sg_w=sv_rgb(ss.light?0xecf4fa:0xe7f5ff);sg_f=sv_grid;
}
static uint16_t sg_mix(uint16_t x,uint16_t y,float v){
    v=sv_cl(v);unsigned r=(unsigned)(((x>>11)&31)*(1-v)+((y>>11)&31)*v+.5f),
      g=(unsigned)(((x>>5)&63)*(1-v)+((y>>5)&63)*v+.5f),b=(unsigned)((x&31)*(1-v)+(y&31)*v+.5f);
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static void sg_put(int x,int y,uint16_t c){
    // Canvas stores panel-order RGB565, not CPU-order.
    if(x>=12&&x<228&&y>=54&&y<89)cv_px[y*240+x]=swap16(c);
}
static float sg_x(float x){return sg_ox+x*sg_scale;}
static float sg_y(float y){return sg_oy+y*sg_scale;}
/* Raster convention matches SurfaceArt.h: pixel centres, round line ends,
 * minimum .53 px radius. Restrict all drawing to the existing graph well. */
static void sg_line(float x,float y,float u,float v,uint16_t c,float thick){
    x=sg_x(x);y=sg_y(y);u=sg_x(u);v=sg_y(v);
    float rad=fm_maxf(.53f,thick*sg_scale*.5f),dx=u-x,dy=v-y,ds=dx*dx+dy*dy;
    int l=clamp((int)fm_floorf(fm_minf(x,u)-rad),12,228),rr=clamp((int)(fm_maxf(x,u)+rad+1),12,228);
    int top=clamp((int)fm_floorf(fm_minf(y,v)-rad),54,89),bt=clamp((int)(fm_maxf(y,v)+rad+1),54,89);
    for(int j=top;j<bt;j++)for(int i=l;i<rr;i++){
        float q=ds>0?sv_cl(((i+.5f-x)*dx+(j+.5f-y)*dy)/ds):0;
        float rx=i+.5f-x-q*dx,ry=j+.5f-y-q*dy;if(rx*rx+ry*ry<=rad*rad)sg_put(i,j,c);
    }
}
static void sg_path(const SGPoint *p,unsigned n,uint16_t c,float w){for(unsigned i=1;i<n;i++)sg_line(p[i-1].x,p[i-1].y,p[i].x,p[i].y,c,w);}
#define SG_PATH(col,width,...) do {const SGPoint pts[]={__VA_ARGS__};sg_path(pts,sizeof pts/sizeof pts[0],col,width);}while(0)
static void sg_fill(float x,float y,float w,float h,uint16_t c){
    float l=sg_x(x),r=sg_x(x+w),t=sg_y(y),b=sg_y(y+h);
    int x0=clamp((int)fm_floorf(l),12,228),x1=clamp((int)(r+1),12,228),y0=clamp((int)fm_floorf(t),54,89),y1=clamp((int)(b+1),54,89);
    for(int j=y0;j<y1;j++)if(j+.5f>=t&&j+.5f<b)for(int i=x0;i<x1;i++)if(i+.5f>=l&&i+.5f<r)sg_put(i,j,c);
}
static void sg_rect(float x,float y,float w,float h,uint16_t c,uint16_t fill,int filled,float sw){
    if(w<=0||h<=0)return;if(filled)sg_fill(x,y,w,h,fill);
    SG_PATH(c,sw,{x,y},{x+w,y},{x+w,y+h},{x,y+h},{x,y});
}
static float sg_cross(float ax,float ay,float bx,float by,float x,float y){return (bx-ax)*(y-ay)-(by-ay)*(x-ax);}
static void sg_tri(float x,float y,float u,float v,float p,float q,uint16_t c){
    x=sg_x(x);y=sg_y(y);u=sg_x(u);v=sg_y(v);p=sg_x(p);q=sg_y(q);
    int l=clamp((int)fm_floorf(fm_minf(x,fm_minf(u,p))),12,228),r=clamp((int)(fm_maxf(x,fm_maxf(u,p))+1),12,228),
        t=clamp((int)fm_floorf(fm_minf(y,fm_minf(v,q))),54,89),b=clamp((int)(fm_maxf(y,fm_maxf(v,q))+1),54,89);
    for(int j=t;j<b;j++)for(int i=l;i<r;i++){float a=sg_cross(x,y,u,v,i+.5f,j+.5f),z=sg_cross(u,v,p,q,i+.5f,j+.5f),e=sg_cross(p,q,x,y,i+.5f,j+.5f);if((a>=0&&z>=0&&e>=0)||(a<=0&&z<=0&&e<=0))sg_put(i,j,c);}
}
static void sg_ring(float x,float y,float rr,uint16_t c,uint16_t fill,int filled,float sw){
    if(filled){float r=fm_maxf(.6f,rr*sg_scale),cx=sg_x(x),cy=sg_y(y);
        for(int j=(int)(cy-r-1);j<=cy+r+1;j++)for(int i=(int)(cx-r-1);i<=cx+r+1;i++){float a=i+.5f-cx,b=j+.5f-cy;if(a*a+b*b<=r*r)sg_put(i,j,fill);}}
    float px=x+rr,py=y;for(int i=1;i<=20;i++){float ph=2*SG_PI*i/20,nx=x+rr*fm_cosf(ph),ny=y+rr*fm_sinf(ph);sg_line(px,py,nx,ny,c,sw);px=nx;py=ny;}
}
static void sg_jack(float x,float y,uint16_t c){sg_ring(x,y,2.4f,c,sv_well,1,1.5f);}
static void sg_grid(float x,float y,float w,float h,int n,int m){
    for(int i=0;i<=n;i++)sg_line(x+w*i/n,y,x+w*i/n,y+h,sg_f,1);
    for(int j=0;j<=m;j++)sg_line(x,y+h*j/m,x+w,y+h*j/m,sg_f,1);
}
static void sg_arrow(float x,float y,float u,float v,uint16_t c){
    sg_line(x,y,u,v,c,1.6f);float dx=u-x,dy=v-y,mag=fm_sqrtf(dx*dx+dy*dy);if(mag<.001f)return;
    dx/=mag;dy/=mag;float a=4*.7960838f,b=4*.6051864f;
    SG_PATH(c,1.6f,{u-a*dx-b*dy,v-a*dy+b*dx},{u,v},{u-a*dx+b*dy,v-a*dy-b*dx});
}
static float sg_peak(unsigned i,unsigned n){
    if(!sv_has_data(SV_WAVE)||!sv_data_len||!n)return 0;
    unsigned j=i*sv_data_len/n;if(j>=sv_data_len)j=sv_data_len-1;return sv_data[j]/127.f;
}
static void sg_wave(float x,float y,float w,float h,unsigned start,unsigned count,unsigned total){
    float old=sg_peak(start,total);
    if(!sv_has_data(SV_WAVE)){sg_line(x,y,x+w,y,sg_f,1.6f);return;}
    for(unsigned i=1;i<count;i++){float v=sg_peak(start+i,total),a=x+w*(i-1)/(count-1),b=x+w*i/(count-1);
        sg_line(a,y-h*old,b,y-h*v,sg_b,1.3f);sg_line(a,y+h*old,b,y+h*v,sg_b,1.3f);old=v;}
}
static const float sg_random[49]={0.81931173f,0.10468243f,0.80010708f,0.62970217f,0.77850283f,0.56238123f,0.98194651f,0.91785499f,0.82230940f,0.94876340f,0.74967616f,0.64953146f,0.20571225f,0.93579155f,0.12065922f,0.83035126f,0.17331499f,0.06731168f,0.94521832f,0.19909202f,0.72817389f,0.10684148f,0.39348707f,0.85087357f,0.22768857f,0.53276616f,0.58355101f,0.48984472f,0.05408392f,0.05695586f,0.29085752f,0.63357332f,0.03987737f,0.36001081f,0.74483137f,0.03832910f,0.98840165f,0.05130799f,0.30671364f,0.88539232f,0.12956364f,0.56356142f,0.29113187f,0.55511821f,0.47407815f,0.72912808f,0.63900664f,0.45262511f,0.22001959f};
static void sg_echo(float a,float b,float c,float d){
    sg_wave(7,7,266,4,0,120,120);sg_line(7,13,273,13,sg_d,1.2f);
    for(int j=0;j<9;j++){
        int i=j;float q=i/8.f,xx=15+250*q+(sg_random[i+20]-.5f)*b*5,yy=28+(sg_random[i+7]-.5f)*b*13+(q-.5f)*c*11;
        float len=8+3*sg_random[i+7]+d*4*sg_random[i+40],hh=8+3*sg_random[i+2];
        float left=fm_clampf(xx-len*.5f,4,273-len),right=left+len,top=yy-hh*.5f,bot=yy+hh*.5f,z=2.3f;
        uint16_t edge=j==4?sg_v:sg_d,face=sg_mix(sv_well,sg_d,.32f),bevel=sg_mix(sv_well,sg_d,.58f);
        sg_line(xx,14,xx,top,sg_f,1);
        sg_tri(left,top,left+z,top-z,right+z,top-z,bevel);sg_tri(left,top,right+z,top-z,right,top,bevel);
        sg_tri(right,top,right+z,top-z,right+z,bot-z,face);sg_tri(right,top,right+z,bot-z,right,bot,face);
        SG_PATH(edge,1.1f,{left,top},{left+z,top-z},{right+z,top-z},{right+z,bot-z},{right,bot});
        sg_rect(left,top,len,hh,edge,face,1,1.2f);
        int count=2+(int)(a*2+.5f);for(int k=0;k<count;k++){
            float src=sg_peak(i*count+k,9*count),h=hh*(.22f+.52f*src),x=left+1.5f+(k+.5f)*(len-3)/count;
            sg_line(x,yy-h*.5f,x,yy+h*.5f,j==4?sg_v:sg_d,1.15f);}
        sg_line(left+1,bot-1,right-1,bot-1,j==4?sg_a:sg_f,1.1f);
    }
    sg_line(7,44,273,44,sg_d,1.2f);for(int i=0;i<=44;i++)sg_line(7+i*266.f/44,44,7+i*266.f/44,44-(i%4==0?2.5f:1.2f),sg_d,1);
    sg_line(7+266*ss.mark[2]/127.f,2,7+266*ss.mark[2]/127.f,12,sg_a,2);
}
static void sg_grain(float a,float b,float c,float d){
    sg_line(7,36,273,36,sg_d,1.6f);int count=9+(int)(b*15+.00001f),win=clamp((int)(c*5+.5f),0,5);
    float w=16+15*a;
    for(int j=0;j<count;j++){float x=8+(264-w)*j/(count-1),px=x,py=36;uint16_t col=j%3==0?sg_d:j%3==2?sg_v:sg_b;
        for(int i=0;i<=24;i++){float z=i/24.f,v=.5f*(1-fm_cosf(2*SG_PI*z));
            if(win==2)v=fm_minf(1,fm_minf(z/.15f,(1-z)/.15f));
            if(win==3||win==4){float u=win==4?1-z:z;v=u<.03f?fm_sinf(.5f*SG_PI*u/.03f):fm_expf(-5*(u-.03f)/.97f);if(u>.92f)v*=.5f+.5f*fm_cosf(SG_PI*(u-.92f)/.08f);}
            if(win==5){float u=(z-.5f)/.16f;v=sv_cl((fm_expf(-.5f*u*u)-.00752f)/(1-.00752f));}
            float xx=x+w*z,yy=36-26*v;if(i)sg_line(px,py,xx,yy,col,2);px=xx;py=yy;
        }
    }
    float bw=d>.5f?20:230,x=140-bw*.5f;SG_PATH(d>.5f?sg_a:sg_d,1.7f,{x,40},{x,45},{x+bw,45},{x+bw,40});
}
static void sg_motion(float a,float b,float c,float d){
    sg_rect(7,7,266,34,sg_d,0,0,1.5f);sg_line(7,24,273,24,sg_f,1);
    for(int i=0;i<=12;i++)sg_line(7+i*266.f/12,7,7+i*266.f/12,10,sg_d,1);
    float scan=2*a-1,travel=fm_fabsf(scan)*124,sign=scan<0?-1:1;
    for(int j=0;j<9;j++){int rev=j/8.f<b;float dir=(rev?-1:1)*sign,ex=fm_clampf(140+dir*travel+(j-4)*.85f,10,270),dy=(j-4)*c*3,ey=24+dy,x1=140+dir*fm_maxf(3,travel*.45f);
        uint16_t col=rev?sg_g:sg_b;float px=140,py=24;
        for(int k=1;k<=32;k++){float v=k/32.f,u=1-v,x=u*u*u*140+3*u*u*v*x1+3*u*v*v*(ex-dir*7)+v*v*v*ex,y=u*u*u*24+3*u*u*v*(24+dy*.1f)+3*u*v*v*ey+v*v*v*ey;
            sg_line(px,py,x,y,col,j==4?2.2f:1.2f);px=x;py=y;}
        if(fm_fabsf(ex-140)>5){float z=ex>=140?1:-1;SG_PATH(col,1.2f,{ex-z*3,ey-1.7f},{ex,ey},{ex-z*3,ey+1.7f});}}
    sg_rect(137.5f,9,5,30,sg_d,sv_well,1,1.5f);sg_line(140,11,140,37,sg_a,1.7f);
    float w=d>.5f?10:80,x=140-w*.5f;SG_PATH(d>.5f?sg_a:sg_d,1.5f,{x,46},{x,43},{x+w,43},{x+w,46});
}

static void sg_filter(float a,float b,float c,float d){
    // Response and drive transfer, as on the phone. Neither is a live FFT.
    sg_grid(8,7,197,34,9,2);float px=8,py=40;int mode=(int)(a*2+.5f);
    for(int i=0;i<=88;i++){float q=i/88.f,z=(q-b)*14,low=1/(1+fm_expf(z)),band=fm_expf(-z*z*.22f),v=mode==0?low:mode==1?band:1-low,x=8+197*q,y=40-32*sv_cl(.08f+.73f*v+.24f*c*band);
        if(i)sg_line(px,py,x,y,sg_a,2.2f);
        if(i%4==0)sg_line(x,y,x,40,sg_mix(sv_well,sg_d,.22f),1);
        px=x;py=y;}
    sg_line(8+197*b,6,8+197*b,41,sg_g,1.3f);
    sg_rect(225,7,44,34,sg_d,0,0,1.2f);
    sg_line(225,24,269,24,sg_f,1);sg_line(247,7,247,41,sg_f,1);sg_line(225,41,269,7,sg_f,1);
    float gain=1+6*d;px=225;py=24+15*fm_tanhf(gain)/fm_sqrtf(gain);
    for(int i=1;i<=40;i++){float q=i/40.f,x=225+44*q,y=24-15*fm_tanhf(gain*(2*q-1))/fm_sqrtf(gain);sg_line(px,py,x,y,sg_a,2);px=x;py=y;}
}

static void sg_delay(float a,float b,float c,float d){
    sg_line(9,13,271,13,sg_f,1);sg_line(9,31,271,31,sg_f,1);
    float step=19+33*a;int n=clamp(1+(int)(247/step),1,14);uint16_t fb=sg_mix(sg_d,sg_g,b);
    for(int i=0;i<n;i++){float x=12+i*step,y=d>.5f?(i%2?26:8):17;
        uint16_t color=sg_mix(sg_d,sg_v,i==0?.25f+.75f*c:c*fm_powf(.86f,i));
        sg_rect(x,y,11,10,color,0,0,1.8f);
        if(i<n-1)sg_arrow(x+11,y+5,x+step,d>.5f?(i%2?13:31):22,fb);}
    float end=12+(n-1)*step+5.5f,yy=d>.5f?((n-1)%2?36:18):27;
    SG_PATH(fb,1.6f,{end,yy},{end,43},{9,43},{9,d>.5f?13:22});sg_arrow(9,18,9,13,fb);
}

static void sg_reverb(float a,float b,float c,float d){
    // Extend the mesh by adding cells at its original pitch; no x scaling.
    const int nx=16,ny=4;
    for(int j=0;j<ny;j++){
        float y=10+9*j,px=7,py=y+4*b*fm_sinf(j*.5f);uint16_t col=sg_mix(sg_d,sg_b,.25f+.75f*a);
        for(int i=1;i<=112;i++){float u=i/112.f,x=7+266*u,yy=y+4*b*fm_sinf(12*SG_PI*u+j*.5f);
            sg_line(px,py,x,yy,col,1.6f);px=x;py=yy;}
    }
    for(int i=0;i<nx;i++){
        float x=14+252.f*i/(nx-1),px=x+4*b*fm_sinf(i*.4f),py=5;
        uint16_t col=i%2==0?sg_mix(sg_d,sg_v,c):i%4==1?sg_mix(sg_d,sg_a,d):sg_d;
        for(int j=1;j<=24;j++){float u=j/24.f,xx=x+4*b*fm_sinf(2*SG_PI*u+i*.4f),y=5+38*u;
            sg_line(px,py,xx,y,col,1.4f);px=xx;py=y;}
    }
    // Interleaved crossings restore the woven appearance of the phone mesh.
    for(int j=0;j<ny;j++)for(int i=0;i<nx;i++)if((i+j)%2==0){
        float x=14+252.f*i/(nx-1),u=(x-7)/266.f,y=10+9*j+4*b*fm_sinf(12*SG_PI*u+j*.5f);
        float xx=x+4*b*fm_sinf(2*SG_PI*sv_cl((y-5)/38)+i*.4f);
        float l=fm_maxf(7,xx-2.8f),r=fm_minf(273,xx+2.8f);
        float yl=10+9*j+4*b*fm_sinf(12*SG_PI*(l-7)/266+j*.5f),yr=10+9*j+4*b*fm_sinf(12*SG_PI*(r-7)/266+j*.5f);
        sg_line(l,yl,r,yr,sv_well,3.4f);sg_line(l,yl,r,yr,sg_mix(sg_d,sg_b,.25f+.75f*a),1.6f);
    }
}

static float sg_tape_y(float q,float a,float b){return 28-6*a*fm_sinf(2*SG_PI*q)+2*b*fm_sinf(10*SG_PI*q);}
static void sg_tape(float a,float b,float c,float d){
    float px=7,py=sg_tape_y(0,a,b);
    for(int i=1;i<=132;i++){float q=i/132.f,x=7+266*q,y=sg_tape_y(3*q,a,b);sg_tri(px,py,x,y,x,y+11,sg_f);sg_tri(px,py,x,y+11,px,py+11,sg_f);px=x;py=y;}
    px=7;py=sg_tape_y(0,a,b);
    for(int i=1;i<=132;i++){float q=i/132.f,x=7+266*q,y=sg_tape_y(3*q,a,b);sg_line(px,py,x,y,sg_a,2.4f);sg_line(px,py+11,x,y+11,sg_b,1.7f);px=x;py=y;}
    float head=sg_tape_y(3*133/266.f,a,b);sg_tri(130,6,150,6,147,head,sg_f);sg_tri(130,6,147,head,133,head,sg_f);
    SG_PATH(sg_d,2,{130,6},{150,6},{147,head},{133,head},{130,6});sg_line(133,head,147,head,sg_v,2.3f);
    for(int i=0;i<12+(int)(d*27);i++){float x=14+(i*37%250),y=sg_tape_y(3*(x-7)/266.f,a,b)+4;sg_line(x,y,x+3,y-1,sg_mix(sg_w,sg_v,c),1.4f);}
}

static SGPoint sg_insert_point(float q,float t,float width,float level,float mr,float sr){
    float tau=2*SG_PI,u=q*tau*5+2.1f*t,gain=.76f+.24f*sv_cl(1.6f*level*level),horizontal=gain*(.4f+.8f*width);
    float m=gain*(.72f*fm_sinf(u)+.20f*fm_sinf(2.02f*u+.6f*t)),s=horizontal*(.60f*fm_sinf(1.37f*u+.73f*t)+.15f*fm_sinf(2.63f*u-.5f*t));
    float dx=horizontal*(.60f*1.37f*fm_cosf(1.37f*u+.73f*t)+.15f*2.63f*fm_cosf(2.63f*u-.5f*t)),dy=gain*(.72f*fm_cosf(u)+.20f*2.02f*fm_cosf(2.02f*u+.6f*t));
    float norm=fm_maxf(.08f,fm_sqrtf(dx*dx+dy*dy)),energy=fm_sqrtf(mr*mr+sr*sr),amp=.26f*fm_tanhf(3.8f*fm_sqrtf(fm_maxf(0,energy-.0005f)));
    float frac=energy>0?sv_cl(sr*sr/(energy*energy)):.5f,taper=.35f+.65f*q*q*(3-2*q),ripple=amp*taper*(.78f*fm_sinf(tau*9*q-7.2f*t)+.22f*fm_sinf(tau*14*q-10.4f*t));
    return (SGPoint){fm_clampf(s-ripple*dy/norm*(.70f+.30f*frac),-.99f,.99f),fm_clampf(m+ripple*dx/norm*(1-.30f*frac),-.99f,.99f)};
}
static void sg_insert(float a,float b,float c,float d){
    // Preserve the small M/S scope; use the spare width to expose both
    // channels, insert mix levels, and output meters.
    sg_jack(9,16,sg_b);sg_jack(9,32,sg_b);
    for(int j=0;j<2;j++){float y=16+16*j;sg_line(12,y,27,y,sg_b,1.6f);
        sg_line(73,y,91,y,sg_b,1.6f);sg_line(137,y,163,y,sg_b,1.6f);}
    for(int i=0;i<2;i++){float x=27+64*i,v=i?d:c;uint16_t col=i?sg_a:sg_v;
        sg_rect(x,7,46,34,col,0,0,1.8f);
        sg_rect(x+5,13,36,8,sg_d,sv_well,1,1);
        sg_fill(x+6,14,34*v,6,col);
        if(i)SG_PATH(col,1.5f,{x+9,27},{x+22,27},{x+35,36});
        else sg_line(x+11,27,x+34,36,col,1.5f);
    }
    sg_rect(163,5,56,38,sg_d,0,0,1.5f);sg_rect(172,9,38,30,sg_d,0,0,1);sg_line(191,10,191,38,sg_f,1);sg_line(174,24,208,24,sg_f,1);
    float t=(plat_ms()%600000u)*.001f,mr=ss.mark[0]/127.f,sr=ss.mark[1]/127.f;SGPoint last=sg_insert_point(0,t,a,b,mr,sr);
    for(int i=1;i<=64;i++){float q=i/64.f;SGPoint next=sg_insert_point(q,t,a,b,mr,sr);float phase=.035f*t,v=.5f+.5f*fm_sinf(2*SG_PI*(q*6+phase)),u=.5f+.5f*fm_sinf(2*SG_PI*(q*6+phase+.34f));
        sg_line(191+13*last.x,24-13*last.y,191+13*next.x,24-13*next.y,sg_mix(sg_mix(sg_b,sg_v,c*fm_powf(v,1.35f)),sg_a,d*fm_powf(1-u,1.35f)),1.9f);last=next;}
    for(int i=0;i<2;i++){float y=16+16*i,level=i?sr:mr;sg_line(219,y,228,y,sg_b,1.6f);sg_rect(228,y-4,32,8,sg_d,sv_well,1,1);sg_fill(229,y-3,30*sv_cl(level),6,i?sg_v:sg_b);sg_line(260,y,269,y,sg_b,1.6f);sg_jack(272,y,sg_b);}
}

static void sg_env(float a,float b,float c,float d){
    sg_grid(8,7,264,34,12,2);float total=3*a*a+b+.65f+5*d*d,xa=8+264*3*a*a/total,xd=xa+264*b/total,xs=xd+264*.65f/total,yy=41-33*c;
    const SGPoint pts[]={{8,41},{xa,8},{xd,yy},{xs,yy},{272,41}};
    const uint16_t cols[]={sg_b,sg_g,sg_v,sg_a};
    for(int i=0;i<4;i++){
        SGPoint p=pts[i],q=pts[i+1];sg_tri(p.x,41,p.x,p.y,q.x,q.y,sg_mix(sv_well,cols[i],.15f));sg_tri(p.x,41,q.x,q.y,q.x,41,sg_mix(sv_well,cols[i],.15f));
        sg_line(p.x,p.y,q.x,q.y,sg_b,2.4f);
        if(i<3){sg_line(q.x,q.y,q.x,41,sg_mix(sg_f,cols[i],.5f),1);sg_ring(q.x,q.y,2.3f,sg_w,sv_well,1,1.5f);}
    }
}

static void sg_lfo(float a,float b,float c,float d){
    const float left=7,width=222,end=229;
    sg_grid(left,7,width,34,12,2);int shape=(int)(d*3+.5f),n=3+3*(int)(a*4),target=clamp((int)(c*3+.5f),0,3);
    if(shape==3){const float held[]={-.65f,.8f,-.25f,.48f,-.85f};float amp=5+8*b;
        for(int i=0;i<n;i++){float y=24-amp*held[i%5],x=left+width*i/n,e=left+width*(i+1)/n;if(i)sg_line(x,24-amp*held[(i-1)%5],x,y,sg_b,2);sg_line(x,y,e,y,sg_b,2);}}
    else if(shape==2){for(int i=0;i<n;i++){float x=left+width*i/n,xm=left+width*(i+.5f)/n,e=left+width*(i+1)/n;if(i)sg_line(x,24+13*b,x,24-13*b,sg_b,2);SG_PATH(sg_b,2,{x,24-13*b},{xm,24-13*b},{xm,24+13*b},{e,24+13*b});}}
    else{float px=left,py=24+(shape==1?13*b:0);for(int i=1;i<=144;i++){float z=i/144.f*n,ph=z-fm_floorf(z),v=shape==1?(ph<.5f?4*ph-1:3-4*ph):fm_sinf(2*SG_PI*z),x=left+width*i/144,y=24-13*b*v;sg_line(px,py,x,y,sg_b,2);px=x;py=y;}}
    float y=8+9.3f*target;SG_PATH(sg_b,1.6f,{end,24},{246,24},{246,y},{269,y});for(int i=0;i<4;i++)sg_jack(272,8+9.3f*i,i==target?sg_b:sg_d);
}

/* Schematic pitch sequence: three reference pitches per octave, using the
 * same index traversal as engine/dsp/Arp.h. No seven-event restart or
 * duplicate turning points. This is a pattern preview, not held-note history. */
static int sg_arp_note(int pattern,int n,int i){
    int k=i%n;
    switch(pattern){
    case 0:return k;
    case 1:return n-1-k;
    case 2:{int period=n>1?2*n-2:1;k=i%period;return k<n?k:period-k;}
    case 3:return k%2?n-1-k/2:k/2;
    default:{unsigned rng=99;for(int j=0;j<=i;j++)rng=rng*1664525u+1013904223u;return (int)((rng>>8)%(unsigned)n);}
    }
}
static int sg_arp_preview(int pattern,int levels,int count,int *pitch,int *color){
    // Accents identify the first and last displayed events, not pitch extrema.
    // Every interior note remains neutral regardless of pitch or pattern.
    for(int i=0;i<count;i++){
        pitch[i]=sg_arp_note(pattern,levels,i);
        color[i]=i==0?1:i==count-1?2:0;
    }
    return count;
}
static void sg_arp(float a,float b,float c,float d){
    int rate=clamp((int)(a*12+.5f),0,12),oct=clamp((int)(c*3+.5f),0,3)+1,p=clamp((int)(b*4+.5f),0,4);
    int count=rate?clamp(3+rate*2,1,24):0,levels=oct*3,pitch[24],colors[24];
    sg_grid(8,7,264,34,count?count:12,3);
    sg_arp_preview(p,levels,count,pitch,colors);
    float step=264.f/(count?count:1);
    for(int i=0;i<count;i++){
        float y=34-23.f*pitch[i]/(levels-1);
        uint16_t col=colors[i]==1?sg_b:colors[i]==2?sg_v:sg_d;
        sg_rect(9+i*step,y+1,fm_maxf(3,(step-3)*(.18f+.82f*d)),6,col,col,1,1);
    }
    if(count&&ss.mark[0]){int i=clamp((int)(count*ss.progress/(float)SV_NORM),0,count-1);float x=9+step*i;sg_line(x,43,x+fm_minf(8,step-2),43,sg_g,2.2f);}
}
static void sg_mixer(float b,float c,float d){
    int src=(int)(b*8+.5f)-1,on=src>=0&&src<8&&src!=ss.track;uint16_t col=on?sg_g:sg_d;
    for(int i=0;i<8;i++){float x=10+15*i;sg_jack(x,8,on&&i==src?sg_g:sg_d);sg_line(x,11,x,19,sg_f,1);}
    sg_line(10,19,115,19,sg_f,1);
    if(on)SG_PATH(sg_g,1.8f,{10+15*src,11},{10+15*src,19},{124,19},{124,24},{136,24});
    sg_rect(136,17,18,14,col,0,0,1.5f);SG_PATH(col,1.5f,{140,27},{145,21},{150,24});sg_line(154,24,164,24,col,1.6f);
    sg_rect(164,7,64,34,sg_d,0,0,1.5f);sg_grid(168,10,56,27,4,2);
    float q=(20+40*c)/60,kx=168+56*q,ky=37-27*q;SG_PATH(sg_v,2,{168,37},{kx,ky},{224,ky-27*(1-q)/(on?1+19*d*d:1)});
    sg_line(228,17,238,17,sg_b,1.6f);sg_line(228,31,238,31,sg_b,1.6f);
    for(int i=0;i<2;i++){float y=17+14*i;sg_rect(238,y-4,28,8,sg_d,0,0,1);sg_fill(239,y-3,26*ss.mark[2]/127.f,6,sg_g);sg_jack(272,y,sg_b);}
}

static void sg_key(void){
    // A fixed-size 12-key register travels along a seven-position octave rail.
    // GEN has five octaves (2..6); the normalized range still maps left/right.
    const int gen=ss.mode==SV_GEN_HUB,steps=gen?4:6;
    const int oct=clamp((int)(sv_n(2)*steps+.5f),0,steps);
    const float x0=7+182.f*oct/steps;
    float first=-1,last=-1;
    int root=gen?clamp((int)(sv_n(0)*11+.5f),0,11):clamp((int)(sv_n(1)*11+.5f),0,11);
    for(int i=0;i<12;i++){float x=x0+7*i;int v=sv_has_data(SV_WAVE)&&sv_data_len==12?sv_data[i]:0;
        int chord=gen?v==3:(v&2)!=0;
        uint16_t col=i==root?sg_a:(gen?v!=0:(v&1)!=0)?sg_b:sg_d;
        sg_rect(x,13,5,23,col,sg_a,i==root,1.4f);
        if(chord){if(first<0)first=x+2;last=x+2;sg_line(x+2,6,x+2,10,sg_v,1.6f);}}
    if(first>=0)sg_line(first,6,last,6,sg_v,1.6f);
    sg_line(7,43,273,43,sg_f,1.4f);
    for(int i=0;i<=steps;i++){float x=7+266.f*i/steps;sg_line(x,40,x,46,i==oct?sg_v:sg_d,i==oct?2.5f:1.2f);}
}

static void sg_save(float a){
    float level=ss.mark[2]/127.f,th=ss.mark[0]/127.f;
    sg_grid(7,7,229,34,12,2);sg_line(7,24,236,24,sg_b,1.6f);
    if(level>0)sg_rect(7,24-15*level,229,30*level,sg_b,sg_mix(sv_well,sg_g,.22f),1,1.2f);
    sg_line(7,24-15*th,236,24-15*th,sg_a,1.6f);sg_line(7,24+15*th,236,24+15*th,sg_a,1.6f);
    sg_rect(246,7,25,34,sg_d,0,0,1.5f);for(int i=0;i<9;i++){float y=37-i*3.1f;sg_fill(250,y,17,2,i<level*9?sg_g:sg_f);}
    uint16_t red=sv_rgb(ss.light?0xff5876:0xff5279);sg_ring(13,7,3,a>.5f?red:sg_v,red,a>.5f,1.5f);
}

static void sg_length(void){
    sg_rect(8,12,130,24,sg_d,0,0,1.5f);sg_rect(142,12,130,24,sg_b,0,0,1.5f);
    sg_wave(10,24,126,8,0,20,40);sg_wave(144,24,126,8,20,20,40);
    for(int i=1;i<8;i++){sg_line(8+i*130.f/8,12,8+i*130.f/8,15,sg_d,1);sg_line(142+i*130.f/8,12,142+i*130.f/8,15,sg_d,1);}
    SG_PATH(sg_w,2,{8,8},{8,5},{272,5},{272,8});sg_line(132,41,148,41,sg_g,1.6f);sg_line(140,38,140,44,sg_g,1.6f);
}

static void sg_tempo(float b){
    sg_line(8,9,272,9,sg_d,2);sg_line(8,5,8,14,sg_d,1.6f);sg_line(272,5,272,14,sg_d,1.6f);
    int bars=1+(int)(sv_n(0)*31+.5f),ticks=bars>16?32:bars*4;
    for(int i=0;i<=ticks;i++)sg_line(8+i*264.f/ticks,9,8+i*264.f/ticks,i%4==0?16:12,i%4==0?sg_b:sg_d,1.4f);
    sg_rect(8,20,264,8,sg_d,0,0,1.5f);float w=116+148*(1-b);
    sg_rect(8,33,w,8,sg_b,0,0,1.7f);for(int i=1;i<bars;i++)sg_line(8+w*i/bars,33,8+w*i/bars,41,sg_f,1);
    SG_PATH(sg_v,2,{8+w,30},{fm_minf(276,15+w),37},{8+w,44});
}

/* P_FX was the full-width fallback in surfaceGraphic(), not a 96x48
 * compact symbol. Reproduce its original 432x70 working coordinates. */
static void sg_speaker(float x,float y,float size,uint16_t ink){
    sg_fill(x,y-size*.7f,size*.7f,size*1.4f,sg_d);sg_fill(x+2,y-size*.7f+2,size*.7f-4,size*1.4f-4,sg_f);
    sg_ring(x+size*.35f,y-size*.35f,size*.13f,sg_w,sg_w,1,1);
    sg_ring(x+size*.35f,y+size*.25f,size*.20f,ink,ink,1,1);
    sg_ring(x+size*.35f,y+size*.25f,size*.08f,sv_well,sv_well,1,1);
}
static void sg_room(float x,float width,float size){
    const float left=x+4,right=x+width-4,cy=57;
    sg_rect(left,27,right-left,60,sg_d,0,0,3);
    sg_speaker(left+7,cy,24,sg_v);
    float sx=left+31,extent=(right-sx)*(.23f+.63f*size);
    for(int i=0;i<4;i++){float xx=sx+extent*(i+.5f)/4,rr=8+17*(i+1)/4.f;uint16_t col=i<2?sv_rgb(0xffe16a):sg_v;
        SG_PATH(col,3,{xx+rr*.50f,cy-rr*.86f},{xx+rr*.87f,cy-rr*.5f},{xx+rr,cy},{xx+rr*.87f,cy+rr*.5f},{xx+rr*.50f,cy+rr*.86f});}
}
static void sg_fx(float a,float b,float c,float d){
    sg_scale=.5f;sg_ox=12;sg_oy=43;
    sg_room(0,208,b);
    float x=220,width=212,cy=57;
    sg_speaker(x+5,cy,25,sg_v);
    float origin=x+36,span=fm_maxf(12,width-47),spacing=fm_maxf(12,span*(.06f+.22f*d));
    for(int i=0;i<14;i++){float xx=origin+(i+.5f)*spacing;if(xx+8>x+width)break;
        float radius=fm_maxf(3,fm_minf(12,spacing*.39f)*(.3f+.7f*fm_powf(.78f,i*.5f)));
        uint16_t col=c>.001f?sg_v:sg_d;
        sg_ring(xx,cy,radius,col,col,1,1);float inner=fm_maxf(1,radius-3);sg_ring(xx,cy,inner,sv_well,sv_well,1,1);
        if(radius>=7)sg_line(xx,cy-radius-3,xx,cy-radius-7,sv_rgb(0xffe16a),2);
    }(void)a;
}
static int sg_legacy(void){
    if(ss.mode==SV_FIELD||ss.mode==SV_GENERATOR)return 0;
    if(ss.mode==SV_GEN_HUB&&(ss.page==7||ss.page==4||ss.page==8)){
        if(ss.page==8){sg_init();sg_ring(140,24,12,ss.transport==2?sv_rgb(ss.light?0xff5876:0xff5279):sg_d,sv_well,0,3);return 1;}
        sg_init();sg_key();return 1;
    }
    sg_init();float a=sv_n(0),b=sv_n(1),c=sv_n(2),d=sv_n(3);
    switch(ss.page){
    case 1:sg_fx(a,b,c,d);break;case 2:sg_env(a,b,c,d);break;case 3:sg_lfo(a,b,c,d);break;
    case 4:sg_arp(a,b,c,d);break;case 5:sg_line(7,24,273,24,sg_d,2);for(int i=0;i<=24;i++)sg_line(7+i*266.f/24,18,7+i*266.f/24,30,sg_f,1.4f);sg_ring(7+266*a,24,4,sg_a,sg_a,1,1);break;
    case 6:sg_mixer(b,c,d);break;case 7:sg_key();break;case 8:sg_save(a);break;case 9:sg_length();break;case 10:sg_tempo(b);break;
    case 11:sg_echo(a,b,c,d);break;case 12:sg_filter(a,b,c,d);break;case 13:sg_delay(a,b,c,d);break;case 14:sg_reverb(a,b,c,d);break;
    case 15:sg_tape(a,b,c,d);break;case 16:sg_insert(a,b,c,d);break;case 17:sg_grain(a,b,c,d);break;case 18:sg_motion(a,b,c,d);break;
    default:return 0;
    }return 1;
}
#undef SG_PATH
