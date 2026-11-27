/* Procedural graphics, using the existing semantic palette and hub geometry. */
static uint16_t sv_rgb(uint32_t c){return RGB((c>>16)&255,(c>>8)&255,c&255);}
static uint16_t sv_bg,sv_panel,sv_well,sv_raised,sv_ink,sv_dim,sv_grid,sv_track,sv_red,sv_ac[4],sv_trace[4];
static void sv_colors(void){
    static const uint32_t light[]={0xc6c9cb,0xd9dce0,0x17202a,0xe9e9e5,0x161b29,0x4e5868,0x364455,0xaab4be,0xc42248,0x164bc9,0x007d58,0x8522ba,0x925900,0x51b8ff,0x39efac,0xc68aff,0xffc361};
    static const uint32_t dark[]={0x100e1a,0x201c32,0x090815,0x302845,0xf2f2ff,0xb3a9cb,0x322747,0x48405f,0xff5279,0x49d9ff,0x53ffab,0xbd8aff,0xffffc457,0x49d9ff,0x53ffab,0xbd8aff,0xffffc457};
    const uint32_t *p=ss.light?light:dark;
    sv_bg=sv_rgb(p[0]);sv_panel=sv_rgb(p[1]);sv_well=sv_rgb(p[2]);sv_raised=sv_rgb(p[3]);sv_ink=sv_rgb(p[4]);sv_dim=sv_rgb(p[5]);sv_grid=sv_rgb(p[6]);sv_track=sv_rgb(p[7]);sv_red=sv_rgb(p[8]);
    for(unsigned i=0;i<4;i++){sv_ac[i]=sv_rgb(p[9+i]);sv_trace[i]=sv_rgb(p[13+i]);}
}
static void sv_text(int x,int y,int width,const felucca_font_t *font,const char *s,uint16_t c){
    char b[64];unsigned n=0;while(s[n]&&n<sizeof b-1){b[n]=s[n];n++;}b[n]=0;
    while(n&&text_w(font,b)>width)b[--n]=0;
    cv_text(x,y,font,b,c);
}
static float sv_log10(float x){return fm_log2f(x)*.30102999566f;}
static float sv_mod(float a,float b){return a-b*fm_floorf(a/b);}
static float sv_n(unsigned i){return sv_local_norm(i)/(float)SV_NORM;}
static float sv_cl(float x){return x<0?0:x>1?1:x;}
static void sv_plot_line(int x0,float a,int x1,float b,uint16_t c){
    cv_line(12+x0,87-(int)(sv_cl(a)*33),12+x1,87-(int)(sv_cl(b)*33),c);
}
static void sv_grid_plot(void){
    cv_rect(9,52,222,49,sv_well);
    for(int x=12;x<=228;x+=36)cv_line(x,54,x,88,sv_grid);
    for(int y=54;y<=89;y+=8)cv_line(12,y,227,y,sv_grid);
}
static int sv_has_data(unsigned kind){return sv_data_len&&sv_data_ctx==ss.context&&sv_data_gen==ss.generation&&sv_data_kind==kind;}
static float sv_curve(float t){
    float a=sv_n(0),b=sv_n(1),c=sv_n(2),d=sv_n(3);
    if(ss.mode==SV_FIELD){
        switch(ss.page){
        case 2:return .70f+.055f*fm_sinf(t*25.f)+.24f*fm_expf(-fm_powf((t-.5f)/.018f,2.f)); // declick schematic
        case 3:return .9f-.75f*(sv_mod(t*6,1.f)<.07f); // harmonic rejection schematic
        case 5:{float sr=24000;if(sv_has_data(SV_NOISE)&&sv_data_len==3){unsigned hz=(sv_data[0]<<14)|(sv_data[1]<<7)|sv_data[2];if(hz>=8000)sr=hz;}
            float f=20.f*fm_powf(fm_minf(24000,sr*.5f)/20,t),hp=20+1980*a,lp=500+23500*b;
            const float slopes[]={6,12,24,48};int hi=clamp((int)(c*3+.5f),0,3),lo=clamp((int)(d*3+.5f),0,3);
            float db=(hp<=20?0:-10*sv_log10(1+fm_powf(hp/f,slopes[hi]/3)))+(lp>=sr*.48f?0:-10*sv_log10(1+fm_powf(f/lp,slopes[lo]/3)));return 1+db/48;}
        case 6:{float threshold=65*a/75;return fm_maxf(0,t-fm_minf(b*48/75,fm_maxf(0,threshold-t)));}
        case 7:{float threshold=60*a-60,x=t*60-60,ratio=1+15*b,over=x-threshold;
            float y=over<=-3?x:over>=3?threshold+over/ratio:x+(1/ratio-1)*(over+3)*(over+3)/12;return (y+60)/60;}
        case 8:return .28f+fm_minf(.24f,.10f*fm_powf(10,(48*a-24)/20))*fm_sinf(t*31)*fm_sinf(t*3.141593f);
        case 9:{float dur=1; if(sv_has_data(SV_NOISE)&&sv_data_len==3)dur=fm_maxf(.001f,((sv_data[0]<<14)|(sv_data[1]<<7)|sv_data[2])/1000.f);
            float fi=fm_minf(.5f,2*a/dur),fo=fm_minf(.5f,2*b/dur);return fm_minf(1,fm_minf(fi?t/fi:1,fo?(1-t)/fo:1));}
        case 10:return .28f+.20f*fm_sinf(t*31)*fm_sinf(t*3.141593f);
        default:return .5f;
        }
    }
    switch(ss.page){
    case 2:{float at=.05f+.32f*a,dec=.06f+.22f*b,rel=.08f+.28f*d;
        return t<at?t/at:t<at+dec?1-(1-c)*(t-at)/dec:t<1-rel?c:c*(1-t)/rel;}
    case 3:{float z=sv_mod(t*(1+3*a),1.f),v;int shape=(int)(d*3+.5f);
        v=shape==0?fm_sinf(z*6.2831853f):shape==1?1-4*fm_fabsf(z-.5f):shape==2?(z<.5f?1:-1):((int)(t*12)%3-1);
        return .5f+.45f*b*v;}
    case 12:{float cutoff=.08f+.84f*b,x=(t-cutoff)*12.f;int f=(int)(a*2+.5f);float low=1/(1+fm_expf(x));
        return f==0?low:f==1?4*low*(1-low):1-low;}
    case 13:return .94f*fm_powf(.15f+.8f*b,t*7)*(.5f+.5f*fm_cosf(t*(10+25*(1-a))));
    case 14:return .93f*fm_expf(-t*(1+6*(1-b)))*(.7f+.3f*fm_cosf(t*90));
    case 15:return .5f+.35f*fm_sinf(t*31+2*a*fm_sinf(t*19)+b*fm_sinf(t*73));
    case 16:return .5f+.4f*(.3f+b*.7f)*fm_sinf(t*35);
    case 11:case 17:case 18:return .5f+.38f*fm_sinf(t*25)*(.5f+.5f*fm_cosf(t*9+a*4));
    default:return t;
    }
}
static uint16_t sv_heat(unsigned v){
    static const uint32_t l[]={0x17202a,0x233247,0x2d4863,0x365e79,0x477d92,0x5b9ba2,0x77b2a8,0x9cc4b1,0xc0cdb4,0xd7c7a0,0xe8bc83,0xf3d4a2};
    static const uint32_t d[]={0x090815,0x201735,0x33244f,0x47376b,0x4f538b,0x506f99,0x568f9f,0x64afa5,0x8bc4ad,0xbccab0,0xe0bd83,0xffd5a0};
    return sv_rgb((ss.light?l:d)[v>11?11:v]);
}
static uint16_t sv_gen_color(unsigned v);
static void sv_phrase_plot(void){
    if(!sv_has_data(SV_FIELD_MAP)||sv_data_len<401){sv_text(50,63,170,&FONT_XS,"WAITING FOR FIELD",sv_trace[0]);return;}
    int n=sv_data[400];if(n>16||401+6*n>sv_data_len)return;
    for(int y=0;y<35;y++)for(int x=0;x<216;x+=3)cv_rect(12+x,54+y,3,1,sv_gen_color(sv_data[(y*20/35)*20+x*20/216]));
    for(int i=1;i<n;i++){
        int x=12+sv_data[401+6*i]*215/127,y=54+sv_data[402+6*i]*34/127;
        cv_line(12+sv_data[395+6*i]*215/127,54+sv_data[396+6*i]*34/127,x,y,i<ss.mark[2]?sv_trace[3]:sv_rgb(ss.light?0xecf4fa:0xe7f5ff));
    }
}
/* Original compact pictograph grammar, rendered entirely on the controller. */
#include "legacy_graphics.inc"
static void sv_plot(void){
    cv_rect(9,52,222,49,sv_well);
    if(ss.mode==SV_GEN_HUB&&ss.kind==SV_FIELD_MAP){sv_phrase_plot();}
    else if(sg_legacy()){} // same geometry and colors as the 0.40 graph library
    else if(ss.mode==SV_FIELD&&ss.page==0){
        if(sv_has_data(SV_SPECTRUM)&&sv_data_len==760){
            for(int y=0;y<35;y++)for(int x=0;x<36;x++)cv_rect(12+6*x,54+y,6,1,sv_heat(sv_data[(y*21/35)*36+x]));
        }else sv_text(56,63,150,&FONT_XS,"WAITING FOR SPECTRUM",sv_trace[0]);
        for(unsigned i=0;i<3;i++){
            int pos=ss.mark[i];
            if(sv_has_data(SV_SPECTRUM)&&sv_data_len==760){
                int a=sv_read14(sv_data+756),b=sv_read14(sv_data+758),v=sv_local_norm(i);
                if(v<a||v>b||b<=a)continue;pos=(v-a)*127/(b-a);
            }
            cv_line(12+pos*215/127,54,12+pos*215/127,88,sv_trace[i]);
        }
    }else if(ss.mode==SV_FIELD&&ss.page==4){
        if(sv_has_data(SV_NOISE)&&sv_data_len==756){
            for(int y=0;y<35;y++)for(int x=0;x<36;x++){
                unsigned v=sv_data[(y*21/35)*36+x];cv_rect(12+6*x,54+y,6,1,v>=12?sv_trace[(v-12)&3]:sv_heat(v));
            }
        }else sv_text(58,63,150,&FONT_XS,"LEARN NOISE PROFILE",sv_trace[2]);
    }else if(ss.mode==SV_TAPE&&ss.page==0){
        unsigned center=ss.mark[2]*54/128;
        // The original 54 three-pixel-wide peak columns and window colors.
        for(unsigned i=0;i<54;i++){
            unsigned pos=(2*i+1)*127/108;
            unsigned level=sv_has_data(SV_WAVE)?sv_data[i*sv_data_len/54]:0;
            int h=clamp((int)(level*1.1f*35/127)+1,1,35);
            int active=(ss.algo==4||ss.transport)&&i==center;
            if(active)h=35;
            cv_rect(12+i*4,54+(35-h)/2,3,h,active?sv_trace[3]:(pos>=ss.mark[0]&&pos<=ss.mark[1])?sv_trace[1]:sv_grid);
        }
    }else if(ss.mode==SV_FIELD){
        // Native versions of the original WebView's 36x21 process raster.
        // Drawing at the same sampling density preserves its original silhouette.
        sv_grid_plot();
        if(ss.page==2){
            float last=.72f+.25f*fm_expf(-.25f/(.018f*.018f)),low=.28f;
            for(int x=1;x<36;x++){float t=x/35.f,u=(t-.5f)/.018f,high=.72f+.055f*fm_sinf(t*25)+.25f*fm_expf(-u*u),v=.28f+.055f*fm_sinf(t*25);
                sv_plot_line((x-1)*215/35,last,x*215/35,high,sv_trace[3]);sv_plot_line((x-1)*215/35,low,x*215/35,v,sv_trace[1]);last=high;low=v;}
            cv_line(123,54,123,88,sv_rgb(ss.light?0x365e79:0x47376b));
        }else if(ss.page==3){
            cv_line(12,59,227,59,sv_trace[3]);for(int x=4;x<36;x+=6){int l=12+x*215/35,u=12+(x+1)*215/35;cv_line(l,59,l,81,sv_trace[3]);cv_line(l,81,u,81,sv_trace[3]);cv_line(u,81,u,59,sv_trace[3]);}
        }else if(ss.page==8||ss.page==10){
            float prev=.72f,low=.27f,scale=ss.page==8?fm_powf(10,(48*sv_n(0)-24)/20):2;
            for(int x=1;x<36;x++){float t=x/35.f,base=fm_sinf(t*31)*fm_sinf(t*3.14159265f),a=.72f+.10f*base,b=.27f+fm_minf(.24f,.10f*scale)*base;
                sv_plot_line((x-1)*215/35,prev,x*215/35,a,sv_trace[0]);sv_plot_line((x-1)*215/35,low,x*215/35,b,sv_trace[1]);prev=a;low=b;}
            if(ss.page==10){cv_line(12,71,227,71,sv_trace[3]);cv_line(12,86,227,86,sv_trace[3]);}
        }else if(ss.page!=1){
            if(ss.page==6||ss.page==7)sv_plot_line(0,0,215,1,sv_rgb(ss.light?0x365e79:0x47376b));
            uint16_t col=ss.page==5||ss.page==9?sv_trace[3]:sv_trace[0];float old=sv_curve(0);
            for(int x=1;x<36;x++){float v=sv_curve(x/35.f);sv_plot_line((x-1)*215/35,old,x*215/35,v,col);old=v;}
        }
    }
    for(unsigned i=0;i<4;i++){int x=12+i*54;cv_rect(x,93,50,5,sv_grid);if(ss.k[i].flags&SV_VALID)cv_rect(x,93,(int)(sv_n(i)*50),5,sv_trace[i]);}
}
static uint16_t sv_gen_color(unsigned v){
    static const uint32_t col[]={0x20304d,0x253b59,0x294666,0x2e5072,0x3a617e,0x49748b,0x588798,0x6d9a9f,0x89aba0,0xa5bca1,0xbec99e,0xcec890,0xdfc882,0xefc774};
    return sv_rgb(col[v>13?13:v]);
}
static void sv_generator(void){
    // Preserve the original dedicated generative deck: a square field map
    // occupying the entire physical screen, with local route/cursor overlay.
    if(sv_has_data(SV_FIELD_MAP)&&sv_data_len>=401){
        for(int y=0;y<20;y++)for(int x=0;x<20;x++)cv_rect(x*12,y*12,12,12,sv_gen_color(sv_data[20*y+x]));
        int n=sv_data[400];if(n>16||401+6*n>sv_data_len)n=0;
        for(int i=0;i<n;i++){
            int x=sv_data[401+6*i]*239/127,y=sv_data[402+6*i]*239/127;
            uint16_t c=sv_data[403+6*i]||i<ss.mark[2]?sv_trace[3]:C_WHITE;
            if(i){int px=sv_data[395+6*i]*239/127,py=sv_data[396+6*i]*239/127;cv_line(px,py,x,y,c);}
            cv_rect(x-2,y-2,5,5,c);
        }
    }else {cv_rect(0,0,240,240,sv_well);sv_text(60,110,150,&FONT_XS,"WAITING FOR FIELD",sv_trace[0]);}
}
// Compact bounded integer formatting; no hosted printf in the firmware.
static char *sv_uint(char *p,unsigned n){char tmp[12];unsigned j=0;do{tmp[j++]='0'+n%10;n/=10;}while(n);while(j)*p++=tmp[--j];*p=0;return p;}
static char *sv_append(char *p,const char *s){while(*s)*p++=*s++;*p=0;return p;}
static void sv_controls(int y0,int height){
    for(unsigned i=0;i<4;i++){
        int x=8+(i%2)*114,y=y0+(i/2)*height;SParam *k=&ss.k[i];
        cv_rect(x,y,110,height-2,sv_panel);cv_rect(x+4,y+13,102,height-23,sv_well);
        if(!(k->flags&SV_VALID))continue;
        char lab[24],*l=sv_append(lab,"K");l=sv_uint(l,i+1);l=sv_append(l," ");sv_append(l,k->name);sv_text(x+4,y,103,&FONT_XS,lab,sv_ac[i]);
        // Provisional normalized value, explicitly marked until the phone
        // returns the exact formatted value (units and enum labels).
        char value[24];
        const int changed=sv_pending(i)&&sv_local_norm(i)!=k->norm;
        if(changed&&k->step&&!(k->flags&SV_CHOICE))sv_append(sv_uint(value,(sv_local_norm(i)*100+SV_NORM/2)/SV_NORM),"% *");
        else sv_append(value,k->value);
        const felucca_font_t *font=height>=45&&text_w(&FONT_B,value)<=100?&FONT_B:&FONT_XS;
        sv_text(x+6,y+14,100,font,value,sv_trace[i]);
        cv_rect(x+4,y+height-8,102,4,sv_track);cv_rect(x+4,y+height-8,(int)(sv_n(i)*102),4,sv_ac[i]);
    }
}
static void sv_draw(void){
    sv_colors();cv_begin(240,240,sv_bg);
    if(!sv_ready){
        cv_rect(8,8,224,29,sv_ac[0]);sv_text(14,12,210,&FONT_B,"SAMPLER",ss.light?C_WHITE:sv_well);
        sv_text(16,100,210,&FONT_B,"CONNECT PHONE",sv_ink);
        sv_text(16,132,210,&FONT_XS,"OPEN FM-1 SAMPLER 0.41+",sv_dim);
        sv_text(16,147,210,&FONT_XS,"USB MIDI / AUDIO",sv_dim);
    }else if(ss.mode==SV_GENERATOR){
        sv_generator();
        // Temporary local parameter feedback over the field while adjusting.
        if(sv_pending(0)||sv_pending(1)||sv_pending(2)||sv_pending(3)){sv_controls(144,48);}
    }else{
        uint16_t accent=sv_ac[0];if(ss.mode==SV_FIELD){static const uint8_t color[]={0,0,3,2,2,3,1,0,1,3,1};accent=sv_ac[color[ss.page]];}
        else {static const uint8_t color[]={0,2,1,2,1,3,1,1,3,3,3,2,2,2,2,3,3,2,2};accent=ss.page==8?sv_red:ss.page==6?sv_rgb(ss.light?0xb51b79:0xffff64df):sv_ac[color[ss.page<19?ss.page:0]];}
        cv_rect(6,5,228,30,sv_panel);static const uint8_t tc[]={0,1,2,0,3,0,1,2};cv_rect(8,7,22,26,ss.mode!=SV_FIELD&&ss.track==3?sv_rgb(ss.light?0xb51b79:0xffff64df):sv_ac[tc[ss.track]]);
        char num[4];if(ss.mode==SV_FIELD){num[0]='F';num[1]=0;}else sv_uint(num,ss.track+1);
        sv_text(12,8,20,&FONT_B,num,ss.light?C_WHITE:sv_well);
        cv_rect(33,7,174,26,ss.mode==SV_FIELD?sv_ac[0]:sv_raised);
        sv_text(38,10,165,&FONT_B,ss.title,ss.mode==SV_FIELD?(ss.light?C_WHITE:sv_well):sv_ink);
        if(ss.transport==0)cv_rect(214,13,13,13,sv_ink);
        else if(ss.transport==1)for(int i=0;i<15;i++)cv_line(213,11+i,213+(i<8?i:14-i)*2,11+i,sv_ac[1]);
        else for(int y=-7;y<=7;y++){int w=(int)fm_sqrtf(49-y*y);cv_line(220-w,19+y,220+w,19+y,sv_red);}
        cv_rect(12,36,101,15,accent);sv_text(14,36,97,&FONT_XS,ss.pageTitle,ss.light?C_WHITE:sv_well);
        for(unsigned i=0;i<10;i++)cv_rect(116+i*12,43,8,6,(ss.mask>>i)&1u?accent:sv_track);
        sv_plot();
        cv_rect(12,103,216,4,sv_track);cv_rect(12,103,ss.progress*216/SV_NORM,4,sv_ac[3]);
        sv_text(12,109,216,&FONT_XS,ss.status,sv_dim);sv_controls(121,48);
        sv_text(12,226,216,&FONT_XS,ss.mode==SV_FIELD?"SEL SIGNAL  PRE FFT  ALG WINDOW":"HOLD HOME / REC: HUB / DECK",sv_dim);
    }
    // Compare native rendered scan bands; unchanged bands never touch LCD.
    for(unsigned b=0;b<8;b++){
        uint32_t sig=2166136261u;for(unsigned i=b*7200;i<(b+1)*7200;i++)sig=(sig^cv_px[i])*16777619u;
        if(sig!=sv_bands[b]){lcd_sync();lcd_blit(0,b*30,240,30,cv_px+b*7200);sv_bands[b]=sig;}
    }
}
