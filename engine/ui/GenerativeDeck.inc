// Field Study II: 960x450, identical section geometry to the HTML.
void SamplerUI::generativeDeck(){
    using namespace generative;
    const auto& c=app_.genConfig();const auto& p=app_.genPhrase();
    const bool running=app_.genRunning();
    const int cursor=app_.genCursor(),kind=app_.genMap();
    auto header=[&](Rect r,const std::string& a,const std::string& b){
        bevel(r,SURFACE);rrect({r.x+8,r.y+5,5,r.h-10},0,VIOLET);
        text(r.x+19,r.y+r.h*.5f,11,VIOLET,a,0,true);text(r.x+r.w-8,r.y+r.h*.5f,11,VIOLET,b,2,true);
    };
    auto cycle=[&](Rect r,const std::string& v,std::function<void()> f){button(r,v,false,std::move(f),10);};
    rrect({0,0,960,450},0,DECK_BG);
    header({14,12,254,23},"GENERATOR / 01","FIELD II");
    bevel({14,39,254,285},WELL,true);
    text(23,53,11,running?TRACE_GREEN:WELL_DIM,running?"PLAYING":"STOPPED",0,true);
    text(260,53,10,WELL_TEXT,"PASS "+std::to_string(p.pass+1)+" / "+std::to_string(p.n),2);
    rrect({22,68,238,91},0,0xff202c39);stroke({22,68,238,91},0,GRID);
    text(30,80,11,WELL_DIM,"HARMONIC REGISTER",0,true);text(252,80,10,WELL_DIM,"1 OCT",2);
    const std::string values[]={SamplerApp::noteName(c.root),scaleNames[c.scale],std::to_string(c.octave)};
    const char* labels[]={"SEL / ROOT","PRE / SCALE","ALG / OCT"};
    for(int i=0;i<3;i++){
        const float x=30+i*76;Rect box{x,106,70,23};
        text(x,97,8,WELL_DIM,labels[i]);rrect(box,0,0xff263544);stroke(box,0,WELL_DIM,.65f);
        text(x+5,117,10,WELL_TEXT,fit(values[i],55,10));text(x+65,117,9,WELL_DIM,"V",2);
        tap(box,[this,i]{genSelector_=i;});
    }
    for(int i=0;i<12;i++){
        const float x=30+i*18.6f;Rect k{x,135,16.6f,17};
        const int state=app_.genKeyState(i);
        uint32_t fill=state==3?(((now_/250)&1)?TRACE_AMBER:0xff344150):state==2?0xfff5f7f8:state==1?0xff477aa0:0xff344150;
        rrect(k,0,fill);stroke(k,0,i==c.root?TRACE_AMBER:GRID,.7f);
        text(x+8.3f,143.5f,8,state>=2?contrastInk(fill):WELL_TEXT,SamplerApp::noteName(i),1);
        tap(k,[this,i]{app_.genKey(i);});
    }
    const Event* e=cursor>=0&&cursor<p.n?&p.event[cursor]:nullptr;
    const char* readLabels[]={"NOTE","INTERVAL / HOLD","FIELD / PITCH","FIELD / TIME"};
    std::string read[]={e?std::string(SamplerApp::noteName(e->midi))+std::to_string(e->midi/12-1):"--",
        e?num("%.2f",cursor+1<p.n?p.event[cursor+1].on-e->on:p.total-e->on)+" / "+num("%.2f",e->off-e->on):"--",
        e?num("%+.3f",e->pitch):"--",e?num("%+.2f",e->startField)+" / "+num("%+.2f",e->endField):"--"};
    for(int i=0;i<4;i++){
        Rect z{22.f+(i%2)*120,164.f+(i/2)*45,116,41};rrect(z,0,WELL);stroke(z,0,GRID);
        text(z.x+6,z.y+9,9,WELL_DIM,readLabels[i]);text(z.x+6,z.y+26,13,WELL_TEXT,fit(read[i],z.w-8,13),0,true);
    }
    std::string source=app_.genSource().empty()?"SELECT SAMPLE":app_.genSource();
    if(app_.genSourceState()==1)source="LOADING "+source;else if(app_.genSourceState()==3)source="LOAD FAILED";
    cycle({22,255,25,23},"<",[this]{app_.genBrowse(-1);});
    cycle({49,255,166,23},fit(source,156,10),[this]{sheetOpen_=true;sheetScroll_=std::max(0.f,float(app_.libraryCursor())*34-100);app_.refreshLibrary();});
    cycle({217,255,43,23},"LOAD",[this]{app_.requestFile();});
    button({22,283,56,30},"PLAY",running,[this]{app_.genPlay();},11,false,GREEN);
    cycle({80,283,56,30},"STOP",[this]{app_.genStop();});
    cycle({138,283,60,30},"RESET",[this]{bool run=app_.genRunning();app_.genStop(true);if(run)app_.genPlay();});
    button({200,283,60,30},"TAPE",false,[this]{sheetOpen_=false;app_.setGenerativeDeck(false);},11,true,RED);
    if(!app_.toast().empty())text(23,318,8,TRACE_AMBER,fit(app_.toast(),236,8));
    header({282,12,664,23},"FIELD / TRAVERSAL",std::string(mapNames[kind])+" / SEED "+std::to_string(p.config.seed));
    bevel({282,39,664,226},WELL,true);
    text(291,51,9,WELL_DIM,"GRADIENT FIELD / NORMALIZED");
    text(588,51,9,WELL_DIM,"APPLY",2);
    button({594,43,70,17},applyNames[c.apply],false,[this]{app_.genApply((app_.genConfig().apply+1)%3);},9);
    for(int i=0;i<3;i++)button({807.f+i*44,43,42,17},mapNames[i],kind==i,[this,i]{app_.genSetMap(i);},9,false,RAISED);
    Rect map{316,65,574,176};
    if(genMapVersion_!=app_.genVersion()||genMapKind_!=kind){
        genMapVersion_=app_.genVersion();genMapKind_=kind;
        for(int j=0;j<=32;j++)for(int i=0;i<=72;i++)
            genMapValues_[j*73+i]=field(p,i/72.f,1-j/32.f,kind);
    }
    // Sampled heatmap, with interpolated cell centers and marching-square contours.
    auto X=[&](float q){return map.x+map.w*q;};auto Y=[&](float q){return map.y+map.h*(1-q);};
    for(int j=0;j<32;j++)for(int i=0;i<72;i++){
        float a=genMapValues_[j*73+i],b=genMapValues_[j*73+i+1],d=genMapValues_[(j+1)*73+i],e=genMapValues_[(j+1)*73+i+1];
        rrect({map.x+map.w*i/72,map.y+map.h*j/32,map.w/72+.2f,map.h/32+.2f},0,color((a+b+d+e)*.25f));
    }
    for(int j=0;j<32;j++)for(int i=0;i<72;i++){
        const float a=genMapValues_[j*73+i],b=genMapValues_[j*73+i+1],d=genMapValues_[(j+1)*73+i],e=genMapValues_[(j+1)*73+i+1];
        for(float z:{-.5f,-.25f,0.f,.25f,.5f}){
            Point pts[4];int n=0;
            if((a<z)!=(b<z))pts[n++]={i+(z-a)/(b-a),float(j)};
            if((b<z)!=(e<z))pts[n++]={float(i+1),j+(z-b)/(e-b)};
            if((d<z)!=(e<z))pts[n++]={i+(z-d)/(e-d),float(j+1)};
            if((a<z)!=(d<z))pts[n++]={float(i),j+(z-a)/(d-a)};
            for(int k=0;k+1<n;k+=2)line(map.x+map.w*pts[k].x/72,map.y+map.h*pts[k].y/32,map.x+map.w*pts[k+1].x/72,map.y+map.h*pts[k+1].y/32,0x38ebf4f6,.65f);
        }
    }
    stroke(map,0,WELL_DIM,.8f);
    for(int i=1;i<5;i++){
        line(X(i/5.f),map.y,X(i/5.f),map.y+map.h,0x38edf2ee,.7f);
        line(map.x,Y(i/5.f),map.x+map.w,Y(i/5.f),0x38edf2ee,.7f);
    }
    for(int i=1;i<p.n;i++){
        const auto a=p.event[i-1].point,b=p.event[i].point;
        line(X(a.x),Y(a.y),X(b.x),Y(b.y),0x9917202a,4);
        if(i<=cursor)line(X(a.x),Y(a.y),X(b.x),Y(b.y),TRACE_AMBER,2.2f);
        else for(int j=0;j<16;j+=2){const float u=j/16.f,v=(j+1)/16.f;
            line(X(std::lerp(a.x,b.x,u)),Y(std::lerp(a.y,b.y,u)),X(std::lerp(a.x,b.x,v)),Y(std::lerp(a.y,b.y,v)),0xfff2f4e9,1.5f);}
    }
    for(int i=0;i<p.n;i++){
        auto q=p.event[i].point;const bool pin=c.pins[i]>=0;float rr=i==cursor?5.6f:3.2f;
        circle(X(q.x),Y(q.y),rr+1,0xff17202a);circle(X(q.x),Y(q.y),rr,i==cursor||pin?TRACE_AMBER:0xffe9ece6);
        text(X(q.x)+6,Y(q.y)-7,8,0xfff8fafb,std::to_string(i+1));
    }
    auto relocate=[this,map](float x,float y){app_.genRelocate(clamp01((x-map.x)/map.w),1-clamp01((y-map.y)/map.h));};
    hit(map,relocate,relocate);
    for(int j=0;j<64;j++)rrect({900,72+j*2.5f,18,2.6f},0,color(1-2.f*j/63));
    text(921,73,8,WELL_DIM,"+1");text(921,152,8,WELL_DIM,"0");text(921,229,8,WELL_DIM,"-1");
    text(292,70,8,WELL_DIM,"1.0");text(292,153,9,WELL_DIM,"Y");text(292,237,8,WELL_DIM,"0.0");
    text(316,254,8,WELL_DIM,"0.0");text(603,254,9,WELL_DIM,"X / DRAG TO RELOCATE",1);text(890,254,8,WELL_DIM,"1.0",2);
    text(938,254,8,WELL_DIM,std::string(mapNames[kind])+" OFFSET",2);
    for(int i=0;i<4;i++)button({282.f+i*166,271,163,23},pageNames[i],app_.genPage()==i,[this,i]{app_.genSetPage(i);},11,false,VIOLET);
    const auto k=app_.genKnobs();
    for(int i=0;i<4;i++){
        float x=282+i*166;Rect bar{x,311,160,18};uint32_t col=encoderColor(i);
        text(x,301,9,col,"K"+std::to_string(i+1)+" "+k[i].name,0,true);
        text(x+160,301,9,DIM,k[i].value,2);
        bevel(bar,TRACK,true);rrect({x+1,312,158*k[i].norm,16},0,col);
        line(x+1+158*k[i].norm,312,x+1+158*k[i].norm,327,HI,1.5f);
        auto set=[this,i,bar](float px,float){auto k=app_.genKnobs();k[i].set(clamp01((px-bar.x)/bar.w));};
        hit(bar,set,set);
    }
    text(14,342,11,VIOLET,"PHRASE",0,true);
    cycle({61,333,76,19},motifNames[c.motif],[this]{genSelector_=3;});
    cycle({140,333,36,19},std::to_string(count(c)),[this]{if(app_.genConfig().motif!=3)genSelector_=4;});
    text(181,342,9,DIM,"EVENTS");
    cycle({222,333,68,19},"PIN ENDS",[this]{app_.genPinEnds();});
    cycle({293,333,76,19},"CLEAR PINS",[this]{app_.genClearPins();});
    text(376,342,9,DIM,c.motif==3&&!running?"CUSTOM: PRESS UNIQUE KEYS / LAST KEY REMOVES":"AMBER = PINNED / OUTLINE = SKELETON");
    text(945,342,9,DIM,num("%.2f S",p.total),2);
    bevel({14,356,932,82},WELL,true);
    text(24,367,9,WELL_DIM,"PHRASE / FREE NOTE BOUNDARIES");
    text(937,367,9,WELL_DIM,(running?"PLAYING / PASS ":"PREVIEW / PASS ")+std::to_string(p.pass+1),2);
    const float start=58,span=868,top=379,bot=428;
    auto NY=[&](int semi){return bot-(bot-top)*std::clamp(semi,0,12)/12.f;};
    for(int i=0;i<=12;i++)line(start,NY(i),start+span,NY(i),GRID,.7f);
    text(52,NY(12),8,WELL_DIM,SamplerApp::noteName(p.config.root),2);
    text(52,NY(0),8,WELL_DIM,SamplerApp::noteName(p.config.root),2);
    if(p.n==0)text(492,405,12,WELL_DIM,"PRESS KEYS TO BUILD A CUSTOM PHRASE",1);
    if(p.total>0)for(int i=0;i<p.n;i++){
        const auto& v=p.event[i];float x=start+span*v.on/p.total,w=std::max(1.f,span*(v.off-v.on)/p.total);
        const float bx=start+span*v.nominalOn/p.total,bw=span*(v.nominalOff-v.nominalOn)/p.total,y=NY(v.midi-rootMidi(p.config)),by=NY(v.base);
        line(bx,375,bx,431,GRID,.8f);
        for(float dx=0;dx<bw;dx+=5){line(bx+dx,by-2,bx+std::min(dx+3,bw),by-2,WELL_DIM,.8f);line(bx+dx,by+2,bx+std::min(dx+3,bw),by+2,WELL_DIM,.8f);}
        line(bx,by-2,bx,by+2,WELL_DIM,.8f);line(bx+bw,by-2,bx+bw,by+2,WELL_DIM,.8f);
        const bool editing=c.motif==3&&!running;
        const uint32_t col=c.pins[i]>=0?TRACE_AMBER:(app_.genActive()&(1u<<i))?0xfff5f7f8:TRACE_BLUE;
        rrect({x,y-2.6f,w,5.2f},0,col);
        if(editing)rrect({bx,by-1.8f,bw,3.6f},0,0xfff5f7f8);
        tap({x,y-5,std::max(8.f,w),10},[this,i]{app_.genTogglePin(i);});
    }
    if(running&&p.total>0){float x=start+span*clamp01(app_.genSeconds()/p.total);line(x,375,x,431,TRACE_AMBER,1);}
}

void SamplerUI::generativeSelector(){
    using namespace generative;
    const int sel=genSelector_;
    if(sel<0||sel>4)return;
    const auto& c=app_.genConfig();
    std::vector<std::string> names;
    int selected=0;
    std::string title;
    switch(sel){
    case 0:title="ROOT";for(int i=0;i<12;i++)names.push_back(SamplerApp::noteName(i));selected=c.root;break;
    case 1:title="SCALE";for(auto n:scaleNames)names.push_back(n);selected=c.scale;break;
    case 2:title="OCTAVE";for(int i=2;i<=6;i++)names.push_back(std::to_string(i));selected=c.octave-2;break;
    case 3:title="PHRASE";for(auto n:motifNames)names.push_back(n);selected=c.motif;break;
    default:title="EVENTS";for(int i=1;i<=16;i++)names.push_back(std::to_string(i));selected=c.steps-1;break;
    }
    rrect({0,0,W_,H_},0,withAlpha(BG,208));tap({0,0,W_,H_},[this]{genSelector_=-1;});
    const int cols=names.size()>8?3:2,rows=(int(names.size())+cols-1)/cols;
    const float ww=std::min(W_*.85f,cols*146.f),hh=std::min(H_*.85f,45.f+rows*34.f);
    Rect box{(W_-ww)*.5f,(H_-hh)*.5f,ww,hh};bevel(box,SURFACE);tap(box,[]{});
    strip({box.x+5,box.y+5,box.w-10,26},VIOLET,title,14);
    const float cw=(ww-12)/cols,ch=(hh-38)/rows;
    for(int i=0;i<int(names.size());i++){
        Rect r{box.x+6+(i%cols)*cw,box.y+35+(i/cols)*ch,cw-3,ch-3};
        button(r,names[i],i==selected,[this,sel,i]{
            const auto c=app_.genConfig();
            if(sel==0)app_.genHarmonic(0,i-c.root);
            else if(sel==1)app_.genHarmonic(1,i-c.scale);
            else if(sel==2)app_.genHarmonic(2,i+2-c.octave);
            else if(sel==3)app_.genMotif(i);
            else app_.genSteps(i+1);
            genSelector_=-1;
        },11,false,VIOLET);
    }
}
