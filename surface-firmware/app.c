/* Native state-based surface. Part of fm1-base's unity build.
 * No synthesis: the platform mixes USB playback to the output.
 * The phone is authoritative; the controller predicts normalized readouts
 * and graphs while ordered edits are outstanding.
 */
#include "../shared/surface_v2.h"
// fm1-base's device unity unit already includes the freestanding BSP
// implementations of memcpy, memset and memcmp. Hosted simulator has libc.
#ifdef PLAT_HOST
#include <string.h>
#endif
#include "fastmath.h"

typedef struct {
    uint16_t norm,step;
    uint8_t flags;
    char name[SV_NAME+1],value[SV_VALUE+1];
} SParam;
typedef struct {
    uint16_t context;
    uint8_t valid;
    SParam k[4];
} SPage;
typedef struct {
    uint16_t context,ack,progress,generation,mask;
    uint8_t light,mode,track,page,algo,transport,kind,mark[3];
    char title[SV_TITLE+1],pageTitle[SV_TITLE+1],status[SV_STATUS+1];
    SParam k[4];
} SState;
typedef struct {uint16_t seq,context;int8_t delta;uint8_t slot;} SEdit;
static SState ss;
static PLAT_POOL SPage sv_cache[3][8][SV_PAGES];
static SPage sv_field_cache[11];
static uint8_t sv_data[SV_DATA_MAX],sv_stage[SV_DATA_MAX];
typedef struct{uint16_t gen,len;uint8_t kind;uint32_t used;uint8_t bytes[SV_DATA_MAX];} SBlob;
static PLAT_POOL SBlob sv_blobs[36];
static uint32_t sv_blob_clock,sv_requested_at;
static uint16_t sv_data_len,sv_data_ctx,sv_data_gen,sv_stage_len,sv_stage_ctx,sv_stage_gen;
static uint8_t sv_stage_kind,sv_stage_mask,sv_data_kind;
static uint8_t sv_ready,sv_dirty=1,sv_seen_ping,sv_oct=3,sv_root,sv_keymode;
static uint32_t sv_usb,sv_ping_at,sv_nav_until,sv_draw_at,sv_bands[8];
static uint32_t sv_keys,sv_btn;
static uint16_t sv_seq;
static SEdit sv_edits[32];
static unsigned sv_count;
static int sv_pending_delta[4];
static uint32_t sv_motion_at[4];
static int sv_motion_dir[4],sv_motion_streak[4];
static uint32_t sv_led_buttons,sv_led_keys;
static const uint8_t sv_buttons[NB]={2,3,4,5,6,7,8,9,10,11,12,13,0,1};
static const uint8_t sv_encoders[NE]={0,1,6,2,3,4,5};

static int sv_send(uint8_t cmd,const uint8_t *p,unsigned n){
    uint8_t f[32];if(n>24||plat_sysex_room()<n+8)return 0;
    f[0]=0xf0;f[1]=0x7d;f[2]='D';f[3]='A';f[4]='W';f[5]=1;f[6]=cmd;
    if(n)memcpy(f+7,p,n);f[7+n]=0xf7;
    return plat_sysex_send(f,n+8)==0;
}
static int sv_midi(unsigned status,unsigned a,unsigned b){
    if(!plat_midi_out_room(TO_USB))return 0;
    plat_midi_out((status>>4)|(status<<8)|(a<<16)|(b<<24));return 1;
}
static void sv_reset_link(void){
    sv_ready=sv_seen_ping=0;sv_count=0;sv_seq=0;sv_nav_until=0;sv_data_len=sv_stage_mask=0;
    memset(sv_pending_delta,0,sizeof sv_pending_delta);
    memset(sv_motion_streak,0,sizeof sv_motion_streak);memset(sv_motion_dir,0,sizeof sv_motion_dir);
    memset(sv_cache,0,sizeof sv_cache);memset(sv_field_cache,0,sizeof sv_field_cache);
    memset(sv_blobs,0,sizeof sv_blobs);sv_blob_clock=sv_requested_at=0;
    sv_keys=plat_keys();sv_btn=plat_buttons();
    for(int i=0;i<NE;i++)(void)plat_enc(i);
    plat_lights(0,0,0,0,0);sv_dirty=1;
}
static void sv_read_text(char *dest,const uint8_t *src,unsigned n){
    for(unsigned i=0;i<n;i++)dest[i]=(src[i]>=32&&src[i]<=126)?(char)src[i]:0;
    dest[n]=0;
}
static void sv_read_knob(SParam *k,const uint8_t *p){
    k->flags=p[0]&15;k->norm=sv_read14(p+1);k->step=sv_read14(p+3);
    sv_read_text(k->name,p+5,SV_NAME);sv_read_text(k->value,p+5+SV_NAME,SV_VALUE);
}
static void sv_state(const uint8_t *p,unsigned n){
    if(n!=SV_STATE_BYTES||p[0]>1||p[6]>SV_FIELD||p[7]>=8||p[8]>=SV_PAGES||p[9]>5||p[10]>2||p[17]>SV_FIELD_MAP)return;
    unsigned mode=p[6],track=p[7],page=p[8],ctx=sv_read14(p+1);
    if(ctx!=sv_context(mode,track,page,p[9])||(mode==SV_FIELD&&(track||page>10)))return;
    SPage *cache=mode==SV_FIELD?&sv_field_cache[page]:&sv_cache[mode][track][page];
    cache->context=(uint16_t)ctx;cache->valid=1;
    for(unsigned i=0;i<4;i++)sv_read_knob(&cache->k[i],p+SV_HEADER_BYTES+2*SV_TITLE+SV_STATUS+i*SV_KNOB_BYTES);
    if(!p[0])return;
    SState next;memset(&next,0,sizeof next);
    next.context=(uint16_t)ctx;next.ack=sv_read14(p+3);next.light=p[5];next.mode=mode;next.track=track;next.page=page;next.algo=p[9];next.transport=p[10];
    next.mask=sv_read14(p+11);next.progress=sv_read14(p+13);next.generation=sv_read14(p+15);next.kind=p[17];memcpy(next.mark,p+18,3);
    sv_read_text(next.title,p+SV_HEADER_BYTES,SV_TITLE);
    sv_read_text(next.pageTitle,p+SV_HEADER_BYTES+SV_TITLE,SV_TITLE);
    sv_read_text(next.status,p+SV_HEADER_BYTES+2*SV_TITLE,SV_STATUS);
    memcpy(next.k,cache->k,sizeof next.k);
    unsigned kept=0,oldcount=sv_count;
    for(unsigned i=0;i<sv_count;i++)if(sv_after(sv_edits[i].seq,next.ack)&&sv_edits[i].context==ctx)sv_edits[kept++]=sv_edits[i];
    sv_count=kept;
    if(!sv_ready||ss.context!=ctx){
        sv_nav_until=0;memset(sv_pending_delta,0,sizeof sv_pending_delta);memset(sv_motion_dir,0,sizeof sv_motion_dir);
    }
    if(!sv_ready||oldcount!=sv_count||memcmp(&ss,&next,sizeof ss))sv_dirty=1;
    ss=next;sv_ready=1;
    if(next.kind&&(!sv_data_len||sv_data_gen!=next.generation||sv_data_kind!=next.kind)){
        for(unsigned i=0;i<36;i++)if(sv_blobs[i].len&&sv_blobs[i].gen==next.generation&&sv_blobs[i].kind==next.kind){
            memcpy(sv_data,sv_blobs[i].bytes,sv_blobs[i].len);sv_data_len=sv_blobs[i].len;sv_data_gen=next.generation;sv_data_kind=next.kind;sv_blobs[i].used=++sv_blob_clock;sv_dirty=1;break;
        }
    }
    if(sv_data_gen==next.generation&&sv_data_kind==next.kind)sv_data_ctx=next.context;
}
static void sv_chunk(const uint8_t *p,unsigned n){
    if(n<9)return;
    unsigned ctx=sv_read14(p),gen=sv_read14(p+2),kind=p[4],total=sv_read14(p+5),off=sv_read14(p+7),len=n-9;
    if(!total||total>SV_DATA_MAX||kind<SV_WAVE||kind>SV_FIELD_MAP||off>=total||off%SV_CHUNK||len!=(total-off>SV_CHUNK?SV_CHUNK:total-off))return;
    if(sv_ready&&(ctx!=ss.context||gen!=ss.generation||kind!=ss.kind))return;
    if(ctx!=sv_stage_ctx||gen!=sv_stage_gen||kind!=sv_stage_kind||total!=sv_stage_len){
        sv_stage_ctx=ctx;sv_stage_gen=gen;sv_stage_kind=kind;sv_stage_len=total;sv_stage_mask=0;
    }
    memcpy(sv_stage+off,p+9,len);sv_stage_mask|=1u<<(off/SV_CHUNK);
    if(sv_stage_mask==((1u<<((total+SV_CHUNK-1)/SV_CHUNK))-1u)){
        if(total!=sv_data_len||ctx!=sv_data_ctx||gen!=sv_data_gen||kind!=sv_data_kind||memcmp(sv_data,sv_stage,total))sv_dirty=1;
        memcpy(sv_data,sv_stage,total);sv_data_len=total;sv_data_ctx=ctx;sv_data_gen=gen;sv_data_kind=kind;
        unsigned at=0;for(unsigned i=0;i<36;i++){if(sv_blobs[i].len&&sv_blobs[i].gen==gen&&sv_blobs[i].kind==kind){at=i;break;}if(sv_blobs[i].used<sv_blobs[at].used)at=i;}
        SBlob *b=&sv_blobs[at];b->gen=gen;b->len=total;b->kind=kind;b->used=++sv_blob_clock;memcpy(b->bytes,sv_stage,total);
    }
}
int app_sysex(const uint8_t *p,uint32_t n){
    if(n<6||p[0]!=0x7d||p[1]!='D'||p[2]!='A'||p[3]!='W'||p[4]!=1)return 0;
    for(unsigned i=0;i<n;i++)if(p[i]&128)return 1;
    unsigned cmd=p[5];p+=6;n-=6;
    if(cmd==5&&n==2){
        if(!sv_seen_ping){if(!sv_send(0x43,0,0))return 1;sv_seen_ping=1;}
        uint8_t v=2;(void)sv_send(SV_HELLO,&v,1);sv_ping_at=plat_ms();
    }else if(cmd==SV_STATE)sv_state(p,n);
    else if(cmd==SV_DATA)sv_chunk(p,n);
    else if(cmd==1&&n==6){
        uint64_t mask=0;for(unsigned i=0;i<6;i++)mask|=(uint64_t)p[i]<<(i*7);
        sv_led_buttons=0;for(unsigned i=0;i<NB;i++)if(mask&(1u<<sv_buttons[i]))sv_led_buttons|=1u<<i;
        sv_led_keys=(uint32_t)(mask>>14)&0x7ffffffu;
        plat_lights(sv_led_buttons,sv_led_keys,0,0,0);
    }else if(cmd==6&&n==4){sv_oct=p[2]>6?6:p[2];sv_root=p[1];sv_keymode=p[3];uint8_t v[]={7,sv_root,sv_oct,sv_keymode};(void)sv_send(0x41,v,4);}
    return 1; // legacy graphics ignored; non-DAW updater frames left to platform
}
static unsigned sv_sent_norm(unsigned i){
    unsigned v=ss.k[i].norm;
    for(unsigned j=0;j<sv_count;j++)if(sv_edits[j].context==ss.context&&sv_edits[j].slot==i)v=sv_parameter_predict(v,ss.k[i].step,sv_edits[j].delta,ss.k[i].flags);
    return v;
}
static unsigned sv_local_norm(unsigned i){
    return sv_parameter_predict(sv_sent_norm(i),ss.k[i].step,sv_pending_delta[i],ss.k[i].flags);
}
static int sv_pending(unsigned i){
    if(sv_pending_delta[i])return 1;
    for(unsigned j=0;j<sv_count;j++)if(sv_edits[j].context==ss.context&&sv_edits[j].slot==i)return 1;
    return 0;
}
static void sv_send_edits(void){
    if(!sv_ready)return;
    for(unsigned i=0;i<4&&sv_count<32;i++){
        int d=sv_pending_delta[i];if(!d)continue;
        d=sv_limit_delta(sv_sent_norm(i),ss.k[i].step,d,ss.k[i].flags);
        sv_pending_delta[i]=d;
        if(!d)continue;
        d=clamp(d,-63,63);
        uint16_t seq=(sv_seq+1)&16383;uint8_t p[7];
        sv_write14(p,ss.context);sv_write14(p+2,seq);p[4]=i;p[5]=d+63;p[6]=0;
        if(!sv_send(SV_EDIT,p,7))return;
        sv_seq=seq;sv_edits[sv_count++]=(SEdit){seq,ss.context,(int8_t)d,(uint8_t)i};sv_pending_delta[i]-=d;
    }
}
static void sv_input(void){
    uint32_t now=plat_ms(),usb=plat_usb_session(),keys=plat_keys(),buttons=plat_buttons();
    if(usb!=sv_usb){sv_usb=usb;sv_reset_link();}
    if(sv_seen_ping&&now-sv_ping_at>3000){sv_reset_link();}
    uint32_t packet;int port;for(int i=0;i<32&&plat_midi_in(&packet,&port);i++){}
    if(!sv_ready){sv_keys=keys;sv_btn=buttons;for(int i=0;i<NE;i++)(void)plat_enc(i);return;}
    for(unsigned i=0;i<NKEYS;i++)if(((keys^sv_keys)>>i)&1u){
        int down=(keys>>i)&1u;
        if(sv_midi(down?0x90:0x80,53+i,down?100:0)){sv_keys^=1u<<i;}
    }
    for(unsigned i=0;i<NB;i++)if(((buttons^sv_btn)>>i)&1u){
        int down=(buttons>>i)&1u;
        if(i==B_OCTDN||i==B_OCTUP){
            if(down){uint8_t oct=(uint8_t)clamp((int)sv_oct+(i==B_OCTUP?1:-1),0,6);uint8_t p[]={7,sv_root,oct,sv_keymode};if(!sv_send(0x41,p,4))continue;sv_oct=oct;}
        }else{
            if(!sv_midi(down?0x91:0x81,sv_buttons[i],down?100:0))continue;
            if(i!=B_PLAY){sv_nav_until=now+100;memset(sv_pending_delta,0,sizeof sv_pending_delta);}
        }
        sv_btn^=1u<<i;
    }
    for(unsigned i=0;i<NE;i++){
        int d=plat_enc(i);if(!d)continue;
        unsigned role=sv_encoders[i];d=clamp(d,-63,63);
        if(role==0||role==1||role==6){
            if(sv_midi(0xb0,16+role,d+64)){sv_nav_until=now+100;memset(sv_pending_delta,0,sizeof sv_pending_delta);}
            continue;
        }
        unsigned slot=role-2;
        if((int32_t)(now-sv_nav_until)<0||!(ss.k[slot].flags&SV_VALID))continue;
        SParam *p=&ss.k[slot];
        // No edits, acceleration buildup or provisional text at a hard stop.
        if(p->step&&!sv_limit_delta(sv_local_norm(slot),p->step,d,p->flags)){
            sv_motion_dir[slot]=sv_motion_streak[slot]=0;continue;
        }
        int dir=d>0?1:-1,det=d<0?-d:d;uint32_t dt=now-sv_motion_at[slot];
        int fast=dir==sv_motion_dir[slot]&&dt<=150&&dt/(unsigned)det<=65;
        sv_motion_streak[slot]=fast?clamp(sv_motion_streak[slot]+det,0,12):0;sv_motion_dir[slot]=dir;sv_motion_at[slot]=now;
        int mult=fast&&(ss.k[slot].flags&SV_ACCEL)?sv_motion_streak[slot]>=6?5:sv_motion_streak[slot]>=3?3:2:1;
        int delta=clamp(sv_pending_delta[slot]+d*mult,-504,504);
        delta=sv_limit_delta(sv_sent_norm(slot),p->step,delta,p->flags);
        if(delta!=sv_pending_delta[slot]){sv_pending_delta[slot]=delta;sv_dirty=1;}
    }
    sv_send_edits();
    if(ss.kind&&(!sv_data_len||sv_data_gen!=ss.generation||sv_data_kind!=ss.kind)&&now-sv_requested_at>=400){
        if(sv_send(SV_REQUEST,0,0))sv_requested_at=now;
    }
}

#include "render.inc"

void app_init(void){sv_usb=0;ss.light=1;sv_reset_link();}
void app_render(int32_t *lr,uint32_t n,uint32_t master_q12){
    (void)master_q12;for(uint32_t i=0;i<n*2;i++)lr[i]=0;
}
void app_panic(void){sv_reset_link();}
void app_ui_init(void){lcd_fill(0,0,240,240,0);sv_dirty=1;memset(sv_bands,0,sizeof sv_bands);}
void app_input_only(void){sv_input();}
void app_say(const char *a,const char *b){(void)a;(void)b;sv_dirty=1;}
void app_frame(void){
    sv_input();uint32_t now=plat_ms();
    if(sv_ready&&ss.mode!=SV_FIELD&&ss.mode!=SV_GENERATOR&&ss.page==16&&now-sv_draw_at>=200)sv_dirty=1;
    if(sv_dirty&&now-sv_draw_at>=16){sv_dirty=0;sv_draw_at=now;sv_draw();}
}
#ifdef PLAT_HOST
int app_expect(const char *what,int *got){
    if(!strcmp(what,"surface_ready"))*got=sv_ready;
    else if(!strcmp(what,"surface_mode"))*got=ss.mode;
    else if(!strcmp(what,"surface_page"))*got=ss.page;
    else if(!strcmp(what,"surface_track"))*got=ss.track;
    else if(!strcmp(what,"surface_data_len"))*got=sv_data_len;
    else if(!strcmp(what,"surface_pending"))*got=sv_count;
    else if(!strcmp(what,"surface_value_confirmed")){
        *got=!strcmp(sv_control_value(0),"0.5 s");
    }
    else if(!strcmp(what,"surface_arp_patterns")){
        static const int expected[4][8]={{0,1,2,0,1,2,0,1},{2,1,0,2,1,0,2,1},{0,1,2,1,0,1,2,1},{0,2,1,0,2,1,0,2}};
        int pitch[24],color[24],ok=1;
        for(int p=0;p<5;p++)for(int n=3;n<=12;n+=3){
            sg_arp_preview(p,n,24,pitch,color);
            int blue=0,violet=0,lo=n,hi=-1;
            for(int i=0;i<24;i++){if(pitch[i]<lo)lo=pitch[i];if(pitch[i]>hi)hi=pitch[i];}
            for(int i=0;i<24;i++){
                if(n==3&&p<4&&i<8&&pitch[i]!=expected[p][i])ok=0;
                if(pitch[i]<0||pitch[i]>=n)ok=0;
                if(p<4&&i>0&&pitch[i]==pitch[i-1])ok=0;
                if(color[i]==1){if(pitch[i]!=lo||blue++)ok=0;}
                if(color[i]==2){if(pitch[i]!=hi||violet++)ok=0;}
                if(!blue&&pitch[i]==lo)ok=0;
                if(!violet&&pitch[i]==hi&&hi!=lo)ok=0;
            }
            if(blue!=1||violet!=(hi!=lo))ok=0;
        }
        *got=ok;
    }
    else if(!strcmp(what,"surface_plot_color")){
        // Exercise the exact production pixel writer in the host canvas.
        const unsigned at=54*240+12;uint16_t old=cv_px[at],color=sv_rgb(0x51b8ff);
        sg_put(12,54,color);*got=cv_px[at]==swap16(color);cv_px[at]=old;
    }
    else if(!strcmp(what,"surface_key_left")||!strcmp(what,"surface_key_right")){
        int lo=228,hi=11;uint16_t bg=swap16(sv_well);
        // Restrict measurement to the keys, excluding the full-width octave rail.
        for(int y=65;y<79;y++)for(int x=12;x<228;x++)if(cv_px[y*240+x]!=bg){if(x<lo)lo=x;if(x>hi)hi=x;}
        int width=hi-lo+1;
        *got=width>=58&&width<=64&&(!strcmp(what,"surface_key_left")?(lo>=20&&lo<=26):(hi>=212&&hi<=219));
    }
    else if(!strcmp(what,"surface_plot_wide")){
        int lo=228,hi=11;uint16_t bg=swap16(sv_well);
        for(int y=54;y<89;y++)for(int x=12;x<228;x++)if(cv_px[y*240+x]!=bg){if(x<lo)lo=x;if(x>hi)hi=x;}
        *got=lo>=16&&lo<=28&&hi>=211&&hi<=223;
    }
    else if(!strcmp(what,"surface_plot_span")||!strcmp(what,"surface_plot_left")||!strcmp(what,"surface_plot_right")){
        int lo=228,hi=11;uint16_t bg=swap16(sv_well);
        for(int y=54;y<89;y++)for(int x=12;x<228;x++)if(cv_px[y*240+x]!=bg){if(x<lo)lo=x;if(x>hi)hi=x;}
        *got=!strcmp(what,"surface_plot_span")?hi-lo+1:!strcmp(what,"surface_plot_left")?lo:hi;
    }
    else if(!strncmp(what,"surface_knob",12)&&what[12]>='0'&&what[12]<='3')*got=sv_local_norm(what[12]-'0');
    else return 0;
    return 1;
}
#endif
