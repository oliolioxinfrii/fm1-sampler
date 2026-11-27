#include "SamplerApp.h"
#include <cassert>
#include <cstdio>
int main(){
    Session s;SamplerApp app(s,[](const uint8_t*,size_t){});
    uint32_t now=0;
    auto run=[&](int ms){for(int i=0;i<ms;i+=16){now+=16;app.tick(now);}};
    auto button=[&](int id,bool down){uint8_t m[]={uint8_t(down?0x91:0x81),uint8_t(id),uint8_t(down?100:0)};app.onMidi(m,3,now);};
    auto enc=[&](int id,int delta){uint8_t m[]={0xb0,uint8_t(16+id),uint8_t(64+delta)};app.onMidi(m,3,now);};
    app.setEditor(true);const auto page=app.page();const auto track=app.track();
    button(SamplerApp::HOME,true);run(704);assert(app.fieldMode()&&app.editor());run(704);assert(app.fieldMode());button(SamplerApp::HOME,false);
    assert(app.takeFieldEvents().empty());
    enc(0,1);enc(6,-2);enc(1,3);enc(2,1);button(SamplerApp::SEL,true);button(SamplerApp::SEL,false);
    assert((app.takeFieldEvents()==std::vector<int>{0,1,6,-2,1,3,2,1,103,1}));
    assert(app.track()==track&&app.page()==page);
    button(SamplerApp::HOME,true);run(100);button(SamplerApp::HOME,false);
    assert((app.takeFieldEvents()==std::vector<int>{108,1}));
    button(SamplerApp::HOME,true);run(704);assert(!app.fieldMode()&&app.editor());button(SamplerApp::HOME,false);
    assert(app.takeFieldEvents().empty()&&app.page()==page);
    puts("field controls OK");
}
