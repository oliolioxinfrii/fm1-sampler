// Revision X: a common visual grammar for the phone and the FM-1's 216x35 plot.
// The compact renderer uses the same geometry, with fewer strokes and no labels.
// All illustrative trajectories are parameter diagrams, not fabricated audio telemetry.
bool SamplerUI::studyGraphic(Rect w, SamplerApp::Page pg, bool compact) {
    using P=SamplerApp;
    const int t=app_.track();
    const auto& ts=app_.state(t);
    const auto k=app_.knobs(pg);
    auto n=[&](int i){return clamp01(k[i].norm);};
    auto X=[&](float x){return w.x+w.w*x/96.f;};
    auto Y=[&](float y){return w.y+w.h*y/48.f;};
    const float scale=std::min(w.w/96.f,w.h/48.f);
    auto sw=[&](float v){return std::max(compact?2.2f:1.f,v*scale);};
    auto L=[&](float x,float y,float xx,float yy,uint32_t c,float thick=0.55f){line(X(x),Y(y),X(xx),Y(yy),c,sw(thick));};
    auto B=[&](float x,float y,float ww,float hh,uint32_t c){rrect({X(x),Y(y),w.w*ww/96.f,w.h*hh/48.f},0,c);};
    auto O=[&](float x,float y,float ww,float hh,uint32_t c){stroke({X(x),Y(y),w.w*ww/96.f,w.h*hh/48.f},0,c,sw(.6f));};
    auto D=[&](float x,float y,float rr,uint32_t c){circle(X(x),Y(y),std::max(compact?2.f:1.f,rr*scale),c);};
    auto label=[&](float x,float y,const std::string& txt,uint32_t c=WELL_DIM){if(!compact)text(X(x),Y(y),std::min(11.f,std::max(7.f,scale*2.7f)),c,txt,1,true);};
    auto tint=[](uint32_t a,uint32_t b,float z){
        z=clamp01(z);uint32_t out=0xff000000u;
        for(int sh=0;sh<=16;sh+=8)out|=uint32_t(std::lround(float((a>>sh)&255)*(1-z)+float((b>>sh)&255)*z))<<sh;
        return out;
    };
    auto bar=[&](float x,float y,float ww,float hh,float v,uint32_t c){
        B(x,y,ww,hh,GRID); B(x,y,ww*clamp01(v),hh,c); O(x,y,ww,hh,WELL_DIM);
    };
    auto arrow=[&](float x,float y,float xx,float yy,uint32_t c){
        L(x,y,xx,yy,c);const float dx=xx-x,dy=yy-y,len=std::hypot(dx,dy);
        if(len>.01f){const float ux=dx/len,uy=dy/len;
            L(xx,yy,xx-2.5f*ux+1.3f*uy,yy-2.5f*uy-1.3f*ux,c);
            L(xx,yy,xx-2.5f*ux-1.3f*uy,yy-2.5f*uy+1.3f*ux,c);}
    };
    auto jack=[&](float x,float y,uint32_t c){D(x,y,1.9f,c);D(x,y,.8f,WELL);};
    auto base=[&](){B(0,0,96,48,WELL);};
    auto source=[&](float x,float y,float ww,float hh){
        const int bins=compact?40:80;const auto p=app_.peaks(t,bins);
        L(x,y+hh*.5f,x+ww,y+hh*.5f,GRID);
        if(p.empty()){label(x+ww*.5f,y+hh*.5f,"NO SAMPLE");return;}
        const float step=ww/std::max(1,(int)p.size());
        for(size_t i=0;i<p.size();++i){float v=clamp01(p[i])*hh*.47f;
            B(x+i*step,y+hh*.5f-v,step+.02f,2*v,TRACE_GREEN);}
    };
    auto pos=[&](){
        const auto home=app_.knobs(P::P_HOME);const float p=clamp01(home[3].norm);
        const float left=clamp01(home[0].norm),right=clamp01(home[1].norm);
        const float absolute=left+(right-left)*p;
        const float x=4+88*absolute;
        L(x,2,x,14,TRACE_AMBER,.75f);D(x,14,1.3f,TRACE_AMBER);
        label(76,4,"POS "+num("%.1f",p*100)+"%",TRACE_AMBER);
        if(!compact){
            Rect hr{X(4),Y(0),X(92)-X(4),Y(15)-Y(0)};
            auto set=[this,hr,left,right](float px,float){if(right>left)app_.knobs(P::P_HOME)[3].set(clamp01(((px-hr.x)/hr.w-left)/(right-left)));};
            hit(hr,set,set);
        }
    };
    switch(pg) {
    case P::P_ECHO:{
        base();source(4,2,88,12);pos();
        // Successive offset readings of one source: texture/scatter/drift/evolve.
        const auto peaks=app_.peaks(t,compact?32:64);
        const int layers=compact?4:6;
        for(int j=0;j<layers;++j){
            float y=19+j*25.f/layers,off=(j-(layers-1)*.5f)*n(2)*2.2f;
            float len=78-12*n(1)*j/layers;
            float x=9+off+4*n(1)*std::sin(j*2.1f);
            O(x,y-1.4f,len,3.8f,j%2?TRACE_BLUE:TRACE_VIOLET);
            for(size_t i=0;i<peaks.size();++i){
                float v=clamp01(peaks[(i+j*size_t(1+int(n(1)*5)))%peaks.size()]);
                float xx=x+(i+.5f)*len/peaks.size(),amp=v*(.7f+1.2f*n(0));
                L(xx,y+0.5f-amp,xx,y+0.5f+amp,tint(TRACE_BLUE,TRACE_VIOLET,clamp01(n(3)*j/layers)),.38f);
            }
        }return true;}
    case P::P_GRAIN:{
        base();source(4,2,88,12);pos();
        const int shape=int(std::lround(n(2)*5));
        auto window=[&](float x){
            x=clamp01(x);
            switch(shape){
            case 2:return x<.25f?.5f*(1-std::cos(4*kPi*x)):x>.75f?.5f*(1-std::cos(4*kPi*(1-x))):1.f;
            case 3:return x<.08f?x/.08f:std::exp(-5.f*(x-.08f))*(1-x);
            case 4:return x>.92f?(1-x)/.08f:std::exp(-5.f*(.92f-x))*x;
            case 5:return std::exp(-.5f*std::pow((x-.5f)/.18f,2.f));
            default:return .5f*(1-std::cos(2*kPi*x));
            }
        };
        L(4,42,92,42,GRID);
        const int count=3+int(n(1)*(compact?5:8));
        const float width=8+34*n(0);
        for(int j=0;j<count;++j){
            const float start=5+(86-width)*j/std::max(1,count-1);
            const uint32_t c=j%2?TRACE_VIOLET:TRACE_BLUE;
            const int steps=compact?12:24;
            for(int i=0;i<steps;++i){float x=float(i)/steps,xx=float(i+1)/steps;
                L(start+width*x,42-19*window(x),start+width*xx,42-19*window(xx),c,.65f);}
        }
        if(n(3)>.5f){L(6,45,90,45,TRACE_AMBER);label(48,46,"FREEZE",TRACE_AMBER);}
        return true;}
    case P::P_MOTION:{
        base();source(4,2,88,12);pos();
        const float scan=2*n(0)-1;
        for(int lane=0;lane<3;++lane){
            const float y=21+10.f*lane;L(4,y,92,y,GRID);
            const int count=compact?4:7;
            for(int j=0;j<count;++j){
                float x=7+j*80.f/count;
                const bool back=(float(j)+.5f)/count<n(1);
                const float dx=(back?-1.f:(scan<0?-1.f:1.f))*(5+9*std::abs(scan));
                float yy=y+std::sin(j*2.3f+lane*1.4f)*n(2)*3;
                arrow(std::clamp(x,4.f,92.f),yy,std::clamp(x+dx,4.f,92.f),yy+(lane-1)*n(2)*2,back?TRACE_VIOLET:TRACE_BLUE);
            }
        }if(n(3)>.5f){O(3,17,90,29,TRACE_AMBER);label(80,45,"LATCH",TRACE_AMBER);}return true;}
    case P::P_EDELAY:{
        base();const bool ping=n(3)>.5f;
        const int repeats=compact?5:7; const float pitch=7+5*n(0);
        float first=12,last=first;
        const float y1=11,y2=25;
        L(3,y1,93,y1,GRID);L(3,y2,93,y2,GRID);
        for(int j=0;j<repeats;++j){
            const float x=first+j*pitch;if(x+5>94)break;last=x;
            const float y=ping&&j%2?y2:y1;
            const float strength=n(2)*std::pow(std::min(.92f,n(1)),float(j));
            const uint32_t c=tint(GRID,TRACE_VIOLET,.12f+.88f*strength);
            B(x-4,y-4,8,8,c);O(x-4,y-4,8,8,tint(WELL_DIM,TRACE_VIOLET,n(2)));
            if(j>0){const float py=ping&&(j-1)%2?y2:y1;L(x-pitch+4,py,x-4,y,TRACE_BLUE);}
        }
        const float ly=ping&&int(std::lround((last-first)/pitch))%2?y2:y1;
        const uint32_t fb=tint(GRID,TRACE_GREEN,n(1));
        L(first,y1+4,first,39,fb,.85f);L(last,ly+4,last,39,fb,.85f);
        arrow(last,39,first,39,fb);label(48,44,"FEEDBACK",TRACE_GREEN);
        label(5,7,"L");label(5,21,"R");return true;}
    case P::P_EREVERB:{
        base();const int nx=compact?10:18,ny=compact?6:10,steps=compact?16:32;
        const float size=n(1),send=n(0);
        auto warp=[&](float x,float y){float taper=std::sin(kPi*x/96)*std::sin(kPi*y/48);
            return std::pair<float,float>{x+size*6*taper*std::sin(y*.22f+x*.10f),y+size*5*taper*std::cos(x*.15f-y*.12f)};};
        for(int family=0;family<2;++family){
            const int count=family?ny:nx;
            for(int j=0;j<=count;++j){
                auto point=[&](float q){return warp(family?4+88*q:4+88.f*j/count,family?4+40.f*j/count:4+40*q);};
                uint32_t c=tint(GRID,TRACE_BLUE,.25f+.75f*send);
                if(j%3==1&&float(j)/count<n(2))c=tint(c,TRACE_VIOLET,.85f);
                if(j%5==2&&float(j)/count<n(3))c=tint(c,TRACE_AMBER,.85f);
                for(int i=0;i<steps;++i){auto p=point(float(i)/steps),q=point(float(i+1)/steps);L(p.first,p.second,q.first,q.second,c,.5f);}
            }
        }return true;}
    case P::P_ETAPE:{
        base();const int steps=compact?40:80;
        auto center=[&](float x){return 24+3.8f*n(0)*std::sin(x*.085f)+1.5f*n(1)*std::sin(x*.62f);};
        for(int i=0;i<steps;++i){float x=3+i*90.f/steps,xx=3+(i+1)*90.f/steps;
            const float y=center(x),yy=center(xx);
            L(x,y+3,xx,yy+3,TRACE_BLUE,2.0f);
            L(x,y,xx,yy,tint(TRACE_VIOLET,WELL_DIM,n(2)*.6f),2.2f);
            if((i*17%steps)/float(steps)>n(2))L(x,y-2,xx,yy-2,TRACE_AMBER,.8f);
        }
        for(int i=0;i<(compact?24:50);++i){const float q=(i*37%101)/101.f;if(q>n(3))continue;
            const float x=5+(i*29%89),y=center(x)-5-(i*13%7);D(x,y,.46f,WELL_DIM);}
        B(42,4,12,15,WELL_DIM);B(45,7,6,12,GRID);L(42,19,54,19,TRACE_AMBER,1);
        label(48,3,"TAPE HEAD",TRACE_AMBER);return true;}
    case P::P_EMIX:{
        base();const float tape=n(2),filter=n(3);
        for(float y:{19.f,29.f}){jack(3,y,TRACE_BLUE);L(5,y,12,y,TRACE_BLUE);}
        O(12,9,17,30,TRACE_VIOLET);O(34,9,17,30,TRACE_AMBER);
        for(float y:{19.f,29.f}){L(29,y,34,y,TRACE_BLUE);L(51,y,57,y,TRACE_BLUE);}
        label(20.5f,14,"TAPE",TRACE_VIOLET);label(42.5f,14,"FILTER",TRACE_AMBER);
        bar(14,31,13,4,tape,TRACE_VIOLET);bar(36,31,13,4,filter,TRACE_AMBER);
        O(57,3,33,42,WELL_DIM);label(73.5f,6,"LIVE M/S");
        // Equal physical scales for side and mid, independent of panel aspect.
        // Capture is already AFTER width and output gain in EchoInsert.
        const float cx=X(73.5f),cy=Y(24),rad=std::min((X(88)-X(59))*.5f,(Y(38)-Y(10))*.5f);
        stroke({cx-rad,cy-rad,2*rad,2*rad},0,GRID,sw(.45f));
        line(cx-rad,cy,cx+rad,cy,GRID,sw(.35f));line(cx,cy-rad,cx,cy+rad,GRID,sw(.35f));
        float mid[Session::kScopePoints],side[Session::kScopePoints];
        const int count=app_.session().insertScope(t,mid,side,Session::kScopePoints);
        for(int i=1;i<count;++i){
            // Intermittent colors encode configured mix, not measured effect contributions.
            uint32_t c=tint(TRACE_BLUE,TRACE_VIOLET,tape*(.25f+.75f*std::pow(std::sin(i*1.77f),2.f)));
            c=tint(c,TRACE_AMBER,filter*(.15f+.85f*std::pow(std::cos(i*1.19f),2.f)));
            line(cx+rad*std::clamp(side[i-1],-1.f,1.f),cy-rad*std::clamp(mid[i-1],-1.f,1.f),cx+rad*std::clamp(side[i],-1.f,1.f),cy-rad*std::clamp(mid[i],-1.f,1.f),c,sw(.55f));
        }
        bar(60,40,27,3,clamp01(n(1)),TRACE_GREEN);
        for(float y:{19.f,29.f}){L(90,y,93,y,TRACE_BLUE);jack(94,y,TRACE_BLUE);}
        return true;}
    case P::P_MIX:{
        base();const int src=ts.scSrc-1;const bool on=src>=0&&src<Session::kTracks&&src!=t;
        for(int j=0;j<8;++j){float y=5+j*5.2f;jack(5,y,on&&src==j?TRACE_VIOLET:WELL_DIM);if(!compact)label(10,y,std::to_string(j+1));}
        O(23,5,17,13,on?TRACE_VIOLET:GRID);label(31.5f,11,"DETECT",on?TRACE_VIOLET:WELL_DIM);
        if(on){float sy=5+src*5.2f;L(7,sy,17,sy,TRACE_VIOLET);L(17,sy,17,11,TRACE_VIOLET);arrow(17,11,23,11,TRACE_VIOLET);}
        O(46,5,33,38,WELL_DIM);label(62.5f,8,"GAIN CELL");
        const float th=-40*(1-n(2)),ratio=1+19*n(3)*n(3);
        auto transfer=[&](float x){float db=-60+60*x;return (db>th&&on?th+(db-th)/ratio:db);};
        const float x0=50,y0=35,ww=25,hh=21;
        L(x0,y0,x0+ww,y0-hh,GRID);
        const float threshold=clamp01((th+60)/60);
        L(x0+ww*threshold,y0,x0+ww*threshold,y0-hh,TRACE_AMBER);
        for(int j=0;j<24;++j){float p=j/24.f,q=(j+1)/24.f;L(x0+ww*p,y0-hh*(transfer(p)+60)/60,x0+ww*q,y0-hh*(transfer(q)+60)/60,on?TRACE_VIOLET:TRACE_BLUE,.8f);}
        if(on){L(40,11,43,11,TRACE_VIOLET);arrow(43,11,46,11,TRACE_VIOLET);}
        for(float y:{29.f,36.f}){jack(22,y,TRACE_BLUE);L(24,y,46,y,TRACE_BLUE);L(79,y,91,y,TRACE_BLUE);jack(93,y,TRACE_BLUE);}
        bar(51,38,23,3,n(0),TRACE_GREEN);label(32,44,on?"SIDECHAIN":"SC OFF",on?TRACE_VIOLET:WELL_DIM);return true;}
    case P::P_ARP:{
        base();const int levels=3*(ts.arpOct+1),count=6+int(n(0)*10);
        for(int i=0;i<=4;++i)L(4,8+8*i,92,8+8*i,GRID);
        for(int j=0;j<count;++j){
            int p=j%std::max(1,levels);
            if(ts.arpPatt==1)p=levels-1-p;
            else if(ts.arpPatt==2){int period=std::max(1,2*levels-2);int step=j%period;p=step<levels?step:period-step;}
            else if(ts.arpPatt==3)p=(j%2)?levels-1-(j/2)%levels:(j/2)%levels;
            else if(ts.arpPatt==4)p=(j*7+3)%levels;
            float x=4+88.f*j/count,y=40-30.f*p/std::max(1,levels-1);
            const uint32_t c=tint(TRACE_BLUE,TRACE_VIOLET,float(p)/std::max(1,levels-1));
            B(x,y-3,88.f/count*(.24f+.70f*n(3)),5,ts.arpRate?c:GRID);
        }return true;}
    case P::P_LEN:{
        base();float pk[Session::kLoopBins];const int m=app_.session().loopPeaks(t,pk,Session::kLoopBins);
        const bool active=st_.loopSamples>0;
        const int bars=std::clamp(st_.loopBars,1,32);
        for(int j=0;j<bars;++j){float x=4+j*88.f/bars;O(x,10,88.f/bars-.25f,28,GRID);}
        if(active&&st_.track_has_clip[t]&&m>0){float mx=.08f;for(int i=0;i<m;++i)mx=std::max(mx,pk[i]);
            const int count=compact?80:160;for(int j=0;j<count;++j){float v=clamp01(pk[j*m/count]/mx)*12;B(4+j*88.f/count,24-v,88.f/count+.01f,2*v,TRACE_BLUE);}}
        if(active)L(4+88*clamp01(loopPos()),8,4+88*clamp01(loopPos()),40,TRACE_AMBER);
        else label(48,24,"NO LOOP");
        L(4,42,92,42,WELL_DIM);L(4,39,4,45,WELL_DIM);L(92,39,92,45,WELL_DIM);
        label(48,46,std::to_string(bars)+" BARS");return true;}
    case P::P_TEMPO:{
        base();const bool active=st_.loopSamples>0;
        const int bars=std::clamp(st_.loopBars,1,16),beats=bars*4;
        L(4,26,92,26,WELL_DIM);
        for(int j=0;j<=beats;++j){float x=4+j*88.f/beats;
            L(x,26,x,j%4==0?10.f:18.f,j%4==0?TRACE_BLUE:GRID,j%4==0?.8f:.5f);}
        if(active)L(4+88*clamp01(loopPos()),7,4+88*clamp01(loopPos()),38,TRACE_AMBER,.8f);
        bar(4,40,88,4,n(2),TRACE_VIOLET);label(48,4,active?num("%.1f",st_.bpm)+" BPM":"NO LOOP");
        return true;}
    default:return false; // existing live sample, filter, ADSR, LFO, key and input plots
    }
}
