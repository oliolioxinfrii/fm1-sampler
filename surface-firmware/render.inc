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
        case 2:return .70f+.055f*sinf(t*25.f)+.24f*expf(-powf((t-.5f)/.018f,2.f)); // declick schematic
        case 3:return .9f-.75f*(fmodf(t*6,1.f)<.07f); // harmonic rejection schematic
        case 5:{float f=20.f*powf(1200.f,t),hp=20+1980*a,lp=500+23500*b;
            const float slopes[]={6,12,24,48};int hi=clamp((int)(c*3+.5f),0,3),lo=clamp((int)(d*3+.5f),0,3);
            float db=-10*log10f(1+powf(hp/f,slopes[hi]/3))-10*log10f(1+powf(f/lp,slopes[lo]/3));return 1+db/48;}
        case 6:{float threshold=65*a/75;return t-fminf(b*48/75,fmaxf(0,threshold-t));}
        case 7:{float threshold=60*a-60,x=t*60-60,ratio=1+15*b,over=x-threshold;
            float y=over<=-3?x:over>=3?threshold+over/ratio:x+(1/ratio-1)*(over+3)*(over+3)/12;return (y+60)/60;}
        case 8:return .28f+fminf(.24f,.10f*powf(10,(48*a-24)/20))*sinf(t*31)*sinf(t*3.141593f);
        case 9:{float dur=1; if(sv_has_data(SV_NOISE)&&sv_data_len==3)dur=fmaxf(.001f,((sv_data[0]<<14)|(sv_data[1]<<7)|sv_data[2])/1000.f);
            float fi=fminf(.5f,2*a/dur),fo=fminf(.5f,2*b/dur);return fminf(1,fminf(fi?t/fi:1,fo?(1-t)/fo:1));}
        case 10:return .28f+.20f*sinf(t*31)*sinf(t*3.141593f);
        default:return .5f;
        }
    }
    switch(ss.page){
    case 2:{float at=.05f+.32f*a,dec=.06f+.22f*b,rel=.08f+.28f*d;
        return t<at?t/at:t<at+dec?1-(1-c)*(t-at)/dec:t<1-rel?c:c*(1-t)/rel;}
    case 3:{float z=fmodf(t*(1+3*a),1.f),v;int shape=(int)(d*3+.5f);
        v=shape==0?sinf(z*6.2831853f):shape==1?1-4*fabsf(z-.5f):shape==2?(z<.5f?1:-1):((int)(t*12)%3-1);
        return .5f+.45f*b*v;}
    case 12:{float cutoff=.08f+.84f*b,x=(t-cutoff)*12.f;int f=(int)(a*2+.5f);float low=1/(1+expf(x));
        return f==0?low:f==1?4*low*(1-low):1-low;}
    case 13:return .94f*powf(.15f+.8f*b,t*7)*(.5f+.5f*cosf(t*(10+25*(1-a))));
    case 14:return .93f*expf(-t*(1+6*(1-b)))*(.7f+.3f*cosf(t*90));
    case 15:return .5f+.35f*sinf(t*31+2*a*sinf(t*19)+b*sinf(t*73));
    case 16:return .5f+.4f*(.3f+b*.7f)*sinf(t*35);
    case 11:case 17:case 18:return .5f+.38f*sinf(t*25)*(.5f+.5f*cosf(t*9+a*4));
    default:return t;
    }
}
static uint16_t sv_heat(unsigned v){
    static const uint32_t l[]={0x17202a,0x233247,0x2d4863,0x365e79,0x477d92,0x5b9ba2,0x77b2a8,0x9cc4b1,0xc0cdb4,0xd7c7a0,0xe8bc83,0xf3d4a2};
    static const uint32_t d[]={0x090815,0x201735,0x33244f,0x47376b,0x4f538b,0x506f99,0x568f9f,0x64afa5,0x8bc4ad,0xbccab0,0xe0bd83,0xffd5a0};
    return sv_rgb((ss.light?l:d)[v>11?11:v]);
}
static void sv_plot(void){
    sv_grid_plot();
    if(ss.mode==SV_FIELD&&ss.page==0){
        if(sv_has_data(SV_SPECTRUM)&&sv_data_len==756){
            for(int y=0;y<35;y++)for(int x=0;x<36;x++)cv_rect(12+6*x,54+y,6,1,sv_heat(sv_data[(y*21/35)*36+x]));
        }else sv_text(56,63,150,&FONT_XS,"WAITING FOR SPECTRUM",sv_trace[0]);
        for(unsigned i=0;i<3;i++){
            int pos=ss.mark[i];if(sv_pending(i)){int d=(int)sv_local_norm(i)-ss.k[i].norm;pos=clamp(pos+d*127/SV_NORM,0,127);}
            cv_line(12+pos*215/127,54,12+pos*215/127,88,sv_trace[i]);
        }
    }else if(ss.mode==SV_FIELD&&ss.page==4){
        if(sv_has_data(SV_NOISE)&&sv_data_len==756){
            for(int y=0;y<35;y++)for(int x=0;x<36;x++){
                unsigned v=sv_data[(y*21/35)*36+x];if(v>=12)cv_rect(12+6*x,54+y,6,1,sv_trace[(v-12)&3]);
            }
        }else sv_text(58,63,150,&FONT_XS,"LEARN NOISE PROFILE",sv_trace[2]);
    }else if(ss.mode==SV_TAPE&&ss.page==0){
        if(sv_has_data(SV_WAVE)){
            for(int x=0;x<216;x++){int h=sv_data[x*sv_data_len/216]*16/127;cv_line(x+12,71-h,x+12,71+h,sv_trace[0]);}
        }
        cv_line(12+(int)(sv_n(0)*215),54,12+(int)(sv_n(0)*215),88,sv_trace[0]);
        cv_line(12+(int)(sv_n(1)*215),54,12+(int)(sv_n(1)*215),88,sv_trace[1]);
        if(ss.transport)cv_line(12+ss.mark[2]*215/127,54,12+ss.mark[2]*215/127,88,sv_trace[3]);
    }else if(ss.mode!=SV_FIELD&&ss.page==6){
        cv_rect(12,59,215,9,sv_grid);cv_rect(12,59,ss.mark[2]*215/127,9,sv_trace[1]);
        cv_rect(12,75,(int)(sv_n(0)*215),8,sv_trace[0]);
    }else if(ss.mode!=SV_FIELD&&ss.page==8){
        cv_rect(12,65,215,12,sv_grid);cv_rect(12,65,ss.mark[2]*215/127,12,sv_trace[1]);cv_line(12+(int)(sv_n(3)*215),58,12+(int)(sv_n(3)*215),84,sv_trace[3]);
    }else if(ss.mode!=SV_FIELD&&(ss.page==9||ss.page==10)){
        for(int i=0;i<8;i++)cv_rect(12+i*27,60,24,19,sv_grid);
        cv_line(12+ss.progress*215/SV_NORM,54,12+ss.progress*215/SV_NORM,88,sv_trace[3]);
    }else if(ss.mode!=SV_FIELD&&ss.page==7){
        for(int i=0;i<14;i++){int x=12+15*i;cv_rect(x,58,13,28,sv_trace[0]);if(i%7!=2&&i%7!=6)cv_rect(x+9,58,8,17,sv_well);}
    }else if(ss.mode!=SV_FIELD&&ss.page==4){
        for(int i=0;i<8;i++){int x=12+i*27;int y=83-(int)(18*fmodf(i*(.4f+sv_n(1)),1));cv_rect(x,y,14+(int)(sv_n(3)*9),3,sv_trace[2]);}
    }else{
        if(ss.mode==SV_FIELD&&(ss.page==6||ss.page==7))sv_plot_line(0,0,215,1,sv_grid);
        if(ss.mode==SV_FIELD&&(ss.page==2||ss.page==8||ss.page==10)){
            float old=.72f;for(int x=1;x<216;x++){float t=x/215.f,v=.72f+.055f*sinf(t*31)*sinf(t*3.141593f);sv_plot_line(x-1,old,x,v,sv_trace[1]);old=v;}
        }
        float old=sv_curve(0);for(int x=1;x<216;x++){float v=sv_curve(x/215.f);sv_plot_line(x-1,old,x,v,sv_trace[0]);old=v;}
        if(ss.mode!=SV_FIELD&&ss.page==3)cv_line(12+ss.mark[2]*215/127,54,12+ss.mark[2]*215/127,88,sv_trace[3]);
    }
    for(unsigned i=0;i<4;i++){int x=12+i*54;cv_rect(x,93,50,5,sv_grid);if(ss.k[i].flags&SV_VALID)cv_rect(x,93,(int)(sv_n(i)*50),5,sv_trace[i]);}
}
static uint16_t sv_gen_color(unsigned v){
    static const uint32_t col[]={0x182e55,0x294d79,0x396b99,0x4989b6,0x67a6cb,0x9bc3d8,0xd0e0db,0xe5e4ce,0xe6d5ac,0xe4be85,0xdca260,0xca8151,0xa95b47,0x793945};
    return sv_rgb(col[v>13?13:v]);
}
static void sv_generator(void){
    // Preserve the original dedicated generative deck: a square field map
    // occupying the entire physical screen, with local route/cursor overlay.
    if(sv_has_data(SV_FIELD_MAP)&&sv_data_len>=401){
        for(int y=0;y<20;y++)for(int x=0;x<20;x++)cv_rect(x*12,y*12,12,12,sv_gen_color(sv_data[20*y+x]));
        int n=sv_data[400];if(n>16||401+3*n>sv_data_len)n=0;
        for(int i=0;i<n;i++){
            int x=sv_data[401+3*i]*239/127,y=sv_data[402+3*i]*239/127;
            uint16_t c=sv_data[403+3*i]||i<ss.mark[2]?sv_trace[3]:C_WHITE;
            if(i){int px=sv_data[398+3*i]*239/127,py=sv_data[399+3*i]*239/127;cv_line(px,py,x,y,c);}
            cv_rect(x-2,y-2,5,5,c);
        }
    }else {cv_rect(0,0,240,240,sv_well);sv_text(60,110,150,&FONT_XS,"WAITING FOR FIELD",sv_trace[0]);}
}
static void sv_controls(int y0,int height){
    for(unsigned i=0;i<4;i++){
        int x=8+(i%2)*114,y=y0+(i/2)*height;SParam *k=&ss.k[i];
        cv_rect(x,y,110,height-2,sv_panel);cv_rect(x+4,y+13,102,height-23,sv_well);
        if(!(k->flags&SV_VALID))continue;
        char lab[24];snprintf(lab,sizeof lab,"K%u %s",i+1,k->name);sv_text(x+4,y,103,&FONT_XS,lab,sv_ac[i]);
        // Provisional normalized value, explicitly marked until the phone
        // returns the exact formatted value (units and enum labels).
        char value[24];if(sv_pending(i)&&k->step)snprintf(value,sizeof value,"%u%% *",(sv_local_norm(i)*100+SV_NORM/2)/SV_NORM);else snprintf(value,sizeof value,"%s%s",k->value,sv_pending(i)?" *":"");
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
        else {static const uint8_t color[]={0,2,1,2,3,3,1,0,3,3,3,2,2,2,2,3,3,2,2};accent=sv_ac[color[ss.page<19?ss.page:0]];}
        cv_rect(6,5,228,30,sv_panel);cv_rect(8,7,22,26,sv_ac[ss.track%4]);
        char num[4];if(ss.mode==SV_FIELD)strcpy(num,"F");else snprintf(num,sizeof num,"%u",ss.track+1);
        sv_text(12,8,20,&FONT_B,num,ss.light?C_WHITE:sv_well);
        cv_rect(33,7,174,26,ss.mode==SV_FIELD?sv_ac[0]:sv_raised);
        sv_text(38,10,165,&FONT_B,ss.title,ss.mode==SV_FIELD?(ss.light?C_WHITE:sv_well):sv_ink);
        if(ss.transport==0)cv_rect(214,13,13,13,sv_ink);
        else if(ss.transport==1)for(int i=0;i<15;i++)cv_line(213,11+i,213+(i<8?i:14-i)*2,11+i,sv_ac[1]);
        else for(int y=-7;y<=7;y++){int w=(int)sqrtf(49-y*y);cv_line(220-w,19+y,220+w,19+y,sv_red);}
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
