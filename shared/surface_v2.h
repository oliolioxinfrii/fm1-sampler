/* Sampler surface protocol, C/C++ shared wire contract. All payload bytes are 7-bit.
 * Envelope F0 7D DAW 01 cmd ... F7. Old firmware ignores the new commands.
 * State is authoritative on phone; firmware owns rendering and optimistic normalized bars.
 */
#ifndef SAMPLER_SURFACE_V2_H
#define SAMPLER_SURFACE_V2_H
#include <stdint.h>
#include <stddef.h>
enum { SV_HELLO=0x44, SV_EDIT=0x45, SV_REQUEST=0x46, SV_STATE=0x20, SV_DATA=0x21 };
enum { SV_TAPE=0, SV_GEN_HUB=1, SV_GENERATOR=2, SV_FIELD=3 };
enum { SV_WAVE=1, SV_SPECTRUM=2, SV_NOISE=3, SV_FIELD_MAP=4 };
enum { SV_VALID=1, SV_CHOICE=2, SV_ACCEL=4, SV_ACTION=8 };
enum { SV_NORM=16383, SV_NAME=14, SV_VALUE=14, SV_TITLE=20, SV_STATUS=36,
       SV_KNOB_BYTES=5+SV_NAME+SV_VALUE, SV_HEADER_BYTES=21,
       SV_STATE_BYTES=SV_HEADER_BYTES+SV_TITLE+SV_TITLE+SV_STATUS+4*SV_KNOB_BYTES,
       SV_CHUNK=180, SV_DATA_MAX=756, SV_PAGES=20 };
/* header: active, context(2), ack(2), theme, mode, track, page, algo,
 * transport, menu mask(2), progress(2), data generation(2), kind,
 * marker A/B/position (each 0..127; 127 is right edge).
 * strings: source title, page title, status; knobs: flags,norm(2),step(2),name,value.
 */
static inline uint16_t sv_read14(const uint8_t *p){return (uint16_t)((p[0]<<7)|p[1]);}
static inline void sv_write14(uint8_t *p,unsigned v){p[0]=(uint8_t)((v>>7)&127);p[1]=(uint8_t)(v&127);}
static inline unsigned sv_context(unsigned mode,unsigned track,unsigned page,unsigned algo){
    return (((mode*8+track)*SV_PAGES+page)*6+algo)&16383u;
}
static inline int sv_after(unsigned a,unsigned b){unsigned d=(a-b)&16383u;return d&&d<8192u;}
static inline unsigned sv_predict(unsigned norm,unsigned stride,int delta){
    int v=(int)norm+(int)stride*delta;return (unsigned)(v<0?0:v>SV_NORM?SV_NORM:v);
}
#endif
