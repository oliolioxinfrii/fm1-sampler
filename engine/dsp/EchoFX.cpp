// SPDX-License-Identifier: MIT
// Adapted from ECHOFIELD FM-1, Copyright (c) 2026 Philip Bos.
#include "EchoFX.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float pi = 3.14159265358979f, tau = 2 * pi;
float unit(float x) { return std::clamp(x, 0.f, 1.f); }
float pole(float hz, int sr) { return 1.f - std::exp(-tau * std::min(hz, .45f * sr) / sr); }
float mix(float a, float b, float k) { return a + (b-a)*k; }
float wrap(float p) { return p - std::floor(p); }
float flush(float x) { return std::abs(x) < 1e-20f ? 0.f : x; }
float read(const std::vector<float>& b, int w, float delay) {
    const int len = int(b.size());
    float p = w - std::clamp(delay, 1.f, float(len-2));
    if (p < 0) p += len;
    int i = int(p);
    return mix(b[i], b[(i+1)%len], p-i);
}
}

EchoInsert::EchoInsert(int sr) : sr_(sr) {
    for (auto& b : tape_) b.assign(int(.016f*sr)+4, 0.f);
}
float EchoInsert::rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return float(rng_ >> 8) * (2.f / 16777216.f) - 1.f;
}
void EchoInsert::process(float* x, int n) {
    const auto& p = target_;
    auto& s = smooth_;
    const float a = 1.f-std::exp(-float(n)/(.025f*sr_));
    auto sm = [&](float& v, float t){ v=mix(v,unit(t),a); };
    sm(s.tape,p.tape); sm(s.wow,p.wow); sm(s.flutter,p.flutter); sm(s.age,p.age); sm(s.hiss,p.hiss);
    sm(s.cutoff,p.cutoff); sm(s.resonance,p.resonance); sm(s.drive,p.drive); sm(s.filterMix,p.filterMix);
    sm(s.width,p.width); sm(s.level,p.level); s.filter=std::clamp(p.filter,0,2);
    const float width=.4f+.8f*s.width, gain=1.6f*s.level*s.level;
    const bool tape=s.tape>1e-5f, filter=s.filterMix>1e-5f;
    const float tw=std::min(1.f,s.tape*4), sat=1+3*s.tape, satNorm=1/std::tanh(sat);
    const float toneA=pole(16000*(1-.8f*s.tape*(.3f+.7f*s.age)),sr_);
    const float g=std::tan(pi*std::min(30.f*std::pow(600.f,s.cutoff),.45f*sr_)/sr_);
    const float k=2-1.94f*s.resonance, a1=1/(1+g*(g+k));
    const float drive=1+6*s.drive, driveNorm=1/std::sqrt(drive);
    for (int i=0;i<n;++i) {
        float l=x[2*i], r=x[2*i+1], v[2]={l,r};
        // Keep history current even when bypassed, so engaging tape has
        // no uninitialized delay line. Noise and wear scale with amount.
        for (int c=0;c<2;++c) tape_[c][wr_]=v[c];
        if (tape) {
            wowPh_+=(.55f+.1f*wowNoise_)/sr_;
            if(wowPh_>=1){wowPh_-=1;wowNoise_=rnd();}
            flutPh_=wrap(flutPh_+7.3f/sr_);
            if ((i&31)==0) flutNoise_=mix(flutNoise_,rnd(),.3f);
            const float d=sr_*(.006f+s.tape*s.wow*.0025f*(1+std::sin(tau*wowPh_))+
                s.tape*s.flutter*.00015f*(1+.6f*std::sin(tau*flutPh_)+.4f*flutNoise_));
            ageTimer_-=1.f/sr_;
            if(ageTimer_<=0){ageTimer_=.05f;if((rnd()+1)*.5f<s.tape*s.age*.012f)ageGain_=.35f;}
            ageGain_=mix(ageGain_,1.f,.0006f*44100/sr_);
            for(int c=0;c<2;++c){
                const float z=std::tanh(read(tape_[c],wr_,d)*sat)*satNorm;
                tone_[c]=flush(mix(tone_[c],z,toneA));
                hiss_[c]=mix(hiss_[c],rnd(),.3f);
                v[c]=mix(v[c],tone_[c]*ageGain_,tw)+hiss_[c]*s.hiss*s.tape*.012f;
            }
        }
        if(++wr_>=int(tape_[0].size()))wr_=0;
        if(filter) for(int c=0;c<2;++c){
            const float v3=v[c]-sv2_[c], v1=a1*(sv1_[c]+g*v3), v2=sv2_[c]+g*v1;
            sv1_[c]=flush(2*v1-sv1_[c]);sv2_[c]=flush(2*v2-sv2_[c]);
            const float y=s.filter==0?v2:s.filter==1?v1*k:v[c]-k*v1-v2;
            v[c]=mix(v[c],std::tanh(y*drive)*driveNorm,s.filterMix);
        }
        if(!filter && !tape && std::abs(width-1)<1e-6f && std::abs(gain-1)<1e-6f) continue;
        const float mid=(v[0]+v[1])*.5f, side=(v[0]-v[1])*.5f*width;
        x[2*i]=(mid+side)*gain; x[2*i+1]=(mid-side)*gain;
    }
}
float EchoReturns::Ring::read(float delay) const { return ::read(data,w,delay); }
void EchoReturns::Ring::push(float value) { data[w]=flush(value); if(++w>=int(data.size()))w=0; }
EchoReturns::EchoReturns(int sr):sr_(sr) {
    const int ap[]={142,107,379,277}, fdn[]={1499,1889,2381,2803,3191,3607,4027,4513};
    for(auto& d:delay_)d.data.assign(sr+8,0.f);
    for(int i=0;i<4;++i)ap_[i].data.assign(std::max(4,int(ap[i]*sr/48000.f)+1),0.f);
    for(int i=0;i<8;++i){length_[i]=std::max(4,int(fdn[i]*sr/48000.f));fdn_[i].data.assign(length_[i]+int(.002f*sr)+8,0.f);}
    shift_.data.assign(int(.11f*sr)+8,0.f);
}
void EchoReturns::process(const float* echo,const float* room,float* out,int n) {
    auto& s=smooth_; const auto& p=target_;
    const float a=1-std::exp(-float(n)/(.04f*sr_));
    s.feedback=mix(s.feedback,std::clamp(p.feedback,0.f,.92f),a);
    s.size=mix(s.size,unit(p.size),a);s.damp=mix(s.damp,unit(p.damp),a);
    s.shimmer=mix(s.shimmer,unit(p.shimmer),a);s.ping=p.ping;
    const float fb=s.feedback*.98f, dlpa=pole(5000,sr_);
    const float t60=.6f*std::pow(24.f,s.size), da=pole(std::min(12000.f*std::pow(.12f,s.damp),10000.f),sr_);
    float g[8];for(int j=0;j<8;++j)g[j]=std::pow(10.f,-3.f*length_[j]/(t60*sr_));
    const float win=.04644f*sr_, dc=std::exp(-tau*20/sr_), timeAlpha=1-std::exp(-1.f/(.057f*sr_));
    for(int i=0;i<n;++i){
        s.time=mix(s.time,std::clamp(p.time,.02f,1.f),timeAlpha);
        const float el=delay_[0].read(s.time*sr_), er=delay_[1].read(s.time*sr_);
        dlp_[0]=flush(mix(dlp_[0],el,dlpa));dlp_[1]=flush(mix(dlp_[1],er,dlpa));
        const float fl=std::tanh(dlp_[0]*fb),fr=std::tanh(dlp_[1]*fb);
        const float il=echo[2*i],ir=echo[2*i+1];
        delay_[0].push(s.ping?(il+ir)*.5f+fr:il+fl);
        delay_[1].push(s.ping?fl:ir+fr);
        float rin=(room[2*i]+room[2*i+1])*.5f+shimFB_*s.shimmer*.16f;
        for(auto& ap:ap_){float d=ap.data[ap.w],v=rin+.6f*d;ap.push(v);rin=d-.6f*v;}
        phase1_=wrap(phase1_+.31f/sr_);phase2_=wrap(phase2_+.47f/sr_);
        float y[8],sum=0;
        for(int j=0;j<8;++j){
            const float mod=j==1?sr_*.000272f*(1+std::sin(tau*phase1_)):j==5?sr_*.000272f*(1+std::sin(tau*phase2_)):0;
            damp_[j]=flush(mix(damp_[j],fdn_[j].read(length_[j]+mod),da));
            y[j]=damp_[j]*g[j];sum+=y[j];
        }
        for(int j=0;j<8;++j){const float sign=((j&1)==0)==((j&2)==0)?-1.f:1.f;fdn_[j].push(y[j]-sum*.25f+sign*rin*.5f);}
        const float wl=(y[2]-y[0]+y[6]-y[4])*.6f,wr=(y[1]-y[3]+y[5]-y[7])*.6f;
        shift_.push((wl+wr)*.25f);
        shiftPhase_=wrap(shiftPhase_+1/win);
        float shifted=0;
        for(int j=0;j<2;++j){float ph=wrap(shiftPhase_+j*.5f);shifted+=shift_.read((1-ph)*win+3)*(1-std::abs(2*ph-1));}
        shimLP_=flush(mix(shimLP_,shifted,.4f));
        shimY_=flush(shimLP_-shimX_+dc*shimY_);shimX_=shimLP_;
        shimFB_=std::tanh(shimY_*.9f);
        out[2*i]+=el+wl;out[2*i+1]+=er+wr;
    }
}
