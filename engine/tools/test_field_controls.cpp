#include "SamplerApp.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(){
    Session s;int tiles=0,palettes=0;bool palette=false,checkField=false;
    SamplerApp app(s,[&](const uint8_t* m,size_t n){
        assert(n<=300);if(n<8)return;
        auto cmd=[&](const uint8_t* p,size_t len){
            if(!len)return;
            if(p[0]==2)palette=false;
            if(p[0]==8){++palettes;palette=true;}
            if(p[0]==9&&checkField&&len>=9){
                assert(palette);auto num=[](const uint8_t* q){return q[0]*128+q[1];};
                int x=num(p+1),y=num(p+3),w=num(p+5),h=num(p+7),count=0;
                assert(x>=12&&x+w<=228&&y>=44&&y+h<=128&&w*h<=768);
                for(size_t i=9;i<len;++i){int run=(p[i]&7)+1;assert((p[i]>>3)<16);if(run==8){assert(i+1<len);run+=p[++i];}count+=run;}
                assert(count==w*h);++tiles;
            }
        };
        if(m[6]!=10){cmd(m+6,n-7);return;}
        for(size_t i=7;i+3<n;){size_t len=m[i]*128+m[i+1];assert(len&&i+2+len<n);cmd(m+i+2,len);i+=2+len;}
    });
    uint32_t now=0;
    auto run=[&](int ms){for(int i=0;i<ms;i+=16){now+=16;app.tick(now);}};
    auto button=[&](int id,bool down){uint8_t m[]={uint8_t(down?0x91:0x81),uint8_t(id),uint8_t(down?100:0)};app.onMidi(m,3,now);};
    auto hold=[&](int id){button(id,true);run(800);button(id,false);run(32);};
    auto enc=[&](int id,int delta){uint8_t m[]={0xb0,uint8_t(16+id),uint8_t(64+delta)};app.onMidi(m,3,now);};
    using A=SamplerApp;
    assert(!app.editor()&&!app.fieldMode()&&!app.generativeDeck());
    hold(A::HOME);assert(app.editor()&&!app.fieldMode());
    button(A::HOME,true);run(800);assert(app.fieldMode()&&app.editor());run(800);assert(app.fieldMode());button(A::HOME,false);
    assert(app.takeFieldEvents().empty());
    const auto page=app.page();const auto track=app.track();
    enc(0,1);enc(6,-2);enc(1,3);enc(2,1);run(32);enc(2,1);run(32);enc(2,-1);
    button(A::SEL,true);button(A::SEL,false);
    assert((app.takeFieldEvents()==std::vector<int>{0,1,6,-2,1,3,302,17,302,18,302,-15,103,1}));
    assert(app.track()==track&&app.page()==page);
    button(A::HOME,true);run(100);button(A::HOME,false);
    assert((app.takeFieldEvents()==std::vector<int>{108,1}));
    button(A::REC,true);run(100);button(A::REC,false);
    assert((app.takeFieldEvents()==std::vector<int>{113,1}));
    checkField=true;std::string pixels;for(int i=0;i<36*21;++i)pixels+="0123456789abcdef"[i%16];
    app.fieldFeedback("OUTPUT|FFT 2048|SAMPLE|K1 START|K2 END|K3 POSITION|K4 PITCH",0,pixels);
    run(5000);assert(tiles>=28&&palettes>0);
    hold(A::GLO);run(4000);assert(app.takeFieldEvents().size()==2&&tiles>=56);
    checkField=false;hold(A::REC);assert(!app.fieldMode()&&!app.editor()&&!app.generativeDeck());
    hold(A::REC);assert(app.generativeDeck()&&!app.editor());
    hold(A::HOME);assert(app.fieldMode()&&app.editor()&&app.generativeDeck());
    hold(A::HOME);assert(!app.fieldMode()&&app.editor()&&app.generativeDeck());
    hold(A::REC);assert(!app.editor()&&app.generativeDeck());
    hold(A::HOME);assert(app.editor()&&!app.fieldMode()&&app.generativeDeck());
    puts("field controls OK");
}
