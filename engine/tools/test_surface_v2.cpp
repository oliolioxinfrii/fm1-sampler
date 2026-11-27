#include "SamplerApp.h"
#include "../../shared/surface_v2.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int main(){
    Session s;
    std::vector<std::vector<uint8_t>> frames;
    SamplerApp app(s,[&](const uint8_t* p,size_t n){
        assert(n<=300);
        frames.emplace_back(p,p+n);
    });
    uint32_t now=0;
    auto run=[&](int ms){for(int t=0;t<ms;t+=16){now+=16;app.tick(now);}};
    auto send=[&](uint8_t cmd,std::vector<uint8_t> p){
        std::vector<uint8_t> f={0xf0,0x7d,'D','A','W',1,cmd};
        f.insert(f.end(),p.begin(),p.end());f.push_back(0xf7);
        app.onSysex(f.data(),f.size());
    };
    auto state=[&](){
        for(auto i=frames.rbegin();i!=frames.rend();++i)
            if(i->size()==SV_STATE_BYTES+8&&(*i)[6]==SV_STATE&&(*i)[7]==1)
                return std::vector<uint8_t>(i->begin()+7,i->end()-1);
        assert(false&&"missing active state");return std::vector<uint8_t>{};
    };
    auto edit=[&](unsigned ctx,unsigned seq,int slot,int delta){
        std::vector<uint8_t> p(7);
        sv_write14(p.data(),ctx);sv_write14(p.data()+2,seq);
        p[4]=slot;p[5]=delta+63;send(SV_EDIT,p);
    };
    using A=SamplerApp;
    app.gotoPage(A::P_ENV);
    // Emulate the installed v2 firmware: it replies only to a standalone
    // PING, never to v1 BATCH or graphical commands.
    run(1200);
    auto pingCount=[&](){size_t count=0;for(const auto& frame:frames)
        if(frame.size()==10&&frame[6]==5&&frame.back()==0xf7)++count;
        return count;};
    assert(pingCount()>=2);
    send(0x43,{});send(SV_HELLO,{2});run(600);
    assert(pingCount()>=3);
    auto p=state();assert(p[6]==SV_TAPE&&p[8]==A::P_ENV);
    unsigned ctx=sv_read14(p.data()+1);
    const auto before=app.knobs()[0].norm;
    const auto step=app.knobs()[0].surfaceStep;
    assert(step>0);
    edit(ctx,1,0,before+2*step<=1?2:-2);run(200);
    auto after=app.knobs()[0].norm;
    assert(std::abs(after-before)>0.0001f);
    p=state();assert(sv_read14(p.data()+3)==1);
    edit(ctx,1,0,1);assert(app.knobs()[0].norm==after);
    edit((ctx+6)&16383,2,0,1);assert(app.knobs()[0].norm==after);
    run(200);assert(sv_read14(state().data()+3)==2);

    app.setFieldMode(true);app.takeFieldEvents();
    auto feedback=[&](const char* pos){
        std::string msg="OUTPUT|FFT 2048 / HANN|SAMPLE|START|END|POSITION|PITCH|0.000 s|5.000 s|1.000 s|0.0 st|COURTYARD|-1|0|1000|200|500|200|HINT|100|100|100|100|0|127|";
        msg+=pos;msg+="|5000|0|16383";
        app.fieldFeedback(msg,0,std::string(756,'c')+std::string(756,'3'));
    };
    frames.clear();feedback("25");run(1000);
    p=state();assert(p[6]==SV_FIELD&&p[17]==SV_SPECTRUM);
    unsigned gen=sv_read14(p.data()+15);ctx=sv_read14(p.data()+1);
    size_t received=0;
    for(const auto& f:frames)if(f[6]==SV_DATA){
        assert(sv_read14(f.data()+12)==760);
        received+=f.size()-17;
        for(size_t i=1;i+1<f.size();++i)assert(f[i]<128);
    }
    assert(received==760);
    // Each half must be independently bounded and validated.
    for(const auto& frame:frames)if(frame[6]==SV_DATA&&sv_read14(frame.data()+14)<756){
        const size_t off=sv_read14(frame.data()+14);
        for(size_t i=0;i<frame.size()-17&&off+i<756;++i)assert(frame[16+i]==3);
    }
    app.fieldFeedback("",0,std::string(756,'4')+std::string(755,'5')+"z");
    feedback("25");run(100);assert(sv_read14(state().data()+15)==gen);
    feedback("40");run(300);
    assert(sv_read14(state().data()+15)==gen);
    edit(ctx,3,0,2);
    assert((app.takeFieldEvents()==std::vector<int>{302,33}));
    assert(sv_after(0,16383)&&!sv_after(16383,0));
    assert(sv_predict(16000,1000,1)==16383&&sv_predict(100,1000,-1)==0);
    // GAUSS (choice 5/5) never overshoots or emits phantom increments.
    assert(sv_limit_delta(SV_NORM,3277,1,SV_CHOICE)==0);
    assert(sv_limit_delta(0,3277,-1,SV_CHOICE)==0);
    assert(sv_parameter_predict(SV_NORM,3277,-1,SV_CHOICE)==13106);
    assert(sv_parameter_predict(13106,3277,1,SV_CHOICE)==SV_NORM);
    assert(sv_limit_delta(0,3277,63,SV_CHOICE)==5);
    assert(sv_limit_delta(SV_NORM,164,10,0)==0);
    assert(sv_limit_delta(0,164,-10,0)==0);
    assert(sv_limit_delta(16000,164,10,0)==3);
    assert(sv_limit_delta(SV_NORM,164,-1,0)==-1);
    assert(sv_limit_delta(SV_NORM,0,1,SV_ACTION)==1);
    puts("surface v2 OK");
}
