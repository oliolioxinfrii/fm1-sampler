#include "SamplerApp.h"
#include "SamplerUI.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main(){
    Session s;
    SamplerApp app(s,[](const uint8_t*,size_t){});
    app.setGenerativeDeck(false);
    app.setEditor(false);
    app.gotoPage(SamplerApp::P_ENV);
    SamplerUI ui(app,false);
    auto close=[](float a,float b){return std::fabs(a-b)<.01f;};
    auto check=[&](float width){
        ui.setSize(width,450,0,0);ui.frame(100);
        const float stretch=(width-418)/(960-418);
        const float right=418+522*stretch;
        bool graph=false,loop=false,header=false,bar=false,font=false;
        for(const auto& op:ui.ops()){
            const auto* v=op.f;
            if(op.type==UiOp::RRECT){
                if(close(v[0],418)&&close(v[1],39)){graph|=close(v[0]+v[2],right)&&close(v[3],188);}
                if(close(v[0],20)&&close(v[1],337)){loop|=close(v[0]+v[2],right)&&close(v[3],101);}
                if(close(v[0],418)&&close(v[1],13)){header|=close(v[0]+v[2],right);}
                if(close(v[0],418+396.75f*stretch)&&close(v[1],305)){bar|=close(v[0]+v[2],right);}
            }
            if(op.type==UiOp::TEXT&&close(v[0],439*1.f+(439-418)*(stretch-1))&&op.text=="ENVELOPE")font|=close(v[2],12);
        }
        assert(graph&&loop&&header&&bar&&font);
        const float x=418+(2+(125.25f-4)*.75f)*stretch;
        ui.touch(0,x,316,100);ui.touch(2,x,316,100);
        assert(std::fabs(app.knobs()[0].norm-.75f)<.015f);
    };
    check(960);check(1120);
    puts("wide study layout OK");
}
