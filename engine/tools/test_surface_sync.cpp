#include "SamplerApp.h"
#include "Theme.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static int n14(const uint8_t* p){return p[0]*128+p[1];}
static uint16_t color(const uint8_t* p){return uint16_t(p[0]*16384+p[1]*128+p[2]);}

int main(){
    try {
        using P=SamplerApp;
        Session session;
        int clears=0,waveformRects=0,blits=0;
        std::array<uint16_t,P::kPrimaryMenus> main{},sub{};
        std::array<uint16_t,16> palette{};
        std::array<uint16_t,216*46> plot{};
        uint16_t background=0;
        bool palReady=false;
        SamplerApp app(session,[&](const uint8_t* m,size_t n){
            require(n<=300,"firmware frame limit exceeded");
            if(n<8)return;
            auto cmd=[&](const uint8_t* p,size_t len){
                if(!len)return;
                if(p[0]==2){++clears;waveformRects=0;palReady=false;background=color(p+1);main.fill(background);sub.fill(background);plot.fill(background);}
                if(p[0]==8&&len>=49){for(int i=0;i<16;++i)palette[i]=color(p+1+3*i);palReady=true;}
                if(p[0]==3&&len>=12){
                    const int x=n14(p+1),y=n14(p+3),w=n14(p+5),h=n14(p+7);
                    require(x+w<=240&&y+h<=240,"rectangle exceeds screen");
                    if((y==44||y==36)&&x>=P::kMenuX&&x<P::kMenuX+P::kPrimaryMenus*P::kMenuStep&&(x-P::kMenuX)%P::kMenuStep==0){
                        auto& row=y==44?main:sub;
                        row[(x-P::kMenuX)/P::kMenuStep]=color(p+9);
                    }
                    if(y==54)++waveformRects;
                    for(int yy=std::max(54,y);yy<std::min(100,y+h);++yy)
                        for(int xx=std::max(12,x);xx<std::min(228,x+w);++xx)plot[(yy-54)*216+xx-12]=color(p+9);
                }
                if(p[0]==9&&len>=9){
                    require(palReady,"bitmap arrived before palette");
                    const int x=n14(p+1),y=n14(p+3),w=n14(p+5),h=n14(p+7);
                    require(x>=12&&y>=54&&x+w<=228&&y+h<=100&&w*h<=768,"bitmap outside plot or firmware buffer");
                    int pos=0;
                    for(size_t i=9;i<len;++i){
                        const unsigned index=p[i]>>3;
                        int run=(p[i]&7)+1;
                        if(run==8){require(i+1<len,"truncated bitmap run");run+=p[++i];}
                        require(pos+run<=w*h,"bitmap run overflow");
                        while(run--){plot[(y-54+pos/w)*216+x-12+pos%w]=palette[index];++pos;}
                    }
                    require(pos==w*h,"incomplete bitmap");
                    ++blits;
                }
            };
            if(m[6]!=10){cmd(m+6,n-7);return;}
            for(size_t i=7;i+3<n;){
                size_t len=n14(m+i);
                require(len>=1&&i+2+len<n,"invalid batch");
                cmd(m+i+2,len);i+=2+len;
            }
        });
        uint32_t now=0;
        auto run=[&](uint32_t ms){const uint32_t end=now+ms;while(now<end){now+=16;app.tick(now);}};
        auto pong=[&](uint32_t drops){
            uint8_t m[30]={0xf0,0x7d,'D','A','W',1,0x40};
            for(int i=28;i>=24;--i){m[i]=drops&127;drops>>=7;}m[29]=0xf7;app.onSysex(m,sizeof m);
        };
        auto checkMenu=[&](){
            const auto parent=P::menuParent(app.page());
            int anchor=0;
            for(int i=0;i<P::kPrimaryMenus;++i)if(P::primaryMenuPage(i)==parent)anchor=i;
            const uint16_t inactive=main[(anchor+1)%P::kPrimaryMenus];
            for(int i=0;i<P::kPrimaryMenus;++i)require((main[i]!=inactive)==(i==anchor),"primary marker did not follow group");
            std::array<P::Page,P::kPrimaryMenus> children{};children.fill(P::kPages);
            int count=0;
            for(int i=0;i<P::kPages;++i){
                auto p=P::menuPage(i);if(P::menuParent(p)!=parent||!app.pageAvailable(p))continue;
                children[anchor+(parent==P::P_LEN?-count:count)]=p;++count;
            }
            for(int i=0;i<P::kPrimaryMenus;++i){
                if(count<=1||children[i]==P::kPages)require(sub[i]==background,"stale child marker");
                else if(children[i]==app.page())require(sub[i]!=background&&sub[i]!=inactive,"active child marker missing");
                else require(sub[i]==inactive,"inactive child marker mismatch");
            }
        };
        run(3000);checkMenu();
        app.setAlgo(4);run(3000);checkMenu();
        const int normalClears=clears;
        for(int page=0;page<P::kPages;++page){
            if(!app.pageAvailable(static_cast<P::Page>(page)))continue;
            app.gotoPage(static_cast<P::Page>(page));const int old=blits;run(3000);
            require(clears==normalClears,"menu navigation cleared the full display");
            checkMenu();
            const auto parent=P::menuParent(app.page());
            if(app.page()!=P::P_HOME)require(blits>old,"page did not send pictographic tiles");
        }
        app.gotoPage(P::P_EFILTER);run(3000);
        const int settled=blits;run(2000);
        require(blits==settled,"static reduced glyph retransmitted");
        // Exercise each of the new FX parameters at the actual
        // firmware palette resolution, including bypassed inserts.
        const P::Page effects[]={P::P_ECHO,P::P_GRAIN,P::P_MOTION,P::P_EFILTER,P::P_EDELAY,P::P_EREVERB,P::P_ETAPE,P::P_EMIX,P::P_KEY};
        for(auto page:effects){
            app.gotoPage(page);run(3000);
            for(int j=0;j<4;++j){
                const auto k=app.knobs()[j];
                const float saved=k.norm;
                k.set(0);run(2000);
                const auto low=plot;
                app.knobs()[j].set(1);run(2000);
                bool meterChanged=false;
                for(int y=39;y<44;++y)for(int x=j*54;x<j*54+50;++x)
                    meterChanged|=plot[y*216+x]!=low[y*216+x];
                if(!meterChanged)throw std::runtime_error(std::string(P::pageName(page))+" / "+k.name+" does not change its glyph meter");
                app.knobs()[j].set(saved);run(1000);
            }
        }
        // Verify the actual chord notes, not just a generic mode bar.
        app.setScale(2,0,0,1);
        require(app.chordGhostMask()==((1u<<0)|(1u<<4)|(1u<<7)),"major tonic triad preview");
        app.keyEvent(62,true);
        require(app.chordGhostMask()==((1u<<2)|(1u<<5)|(1u<<9)),"held diatonic triad preview");
        app.keyEvent(62,false);
        app.setScale(3,0,0,1);
        require(app.chordGhostMask()==((1u<<0)|(1u<<3)|(1u<<7)),"minor tonic triad preview");
        app.setScale(3,0,0,0);
        require(app.chordGhostMask()==0,"key mode should have no chord ghosts");
        // Rapid changes must converge to the final value without
        // replaying intermediate full-screen images.
        app.gotoPage(P::P_EDELAY);run(2000);
        for(int i=0;i<25;++i){app.knobs()[0].set(i/24.f);run(16);}
        run(1500);
        const uint16_t first=plot[39*216];
        require(first==plot[39*216+48]&&first!=plot[39*216+49],"fast turn did not settle to full meter");
        app.gotoPage(P::P_KEY);run(48);
        app.gotoPage(P::P_MIX);run(48);
        app.gotoPage(P::P_ECHO);run(3000);
        require(clears==normalClears,"rapid navigation wiped display");checkMenu();
        app.setAlgo(0);app.gotoPage(P::P_HOME);run(3000);checkMenu();
        int before=clears;app.resetSurface();run(1500);
        require(clears>before&&waveformRects>=54,"reconnect did not redraw waveform");
        uint8_t reconnect[]={0xf0,0x7d,'D','A','W',1,0x43,0xf7};
        before=clears;app.onSysex(reconnect,sizeof reconnect);run(3000);
        require(clears>before&&waveformRects>=54,"firmware reconnect event did not redraw");
        app.gotoPage(P::P_EREVERB);run(3000);pong(0);run(5000);
        before=clears;const auto beforeBlits=blits;pong(1);run(5000);
        require(clears>before&&blits>beforeBlits,"dropped frame did not restore picture");
        checkMenu();
        std::puts("surface synchronization tests passed: tiers, tiles, frame bounds, reconnect");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
