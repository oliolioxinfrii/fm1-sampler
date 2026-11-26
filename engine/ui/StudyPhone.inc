// Phone-scale transcription of fm1-interior-study-r10.html, panelArt.
// Design coordinates are 520 x 186; the compact FM-1 renderer is independent.
// No synthetic source/audio waveforms. Parameter diagrams are schematic.
void SamplerUI::studyPhone(Rect w, SamplerApp::Page pg) {
    using P=SamplerApp; using Pt=std::pair<float,float>; using Path=std::vector<Pt>;
    const int t=app_.track(); const auto& ts=app_.state(t);const auto k=app_.knobs(pg);
    const float a=clamp01(k[0].norm),b=clamp01(k[1].norm),c=clamp01(k[2].norm),d=clamp01(k[3].norm);
    const float tau=2*kPi;
    const uint32_t metal=theme::light?0xff485362:0xff39304c,lip=theme::light?0xff8b9295:0xff807194,
      winding=theme::light?0xff605245:0xff574050,shadow=theme::light?0xff667080:0xff48405f;
    const uint32_t blue=TRACE_BLUE,green=TRACE_GREEN,violet=TRACE_VIOLET,amber=TRACE_AMBER,dim=WELL_DIM,ink=WELL_TEXT;
    auto X=[&](float x){return w.x+x*w.w/520.f;};auto Y=[&](float y){return w.y+y*w.h/186.f;};
    const float sc=std::min(w.w/520.f,w.h/186.f);
    auto al=[](uint32_t color,float v){return withAlpha(color,int(255*clamp01(v)));};
    auto mix=[](uint32_t p,uint32_t q,float v){v=clamp01(v);uint32_t out=0xff000000u;for(int s=0;s<=16;s+=8)out|=uint32_t(std::lround(float(p>>s&255)*(1-v)+float(q>>s&255)*v))<<s;return out;};
    auto rnd=[](int i){double x=std::sin(i*127.1+311.7)*43758.5453;return float(x-std::floor(x));};
    auto L=[&](float x,float y,float xx,float yy,uint32_t color,float width=1.f){line(X(x),Y(y),X(xx),Y(yy),color,std::max(.45f,width*sc));};
    auto R=[&](float x,float y,float ww,float hh,uint32_t fill,uint32_t edge=0,float sw=1.f,float rad=0.f){
        if(ww<=0||hh<=0)return;Rect z{X(x),Y(y),X(x+ww)-X(x),Y(y+hh)-Y(y)};
        if(fill)rrect(z,rad*sc,fill);if(edge)stroke(z,rad*sc,edge,std::max(.45f,sw*sc));};
    auto M=[&](float x,float y,const std::string& str,uint32_t color=0,float size=9.f,int align=0){text(X(x),Y(y)-size*sc*.36f,size*sc,color?color:dim,str,align,false);};
    auto D=[&](const Path& pts,uint32_t color,float width=1.f){for(size_t j=1;j<pts.size();++j)L(pts[j-1].first,pts[j-1].second,pts[j].first,pts[j].second,color,width);};
    auto quad=[&](Pt p,Pt q,Pt r,Pt s,uint32_t color){tri(X(p.first),Y(p.second),X(q.first),Y(q.second),X(r.first),Y(r.second),color);tri(X(p.first),Y(p.second),X(r.first),Y(r.second),X(s.first),Y(s.second),color);};
    auto shape=[&](const Path& pts,uint32_t fill,uint32_t edge=0,float width=1.f){if(pts.size()<3)return;if(fill)for(size_t j=2;j<pts.size();++j)tri(X(pts[0].first),Y(pts[0].second),X(pts[j-1].first),Y(pts[j-1].second),X(pts[j].first),Y(pts[j].second),fill);if(edge){D(pts,edge,width);L(pts.back().first,pts.back().second,pts[0].first,pts[0].second,edge,width);}};
    auto O=[&](float x,float y,float r,uint32_t fill,uint32_t edge=0,float width=1.f){if(fill)circle(X(x),Y(y),r*sc,fill);if(edge){Path p;for(int i=0;i<=40;i++){float z=tau*i/40;p.push_back({x+r*std::cos(z),y+r*std::sin(z)});}D(p,edge,width);}};
    auto rail=[&](float x,float y,float ww,float hh){R(x,y,ww,hh,metal,lip);L(x+2,y+1,x+ww-2,y+1,dim,.7f);L(x+1,y+hh-1,x+ww-1,y+hh-1,WELL);};
    auto port=[&](float x,float y,uint32_t color){O(x,y,6,metal,dim);O(x,y,3.3f,WELL,color,1.4f);};
    auto screw=[&](float x,float y){O(x,y,3,lip,WELL,.8f);L(x-1.8f,y,x+1.8f,y,WELL,.8f);};
    auto arrow=[&](float x,float y,float xx,float yy,uint32_t col,float width=1.5f){L(x,y,xx,yy,col,width);float ang=std::atan2(yy-y,xx-x);shape({{xx,yy},{xx+7*std::cos(ang+2.7f),yy+7*std::sin(ang+2.7f)},{xx+7*std::cos(ang-2.7f),yy+7*std::sin(ang-2.7f)}},col);};
    auto ticks=[&](float x,float y,int n){for(int i=0;i<n;i++)L(x+i*8,y,x+i*8,y-(i%4==0?6.f:3.f),dim,.8f);};
    auto wave=[&](const std::vector<float>& p,float x,float y,float ww,float hh,uint32_t color,float opacity=0.3f){
        if(p.size()<2)return;for(size_t j=1;j<p.size();j++){float xx=x+ww*(j-1)/(p.size()-1),xxx=x+ww*j/(p.size()-1);
            float v=hh*clamp01(p[j-1]),vv=hh*clamp01(p[j]);quad({xx,y-v},{xxx,y-vv},{xxx,y+vv},{xx,y+v},al(color,opacity));}
    };
    auto source=app_.peaks(t,300);
    auto position=[&](float x,float y,float yy){float p=ts.start+(ts.end-ts.start)*ts.echo.position;float px=x+492*p;L(px,y,px,yy,amber,2);M(std::clamp(px+5,16.f,430.f),yy+9,"POS "+num("%.1f%%",ts.echo.position*100),amber);
        Rect hr{X(x),Y(y),X(x+492)-X(x),Y(yy)-Y(y)};
        auto move=[this,t,hr](float px,float){if(app_.track()!=t||app_.algo()!=4)return;const auto& z=app_.state(t);app_.knobs(P::P_HOME)[3].set(clamp01(((px-hr.x)/hr.w-z.start)/std::max(.001f,z.end-z.start)));};hit(hr,move,move);};
    R(0,0,520,186,WELL);
    for(int i=1;i<8;i++)L(65*i,1,65*i,185,GRID,.7f);
    for(int i=1;i<4;i++)L(1,46.5f*i,519,46.5f*i,GRID,.7f);
    clip({X(3),Y(3),X(517)-X(3),Y(183)-Y(3)});
    switch(pg){
    case P::P_HOME:{
        M(15,15,"SOURCE BUFFER");M(505,15,app_.sampleLabel(t),dim,9,2);
        wave(source,14,75,492,51,dim,.28f);
        float l=14+492*a,r=14+492*b;
        if(source.size()>1)for(size_t j=1;j<source.size();j++){
            float xx=14+492.f*(j-1)/(source.size()-1),xxx=14+492.f*j/(source.size()-1);
            if(xx>=l&&xxx<=r){float v=51*clamp01(source[j-1]),vv=51*clamp01(source[j]);quad({xx,75-v},{xxx,75-vv},{xxx,75+vv},{xx,75+v},al(green,.48f));}}
        if(source.empty())M(260,80,"NO SAMPLE",dim,12,1);
        R(l,21,std::max(1.f,r-l),107,0,al(green,.45f));
        L(l,20,l,132,blue,2);L(r,20,r,132,green,2);
        shape({{l-5,20},{l+5,20},{l,28}},blue);shape({{r-5,20},{r+5,20},{r,28}},green);
        for(int j=0;j<st_.voiceN[t];j++){float xx=14+492*clamp01(st_.voicePos[t][j]);L(xx,75-49*clamp01(st_.voiceLvl[t][j]),xx,75+49*clamp01(st_.voiceLvl[t][j]),encoderTrace(j),1.5f);}
        for(int i=0;i<2;i++){float xx=i?r:l;Rect hr{X(xx-9),Y(19),X(xx+9)-X(xx-9),Y(132)-Y(19)};
            auto set=[this,i,t,origin=X(14),span=X(506)-X(14)](float px,float){if(app_.track()==t)app_.knobs(P::P_HOME)[i].set(clamp01((px-origin)/span));};hit(hr,[](float,float){},set);}
        rail(14,141,492,29);R(16,143,488,25,WELL);
        if(ts.algo==4){for(int i=0;i<24;i++)R(21+i*20,153-(i*(ts.echo.mode+1)%7),12,7+(i*(ts.echo.mode+1)%8),al(violet,.28f+(i%3)*.18f));position(14,137,165);}
        else if(ts.algo==3){auto sl=app_.session().sliceFractions(t);for(float v:sl)L(14+492*v,24,14+492*v,130,violet,.9f);M(497,160,k[3].value,amber,10,2);}
        else {int n=5+int(std::lround(c*14));for(int i=0;i<n;i++){float xx=24+i*310.f/(n-1);L(xx,149,xx,163,violet,1.3f);}if(ts.algo==5){L(28,153-7*d,92,153-7*d,amber,2);L(28,157+7*d,92,157+7*d,amber,2);}else R(28,155-(d-.5f)*13,64,5,amber);M(497,161,ts.algo==5?"STRETCH / WIDTH":"SPEED / PITCH",dim,9,2);}
        break;}
    case P::P_ECHO:{
        M(15,14,"SOURCE");M(505,14,"REGISTERED SECTIONS",dim,9,2);wave(source,15,29,490,9,dim,.4f);L(15,44,505,44,lip);
        for(int i=0;i<9;i++){float q=i/8.f,xx=22+q*468+(rnd(i+20)-.5f)*b*24,yy=95+(rnd(i+7)-.5f)*b*54+(q-.5f)*c*48,len=22+22*rnd(i+7)+d*30*rnd(i+40),hh=22+12*rnd(i+2),left=std::clamp(xx-len*.5f,8.f,487-len),right=left+len;
            L(xx,45,xx,yy-hh/2,GRID);
            shape({{left,yy-hh/2},{left+6,yy-hh/2-6},{right+6,yy-hh/2-6},{right,yy-hh/2}},lip,dim,.7f);
            shape({{right,yy-hh/2},{right+6,yy-hh/2-6},{right+6,yy+hh/2-6},{right,yy+hh/2}},metal,dim,.7f);
            R(left,yy-hh/2,len,hh,metal,dim,.8f);int count=3+int(std::lround(a*7));
            for(int j=0;j<count;j++){float src=source.empty()?0.f:source[std::min(source.size()-1,size_t((i*count+j)*source.size()/(9*count)))];float hh2=hh*(.18f+.58f*clamp01(src));R(left+3+j*(len-6)/count,yy-hh2*.5f,std::max(1.6f,(len-6)/count-1.5f),hh2,al(i==4?violet:dim,.6f));}
            L(left+2,yy+hh/2-3,right-2,yy+hh/2-3,i==4?amber:GRID,1.3f);}
        L(15,156,505,156,lip,.8f);ticks(15,156,62);M(15,170,"0");M(505,170,"RELATIVE SOURCE",dim,9,2);position(15,18,38);break;}
    case P::P_GRAIN:{
        M(15,15,"SOURCE WINDOW");M(260,15,"SIZE "+k[0].value+" / DENS "+k[1].value,dim,9,1);M(505,15,k[2].value,violet,10,2);wave(source,14,45,492,23,dim,.35f);
        int n=4+int(std::lround(15*b)),mode=ts.echo.window;float ww=20+75*a;
        auto perc=[&](float q){float v=q<.03f?std::sin(.5f*kPi*q/.03f):std::exp(-5.f*(q-.03f)/.97f);if(q>.92f)v*=.5f+.5f*std::cos(kPi*(q-.92f)/.08f);return v;};
        auto win=[&](float q){if(q<=0||q>=1)return 0.f;switch(mode){case 2:return q<.15f?.5f*(1-std::cos(tau*q/.3f)):q>.85f?.5f*(1-std::cos(tau*(1-q)/.3f)):1.f;case 3:return perc(q);case 4:return perc(1-q);case 5:{float z=(q-.5f)/.16f;return clamp01((std::exp(-.5f*z*z)-.00752f)/(1-.00752f));}default:return .5f*(1-std::cos(tau*q));}};
        for(int i=0;i<n;i++){float xx=18+(460-ww)*i/std::max(1,n-1),yy=112+(i%3)*10;uint32_t col=i%4==0?violet:dim;Path p;
            for(int j=0;j<=40;j++)p.push_back({xx+ww*j/40,yy-48*win(j/40.f)});
            for(size_t j=1;j<p.size();j++)quad({p[j-1].first,yy},p[j-1],p[j],{p[j].first,yy},al(col,.17f));D(p,col,1.1f);L(xx,yy,xx+ww,yy,GRID);R(xx-1,yy-2,3,4,dim);}
        L(14,143,506,143,lip);ticks(14,143,62);float bx=d>.5f?205:134,bw=d>.5f?110:251;D({{bx,151},{bx,163},{bx+bw,163},{bx+bw,151}},d>.5f?amber:dim,2);M(260,178,d>.5f?"POSITION HELD":"WINDOW ADVANCE",d>.5f?amber:dim,9,1);position(14,23,62);break;}
    case P::P_MOTION:{
        M(15,15,"SOURCE POSITION");M(505,15,"PITCH OFFSET",dim,9,2);R(14,23,492,133,al(GRID,.24f),lip,.8f);L(14,90,506,90,dim);
        float ox=260,scan=2*a-1,travel=std::abs(scan)*220,sign=scan<0?-1.f:1.f;
        for(int i=0;i<9;i++){bool rev=i/8.f<b;float dir=(rev?-1.f:1.f)*sign,end=std::clamp(ox+dir*travel+(i-4)*5,30.f,490.f),h=(i-4)*c*13,y2=90+h,x1=ox+dir*std::max(18.f,travel*.45f);Path p;
            for(int j=0;j<=40;j++){float q=j/40.f,z=1-q;p.push_back({z*z*z*ox+3*z*z*q*x1+3*z*q*q*(end-dir*45)+q*q*q*end,z*z*z*90+3*z*z*q*(90+h*.1f)+3*z*q*q*y2+q*q*q*y2});}
            D(p,al(rev?green:blue,rev?.65f:.75f),i==4?3:1.25f);float sgn=end>=ox?1.f:-1.f;shape({{end,y2},{end-sgn*7,y2-3},{end-sgn*7,y2+3}},rev?green:blue);}
        R(ox-7,29,14,120,metal,dim);R(ox-2,34,4,110,amber);screw(ox,26);screw(ox,153);float x=d>.5f?ox-28:ox-92,bw=d>.5f?56:184;D({{x,165},{x,157},{x+bw,157},{x+bw,165}},d>.5f?amber:dim,2);M(260,180,d>.5f?"LATCHED":"FREE",d>.5f?amber:dim,9,1);break;}
    case P::P_EDELAY:{
        M(15,15,"L");M(15,110,"R");M(505,15,"REPEAT / "+k[0].value,dim,9,2);
        L(24,53,503,53,GRID);L(24,121,503,121,GRID);const float dt=22+93*a;uint32_t wire=mix(dim,green,b);
        struct Cell{float x,y,h,w;};std::vector<Cell> cells;
        for(float x=40;x<493;x+=dt){int i=int(cells.size());cells.push_back({x,d>.5f?(i%2?121.f:53.f):87.f,26+20*std::pow(.83f,float(i)),std::min(dt*.5f,24.f)});}
        for(size_t i=1;i<cells.size();i++){auto p=cells[i-1],q=cells[i];float px=p.x+p.w/2,qx=q.x-q.w/2,m=(px+qx)/2;D({{px,p.y},{m,p.y},{m,q.y},{qx,q.y}},wire,2);}
        if(!cells.empty()){auto p=cells.back(),q=cells.front();float py=p.y+p.h/2,qy=q.y+q.h/2;D({{p.x,py},{p.x,153},{q.x,153},{q.x,qy}},wire,2.2f);arrow(q.x,qy+10,q.x,qy,wire,2.2f);}
        for(size_t i=0;i<cells.size();i++){auto q=cells[i];float x=q.x-q.w/2,y=q.y-q.h/2,v=i==0?.17f+.76f*c:.89f*c*std::pow(.86f,float(i));shape({{x,y},{x+5,y-4},{x+q.w+5,y-4},{x+q.w,y}},lip,dim,.7f);R(x,y,q.w,q.h,metal,dim,.8f);R(x+2,y+2,q.w-4,q.h-4,mix(metal,violet,v));L(x+2,q.y,x+q.w-2,q.y,mix(dim,violet,v),1.5f);}
        ticks(24,171,60);M(24,182,"0",dim,8);M(504,182,"RELATIVE TIME",dim,8,2);break;}
    case P::P_EREVERB:{
        const int nx=16,ny=7;auto pt=[&](float u,float v)->Pt{return {18+484*u+24*b*std::sin(tau*(v*.83f+u*.72f))*std::sin(kPi*u),26+126*v+17*b*std::sin(tau*(u*1.55f+v*.54f))*std::sin(kPi*v)};};
        M(17,15,"ROOM / OCCUPANCY MODEL");M(503,15,"SIZE "+k[1].value,green,9,2);
        for(int j=0;j<ny;j++)for(int i=0;i<nx;i++){float u=(i+.5f)/nx,v=(j+.5f)/ny,phase=.5f+.5f*std::cos(tau*(u*1.15f-v*.8f)),env=.5f+.5f*std::cos(tau*(u*.7f+v*1.1f+.18f));float level=clamp01(a*(.42f+.58f*phase)),occ=clamp01((a-(.08f+.68f*(1-env)))*4.4f);quad(pt(float(i)/nx,float(j)/ny),pt(float(i+1)/nx,float(j)/ny),pt(float(i+1)/nx,float(j+1)/ny),pt(float(i)/nx,float(j+1)/ny),al(phase>.62f?blue:dim,.025f+level*.12f+occ*(.17f+.17f*phase)));}
        // Same continuous horizontal strands, vertical strands, and alternating overpasses as the study.
        for(int j=0;j<=ny;j++){Path p;for(int i=0;i<=64;i++)p.push_back(pt(i/64.f,float(j)/ny));D(p,WELL,5.1f);D(p,al(dim,.34f+.38f*a),3);for(auto& q:p)q.second-=.75f;D(p,al(ink,.17f+.36f*a),.8f);}
        for(int i=0;i<=nx;i++){Path p;for(int j=0;j<=40;j++)p.push_back(pt(float(i)/nx,j/40.f));D(p,WELL,3.3f);D(p,al(dim,.30f+.28f*a),1.65f);}
        for(int j=0;j<=ny;j++)for(int i=1;i<nx;i++)if((i+j)%2==0){Path p;for(int z=0;z<=8;z++)p.push_back(pt((i-.23f+.46f*z/8)/nx,float(j)/ny));D(p,WELL,4.7f);D(p,al(dim,.39f+.37f*a),2.8f);}
        if(c>0)for(int j=0;j<ny;j++){Path p;for(int i=0;i<=64;i++)p.push_back(pt(i/64.f,(j+.5f)/ny));D(p,al(violet,c*(.38f+.38f*a)),.7f+1.2f*c);}
        if(d>.001f){int n=1+int(8*d);for(int j=0;j<n;j++){float v=(j+.5f)/n;Path p;for(int i=0;i<=80;i++){float u=i/80.f;p.push_back(pt(u,std::clamp(v+.13f*std::sin(tau*(u*1.25f+v)),.02f,.98f)));}D(p,al(WELL,.64f),2.5f);D(p,al(amber,.28f+.30f*d*(.55f+.45f*a)),1+.55f*d);}}
        M(17,173,"8 DELAY LINES");M(260,173,"DAMP "+k[2].value,violet,9,1);M(503,173,"SHIMMER "+k[3].value,amber,9,2);break;}
    case P::P_ETAPE:{
        const float lo=15,hi=504;
        auto mid=[&](float q){return 98+19*a*std::sin(tau*(q*.92f-.18f))+2.8f*b*std::sin(tau*(q*11+.12f));};
        auto top=[&](float q){return mid(q)-32+1.6f*b*std::sin(tau*q*19);};
        auto bot=[&](float q){return mid(q)+25+1.3f*b*std::sin(tau*q*17);};
        auto wear=[&](float q){float z=0;for(int i=0;i<12;i++){float u=.04f+.92f*rnd(i+129),v=q-u;if(v<-.018f||v>.13f)continue;float amp=clamp01(c*1.6f-rnd(i+23)*.72f),g=v<0?std::exp(-std::pow(v/.006f,2.f)):std::exp(-v/(.009f+.019f*c))*clamp01((.13f-v)/.035f);z=std::max(z,amp*g);}return z;};
        auto curve=[&](auto f){Path p;for(int i=0;i<=120;i++){float q=i/120.f;p.push_back({lo+(hi-lo)*q,f(q)});}return p;};
        auto band=[&](const Path& p,const Path& q,uint32_t fill,uint32_t edge=0,float width=1.f){for(size_t i=1;i<p.size();i++)quad(p[i-1],p[i],q[i],q[i-1],fill);if(edge){D(p,edge,width);D(q,edge,width);L(p.front().first,p.front().second,q.front().first,q.front().second,edge,width);L(p.back().first,p.back().second,q.back().first,q.back().second,edge,width);}};
        auto tp=curve(top),bp=curve(bot),coat=curve([&](float q){return top(q)+11;}),lam=curve([&](float q){return bot(q)-9;}),bottom=curve([&](float q){return bot(q)+8;});
        M(15,15,"COATING / CARRIER");M(506,15,"CONDITION / MODEL",dim,9,2);
        band(tp,coat,lip,dim);band(coat,lam,winding,metal,.8f);band(lam,bp,shadow,dim);band(bp,bottom,metal,dim,.7f);
        D(curve([&](float q){return top(q)+4;}),dim,1.1f);D(curve([&](float q){return top(q)+10;}),al(green,.75f),1.2f);
        if(c>.001f){auto loss=curve([&](float q){return top(q)+11+10*wear(q);});band(coat,loss,al(WELL,.63f));D(loss,al(violet,.14f+.42f*c),1.2f);}
        auto signal=curve([&](float q){return bot(q)-3;});for(size_t i=1;i<signal.size();i++)L(signal[i-1].first,signal[i-1].second,signal[i].first,signal[i].second,al(blue,.65f*(1-.8f*wear(i/120.f))),2.4f);
        for(int i=0;i<int(std::lround(140*d));i++){float q=.012f+.976f*rnd(700+i),yy=top(q)+13+(bot(q)-11-top(q)-13)*rnd(i+731),r=.7f+1.3f*rnd(i+19);R(lo+(hi-lo)*q,yy,r*(1+1.3f*rnd(i+101)),r,al(amber,.45f+.42f*rnd(i+12)));}
        float gx=190,gy=top((gx-lo)/(hi-lo))-12;
        shape({{gx-37,gy-16},{gx-36,gy-23},{gx-28,gy-26},{gx+33,gy-26},{gx+40,gy-23},{gx+43,gy-16},{gx+35,gy+8},{gx+28,gy+15},{gx+18,gy+17},{gx-18,gy+17},{gx-30,gy+13},{gx-34,gy+6}},metal,dim,1.2f);
        D({{gx-34,gy-17},{gx-30,gy-23},{gx+32,gy-23},{gx+38,gy-17}},lip,2);R(gx-14,gy-10,32,15,WELL,lip,.9f,2);R(gx-6,gy-6,16,7,ink);screw(gx-27,gy-12);screw(gx+30,gy-12);L(gx-17,gy+13,gx+18,gy+13,amber,2.4f);D({{gx+42,gy-15},{gx+68,gy-15},{gx+68,24},{gx+127,24}},dim);M(gx+130,27,"HEAD / SECTION");M(15,173,"RELATIVE DISTANCE");ticks(135,174,32);M(503,173,"AGE "+k[2].value,violet,9,2);break;}
    case P::P_EMIX:{
        M(15,15,"INSERT PATCH / BUS");M(505,15,"WIDTH / LEVEL",dim,9,2);
        rail(13,36,84,124);R(18,41,74,114,WELL);M(25,55,"STEREO IN");M(25,79,"L");port(80,76,blue);M(25,103,"R");port(80,100,blue);M(25,136,"2 CH / BUS",dim,8);
        D({{86,76},{105,76},{105,58},{126,58}},blue,2);D({{86,100},{113,100},{113,71},{126,71}},blue,2);
        D({{236,58},{251,58},{251,99},{112,99},{112,128},{126,128}},blue,1.9f);D({{236,71},{244,71},{244,94},{104,94},{104,141},{126,141}},blue,1.9f);
        auto mod=[&](float y,const char* name,float v,uint32_t col){rail(126,y,110,54);R(131,y+5,100,44,WELL);M(137,y+15,name,col);M(221,y+15,"SERIES",dim,8,2);L(140,y+30,222,y+30,al(dim,.35f),2);R(142,y+36,78,10,metal,dim,.7f);R(144,y+38,74*v,6,al(col,.72f));L(144+74*v,y+35,144+74*v,y+48,col,2);screw(228,y+9);};
        mod(38,"TAPE",c,violet);mod(108,"FILTER",d,amber);
        D({{236,128},{264,128},{264,105},{282,105}},blue,2);D({{236,141},{272,141},{272,118},{282,118}},blue,2);
        rail(282,39,154,126);R(287,44,144,116,WELL);M(292,54,"M/S STEREO");M(426,54,"2 CH",dim,8,2);
        R(317,57,84,84,0,GRID,.8f);
        O(359,99,36,0,al(dim,.62f),1.f);O(359,99,18,0,al(dim,.35f),.7f);
        L(323,99,395,99,al(dim,.7f),.8f);L(359,63,359,135,al(dim,.7f),.8f);
        L(334,74,384,124,GRID,.6f);L(334,124,384,74,GRID,.6f);
        M(363,70,"M",dim,8);M(390,95,"S",dim,8,2);
        float midRms=0.f,sideRms=0.f;
        studyScopeEnergy(t,midRms,sideRms);
        // One continuously animated path; windowed post-insert RMS controls
        // a small smooth ripple concentrated at its leading end.
        auto tintAt=[&](float u){
            const float phase=.035f*float(now_%600000u)*.001f;
            float w1=.5f+.5f*std::sin(tau*(u*6+phase));
            float w2=.5f+.5f*std::sin(tau*(u*6+phase+.34f));
            return mix(mix(blue,violet,c*std::pow(w1,1.35f)),amber,d*std::pow(1-w2,1.35f));
        };
        constexpr int points=180;
        auto last=insertViz(0.f,now_,a,b,midRms,sideRms);
        for(int j=1;j<=points;j++){
            const float q=float(j)/points;
            auto next=insertViz(q,now_,a,b,midRms,sideRms);
            const uint32_t col=mix(tintAt(q),ink,.12f);
            L(359+33*last.side,99-33*last.mid,359+33*next.side,99-33*next.mid,col,2.05f);
            last=next;
        }
        M(292,141,"SIGNAL-REACTIVE",dim,7);
        M(292,151,"W "+k[0].value,blue);M(426,151,"G "+k[1].value,green,9,2);M(282,26,"OUTPUT SET");M(434,26,k[1].value,green,9,2);R(357,18,53,9,metal,dim,.7f);R(359,20,49*b*b,5,green);
        D({{436,77},{452,77},{452,64},{470,64}},blue,2.2f);D({{436,121},{452,121},{452,138},{470,138}},blue,2.2f);port(476,64,blue);port(476,138,blue);M(491,67,"L");M(491,141,"R");M(505,178,"STEREO OUT",dim,9,2);M(15,178,"ANIMATED / POST-INSERT RESPONSE",dim,8);break;}
    case P::P_MIX:{
        int sel=ts.scSrc;bool on=sel>0&&sel<=8&&sel!=t+1;float ratio=1+19*d*d,th=-40*(1-c);
        M(15,15,"CHANNEL "+std::to_string(t+1)+" / PATCH");M(505,15,"SIDECHAIN / CONTROL",dim,9,2);
        M(300,29,"OUTPUT SET");M(370,29,k[0].value,blue);R(300,36,190,16,metal,dim);R(303,39,184,10,WELL);R(305,41,180*a,6,blue);
        rail(13,25,94,141);R(17,29,86,133,WELL);for(int i=0;i<8;i++){float yy=40+i*16;uint32_t col=sel==i+1&&on?green:dim;M(24,yy+3,"T"+std::to_string(i+1),col);port(86,yy,col);if(sel==i+1&&on){Path p={{92,yy},{116,yy},{127,yy+(92-yy)*.22f},{127,91},{153,91}};D(p,WELL,5);D(p,blue,2.4f);}}
        M(122,28,"IN");port(140,43,blue);D({{146,43},{291,43},{291,56}},blue,2.2f);
        rail(153,61,105,66);R(158,66,95,56,WELL);M(166,79,"DETECT");screw(247,73);port(160,91,on?green:dim);
        L(172,105,244,105,GRID);R(172,99,72*c,6,al(violet,.65f));L(172+72*c,95,172+72*c,109,violet,2);M(166,118,num("%.0f dB",th),violet);
        D({{258,92},{271,92},{271,112},{285,112}},blue,2.2f);arrow(272,112,285,112,blue,1.4f);
        rail(285,56,129,111);R(290,61,119,101,WELL);M(299,74,"GAIN CELL");M(400,74,on?"SC ON":"SC OFF",on?green:dim,9,2);
        const float x=299,y=82,ww=101,hh=52,q=(th+60)/60,kx=x+ww*q,ky=y+hh*(1-q);R(x,y,ww,hh,0,GRID,.8f);L(x,y+hh,x+ww,y,dim,.7f);L(kx,y,kx,y+hh,violet,.9f);D({{x,y+hh},{kx,ky},{x+ww,ky-(1-q)*hh/(on?ratio:1.f)}},ink,1.8f);O(kx,ky,2.6f,WELL,violet,1.4f);M(299,148,"R "+num("%.1f:1",ratio),amber);M(400,148,"TH "+num("%.0f dB",th),violet,9,2);
        D({{414,103},{436,103},{436,62},{462,62}},blue,2.2f);D({{436,103},{436,140},{462,140}},blue,2.2f);port(468,62,blue);port(468,140,blue);M(482,65,"L",dim,10);M(482,143,"R",dim,10);M(437,175,"OUT / SET");break;}
    case P::P_ARP:{
        int rate=ts.arpRate,pat=ts.arpPatt,oct=ts.arpOct+1,count=rate==0?0:std::min(24,3+rate*2),levels=std::max(2,oct*3);
        M(15,15,"PITCH / EVENT ORDER");M(505,15,k[0].value,blue,9,2);
        for(int i=0;i<=4;i++){float y=151-i*27;L(25,y,492,y,GRID,.8f);L(25,y,38,y-9,dim,.8f);M(9,y+3,std::to_string(i),dim,8);}
        float step=467.f/std::max(1,count);std::vector<int> pitch;for(int i=0;i<count;i++){int j=pat==0?i:pat==1?count-1-i:pat==2?std::min(i,count-1-i):pat==3?(i%2==0?i/2:count-1-(i-1)/2):int(std::lround(rnd(i+31)*count));pitch.push_back(j%levels);}
        if(count){int low=*std::min_element(pitch.begin(),pitch.end()),high=*std::max_element(pitch.begin(),pitch.end());for(int i=0;i<count;i++){float lev=float(pitch[i])/(levels-1),height=10+lev*oct*26,x=25+i*step+2,y=151-height,ww=std::max(4.f,(step-6)*(.18f+.82f*d));uint32_t col=pitch[i]==low?blue:pitch[i]==high?violet:dim;
            shape({{x,y},{x+6,y-6},{x+ww+6,y-6},{x+ww,y}},al(col,.8f),col,.8f);shape({{x+ww,y},{x+ww+6,y-6},{x+ww+6,145},{x+ww,151}},metal,col,.8f);R(x,y,ww,height,al(col,.3f),col,.9f);L(x+2,y+3,x+ww-2,y+3,al(ink,.55f),.8f);if(st_.loopSamples>0&&i==std::min(count-1,int(loopPos()*count)))R(x,154,ww,2,ink);}}
        else M(260,90,"ARPEGGIATOR OFF",dim,11,1);L(25,160,492,160,lip,.8f);ticks(25,166,59);M(505,181,std::to_string(oct)+" OCT / "+k[3].value+" GATE",dim,9,2);break;}
    case P::P_EFILTER:{
        float x=28,y=23,ww=332,hh=126,cut=x+b*ww;int mode=ts.echoInsert.filter;
        M(x,13,"RESPONSE / SCHEMATIC");M(504,13,"TRANSFER",dim,9,2);
        for(int j=0;j<5;j++){float yy=y+j*hh/4;L(x,yy,x+ww,yy,GRID,.6f);if(j==0||j==4)M(23,yy+3,j==0?"MAX":"MIN",dim,8,2);}
        auto f=[&](float q){float z=(q-b)*14,low=1/(1+std::exp(z)),band=std::exp(-z*z*.22f),v=mode==0?low:mode==1?band:1-low;return std::clamp(.08f+.73f*v+.24f*c*band,.02f,.98f);};Path p;
        for(int j=0;j<=120;j++){float q=j/120.f;p.push_back({x+ww*q,y+hh*(1-f(q))});}
        for(size_t j=1;j<p.size();j++)quad({p[j-1].first,y+hh},p[j-1],p[j],{p[j].first,y+hh},al(dim,.17f));
        // Reference response silhouette, not a fake spectrum.
        for(int j=0;j<47;j++){float q=(j+.5f)/47,h=hh*f(q);R(x+q*ww-2.2f,y+hh-h,4.4f,h,al(dim,.12f));}D(p,dim,2.2f);L(cut,y,cut,y+hh,green,1.4f);O(cut,y+hh*(1-f(b)),4,WELL,violet,2);M(cut,165,k[1].value,green,9,1);
        const char* freq[]={"30","253","2134","18000"};for(int j=0;j<4;j++)M(x+j*ww/3,177,freq[j],dim,8,j==3?2:0);
        L(373,19,373,169,lip);R(387,29,116,116,WELL,lip,.8f);L(387,87,503,87,GRID);L(445,29,445,145,GRID);L(387,145,503,29,dim,.8f);Path q;float g=1+6*d;for(int j=0;j<=80;j++){float v=j/80.f;q.push_back({387+116*v,87-std::tanh(g*(v*2-1))/std::sqrt(g)*116*.46f});}D(q,amber,2.7f);M(445,159,"DRIVE "+k[3].value,amber,9,1);break;}
    case P::P_ENV:{
        float at=3*a*a,de=b,re=5*d*d,total=at+de+re+.65f,scale=474/total,xa=24+at*scale,xd=xa+de*scale,xs=xd+.65f*scale,xe=xs+re*scale,sy=151-126*c;Path p={{24,151},{xa,25},{xd,sy},{xs,sy},{xe,151}};
        for(size_t j=1;j<p.size();j++)quad({p[j-1].first,151},p[j-1],p[j],{p[j].first,151},al(dim,.18f));D(p,ink,2.6f);for(size_t j=0;j<p.size();j++){L(p[j].first,p[j].second,p[j].first,151,GRID);R(p[j].first-3,p[j].second-3,6,6,WELL,encoderTrace(std::max(0,int(j)-1)),1.5f);}M(24,15,"AMPLITUDE");M(505,15,num("%.2f s incl. sustain preview",total),dim,9,2);L(24,151,498,151,dim);ticks(24,159,60);M(24,176,"A",blue);M(182,176,"D",green);M(335,176,"S",violet);M(498,176,"R",amber,9,2);break;}
    case P::P_LFO:{
        int n=1+int(std::lround(6*a)),shape=ts.lfoShape;M(20,15,"CONTROL SIGNAL");R(20,28,334,124,0,lip,.8f);L(20,90,354,90,dim,.8f);Path p;
        for(int j=0;j<=280;j++){float q=j/280.f*n,ph=q-std::floor(q),v=shape==1?(ph<.5f?4*ph-1:3-4*ph):shape==2?(ph<.5f?1.f:-1.f):shape==3?2*rnd(int(q)+90)-1:std::sin(tau*q);p.push_back({20+334.f*j/280,90-v*124*.45f*b});}
        for(size_t j=1;j<p.size();j++){
            quad({p[j-1].first,90},p[j-1],p[j],{p[j].first,90},al(dim,.15f));
        }
        D(p,blue,2.5f);
        // Anchor the first visible period on page entry or a rate change.
        // The audio thread counts wraps, so skipped UI frames cannot lose cycles.
        if(lfoSweepTrack_!=t || lfoSweepRate_!=a){
            lfoSweepTrack_=t;
            lfoSweepRate_=a;
            lfoSweepOrigin_=st_.lfoCycle[t];
        }
        const uint32_t elapsed=(st_.lfoCycle[t]+Session::kLfoCycleMod-lfoSweepOrigin_)%Session::kLfoCycleMod;
        const int activeCycle=int(elapsed%uint32_t(n));
        const float phase=clamp01(st_.lfoPhase[t]);
        const float live=std::clamp(st_.lfoValue[t],-1.f,1.f);
        const float hx=20+334.f*(activeCycle+phase)/n,hy=90-live*124*.45f*b;
        if(shape==3 && b>.001f){
            const float start=20+334.f*activeCycle/n,end=20+334.f*(activeCycle+1)/n;
            R(start,29,end-start,122,WELL);
            L(start,90,end,90,GRID,.8f);
            L(start,hy,end,hy,blue,2.5f);
        }
        L(hx,29,hx,151,al(amber,.65f),1.25f);
        O(hx,hy,4.4f,WELL,amber,1.7f);
        O(hx,hy,1.8f,amber);
        L(365,20,365,165,lip);port(382,94,blue);
        const char* names[]={"PITCH","FILTER","VOLUME","PAN"};for(int j=0;j<4;j++){float yy=34+j*39;bool sel=j==ts.lfoTarget;uint32_t col=sel?violet:dim;D({{388,94},{403.f+j*4,94},{403.f+j*4,yy},{440,yy}},al(col,sel?1.f:.25f),sel?2.7f:1.f);R(440,yy-11,64,22,metal,col);M(472,yy+3,names[j],col,9,1);}M(20,176,"LEVEL "+num("%+.0f%%",st_.lfoValue[t]*100),amber);M(354,176,"SHAPE: "+k[3].value,dim,9,2);break;}
    case P::P_KEY:{
        auto sca=app_.scale();M(15,15,"PITCH CLASS");M(505,15,"OCTAVE "+std::to_string(sca.octave),violet,9,2);uint16_t ghost=app_.chordGhostMask();std::vector<float> xs;
        for(int i=0;i<12;i++){float x=14+i*41;bool valid=SamplerApp::inScale(sca.scale,(i-sca.root+12)%12);uint32_t col=i==sca.root?amber:valid?blue:GRID;R(x,50,37,72,metal,lip,.9f);R(x+3,53,31,66,WELL);R(x+6,56,25,43,valid?al(dim,.35f):GRID);R(x+7,57,23,4,valid?al(ink,.45f):metal);R(x+6,105,25,10,i==sca.root?amber:valid?blue:GRID);M(x+18.5f,136,SamplerApp::noteName(i),col,9,1);if(ghost&(1u<<i)){port(x+18.5f,42,amber);L(x+18.5f,42,x+18.5f,50,amber,1.8f);xs.push_back(x+18.5f);}}
        if(xs.size()>1){L(xs.front(),29,xs.back(),29,amber,1.8f);for(float x:xs)L(x,29,x,42,amber,1.8f);}
        L(20,159,500,159,dim);for(int j=0;j<7;j++){float x=20+j*80;L(x,155,x,163,dim);M(x,177,std::to_string(j-3),dim,8,1);}R(20+(sca.octave+3)*80-3,153,6,12,violet);break;}
    case P::P_SAVE:{
        M(15,15,b>.5f?"SOURCE / SOUND":"SOURCE / MICROPHONE");M(505,15,a>.5f?"ARMED":"READY",a>.5f?TRACE_RED:dim,9,2);
        float th=clamp01(app_.micThreshold()*2),level=clamp01(st_.inputPeak*2);R(15,94-66*level,490,132*level,al(green,.4f));L(15,94-66*th,505,94-66*th,amber,2);L(15,94+66*th,505,94+66*th,amber,2);M(505,168,"TRIGGER "+k[3].value,amber,10,2);R(15,156,10,10,a>.5f?TRACE_RED:metal,dim);M(30,165,"LIVE INPUT LEVEL");break;}
    case P::P_LEN:case P::P_TEMPO:{
        int n=std::clamp(st_.loopBars,1,32);bool active=st_.loopSamples>0;M(15,15,active?std::to_string(n)+" BARS / "+num("%.2f s",st_.loopSamples/app_.session().sampleRate()):"NO LOOP");M(505,15,num("%.0f BPM",st_.bpm),dim,9,2);
        float pk[Session::kLoopBins];int m=app_.session().loopPeaks(t,pk,Session::kLoopBins);float mx=.08f;for(int i=0;i<m;i++)mx=std::max(mx,pk[i]);float step=488.f/n;
        for(int i=0;i<n;i++){float x=16+i*step;R(x,30,step-1,62,metal,lip,.6f);R(x+2,33,step-5,56,WELL);if(active&&st_.track_has_clip[t]&&m>0){std::vector<float> p;for(int j=i*m/n;j<(i+1)*m/n;j++)p.push_back(pk[j]/mx);wave(p,x+3,61,std::max(1.f,step-7),22,blue,.5f);}if(step>=18)M(x+3,27,std::to_string(i+1),dim,8);}
        L(16,104,504,104,dim,.8f);for(int i=0;i<n*4;i++)L(16+488.f*i/(n*4),104,16+488.f*i/(n*4),104+(i%4==0?11:5),dim,.8f);
        if(pg==P::P_TEMPO){R(16,128,488,12,metal,dim);R(16,128,120+368*(1-b),12,c>.5f?violet:amber);M(16,153,c>.5f?"STRETCH / PITCH PRESERVED":"TAPE / VARISPEED");M(504,153,num("%.2f s / beat",60/std::max(1.f,st_.bpm)),dim,9,2);}else{D({{16,124},{16,136},{504,136},{504,124}},blue,2);M(260,153,"DUPLICATE OR HALVE RECORDED STRIP",dim,10,1);}M(505,177,"LOOP MATERIAL",dim,9,2);break;}
    default:break;
    }
    endClip();
}

// The approved deck's exact design coordinates. Scale the draw ops and
// inverse-transform the touch callbacks together; do not stretch the artwork.
void SamplerUI::scaleStudy(float s,float ox,float oy,size_t first){
    auto x=[&](float v){return ox+s*v;};auto y=[&](float v){return oy+s*v;};
    for(size_t i=first;i<ops_.size();i++){auto& o=ops_[i];auto* f=o.f;
        switch(o.type){
        case UiOp::RRECT:case UiOp::STROKE:f[0]=x(f[0]);f[1]=y(f[1]);for(int j=2;j<6;j++)f[j]*=s;break;
        case UiOp::TEXT:f[0]=x(f[0]);f[1]=y(f[1]);f[2]*=s;break;
        case UiOp::LINE:f[0]=x(f[0]);f[1]=y(f[1]);f[2]=x(f[2]);f[3]=y(f[3]);f[4]*=s;break;
        case UiOp::CIRCLE:f[0]=x(f[0]);f[1]=y(f[1]);f[2]*=s;break;
        case UiOp::TRI:for(int j=0;j<6;j+=2){f[j]=x(f[j]);f[j+1]=y(f[j+1]);}break;
        case UiOp::CLIP:f[0]=x(f[0]);f[1]=y(f[1]);f[2]*=s;f[3]*=s;break;
        case UiOp::ENDCLIP:break;
        }}
    for(auto& h:hits_){h.r={x(h.r.x),y(h.r.y),s*h.r.w,s*h.r.h};
        auto wrap=[=](std::function<void(float,float)> f)->std::function<void(float,float)>{if(!f)return {};return [=](float x,float y){f((x-ox)/s,(y-oy)/s);};};
        h.down=wrap(h.down);h.move=wrap(h.move);}
}
void SamplerUI::studyDeck(){
    using P=SamplerApp;
    const int t=app_.track();const auto pg=app_.page();const auto k=app_.knobs(pg);
    const auto tc=trackColor(t);const bool rec=st_.freeRecording||st_.track_clip_rec[t],has=app_.loopExists()&&st_.loopSamples>0;
    const int mic=app_.micState();
    rrect({0,0,960,450},0,DECK_BG);
    bevel({20,13,32,38},tc);text(36,32,22,contrastInk(tc),std::to_string(t+1),1,true);tap({20,13,32,38},[this,t]{app_.selectTrack((t+1)%P::kTracks);});
    Rect sound{55,13,280,38};bevel(sound,RAISED);std::string name=app_.sampleLabel(t);if(name.empty())name="Empty";if(mic)name=mic==1?"Listening":mic==2?"Recording":"Saving";text(63,32,18,TEXT,fit(name,250,18),0,true);tri(320,29,328,29,324,34,TEXT);tap(sound,[this]{sheetOpen_=true;sheetScroll_=std::max(0.f,float(app_.libraryCursor())*34-100);app_.refreshLibrary();});
    bevel({20,55,315,23},WELL,true);auto clock=[](float v){char b[24];snprintf(b,sizeof b,"%d:%02d",int(v)/60,int(v)%60);return std::string(b);};
    float sec=has?float(st_.loopSamples/app_.session().sampleRate()):0;
    std::string status=has?std::to_string(st_.loopBars)+"B   "+num("%.0f BPM",st_.bpm)+"   "+clock(loopPos()*sec)+" / "+clock(sec):rec?"RECORDING":"NO LOOP";
    text(28,67,12,TRACE_AMBER,fit(status,298,12));
    scope({20,84,315,163});cassette({21,85,313,161});
    for(int i=0;i<4;i++){Rect z{20+i*80.f,253,75,38};bool active=i==0?st_.playing:i==2?rec:false;uint32_t col=i==2||i==3?RED:TEXT;bevel(z,active?(i==2?RED:GREEN):SURFACE,active);uint32_t fg=active?contrastInk(i==2?RED:GREEN):col;float x=z.x+15,y=z.y+19;
        if(i==0)tri(x-4,y-6,x-4,y+6,x+6,y,fg);else if(i==1)rrect({x-5,y-5,10,10},0,fg);else if(i==2)circle(x,y,5.5f,active?fg:RED);else {line(x-5,y-5,x+5,y+5,fg,2);line(x-5,y+5,x+5,y-5,fg,2);}
        const char* names[]={"PLAY","STOP","REC","CLEAR"};text(z.x+27,y,10,fg,names[i],0,true);tap(z,[this,i]{if(i==0)app_.transportSet(true);else if(i==1)app_.transportSet(false);else if(i==2)app_.loopRecord();else app_.loopClearTrack();});}
    line(20,299,335,299,OUTLINE);rrect({21,308,7,7},0,linkMidi_?GREEN:DIM);text(32,312,10,DIM,fit(app_.toast().empty()?(linkMidi_?"FM-1 LINK":"LOOP READY"):app_.toast(),230,10));button({281,302,54,21},theme::light?"DARK":"LIGHT",false,[this]{setLight(!theme::light);},10);
    const char* tools[]={"GRAIN","MIC","FILE","PROJ","EDIT"};for(int i=0;i<5;i++){float yy=13+i*52.f+(i==4?16.f:0.f);Rect z{349,yy,54,46};bevel(z,SURFACE);if(i==4){line(368,yy+10,385,yy+10,TEXT,2);line(368,yy+16,385,yy+16,TEXT,2);line(368,yy+22,381,yy+22,TEXT,2);}else icon(4+i,376,yy+16,i==0?VIOLET:i==1?RED:ACCENT);text(376,yy+36,9,TEXT,tools[i],1,true);
        tap(z,[this,i]{if(i==0){if(app_.algo()!=4){app_.say("SELECT GRANULAR FIRST");return;}app_.gotoPage(app_.page()==P::P_ECHO?P::P_GRAIN:app_.page()==P::P_GRAIN?P::P_MOTION:P::P_ECHO);}else if(i==1)app_.micSample();else if(i==2)app_.requestFile();else if(i==3)projectOpen_=true;else app_.setEditor(true);});}
    line(349,226,403,226,OUTLINE);
    const uint32_t pageAccent=pageColor(pg);
    bevel({418,13,522,23},SURFACE);rrect({426,20,6,10},0,pageAccent);text(439,25,12,pageAccent,P::pageName(pg),0,true);
    Rect alg{790,16,146,17};bevel(alg,RAISED);text(795,24.5f,10,TEXT,fit(P::algoName(app_.algo()),126,10));tri(925,22,931,22,928,26,TEXT);tap(alg,[this]{app_.setAlgo((app_.algo()+1)%6);});
    // Keep the existing tuning action, beside the sample algorithm.
    if(app_.algo()<=1&&pg==P::P_HOME)button({754,16,32,17},"TUNE",app_.tuned(t),[this]{app_.autoTune();},8);
    bevel({418,39,522,188},WELL,true);
    if(mic)sampleView({419,40,520,186});else studyPhone({419,40,520,186},pg);
    const int selected=activeMenu(pg);const auto& group=kMenus[selected];
    const uint32_t family=pageColor(group.pages[0]);
    for(int i=0;i<kMenuCount;i++){
        Rect z{418+i*52.5f,232,49.5f,24};bool on=i==selected;
        const uint32_t col=pageColor(kMenus[i].pages[0]);
        bevel(z,on?col:SURFACE,on);
        text(z.x+z.w/2,244,10,on?contrastInk(col):col,fit(kMenus[i].editorLabel,z.w-3,10),1,true);
        tap(z,[this,i]{app_.gotoPage(nextMenuPage(app_,kMenus[i]));});
    }
    line(418,283,940,283,OUTLINE);
    const std::string caption=std::string(group.editorLabel)+" / "+P::pageName(pg);
    text(418,272,10,family,fit(caption,126,10));
    const int count=childCount(app_,group);
    if(count>1){
        const bool leftward=group.pages[0]==P::P_LEN;
        const float parentX=418+selected*52.5f+24.75f;
        // The caption occupies x=418..544. SAMPLE and FX children
        // start just beyond it; all other groups retain their existing anchor.
        const float childAnchor=(!leftward&&selected<2)?std::max(552.f,parentX):parentX;
        const float room=leftward?childAnchor-418:940-childAnchor;
        const float gap=5.f,railY=259.f;
        const float childW=std::min(64.f,(room-gap*(count-1))/count);
        struct StudyChild{P::Page page;Rect rect;std::string label;};
        std::vector<StudyChild> children;
        for(int j=0;j<group.count;j++){
            auto p=group.pages[j];
            if(!app_.pageAvailable(p))continue;
            const float offset=float(children.size())*(childW+gap);
            const float x=leftward?childAnchor-offset-childW:childAnchor+offset;
            children.push_back({p,{x,262,childW,19},group.labels[j]});
        }
        // The stem and rail are always in the parent family color.
        float lo=parentX,hi=parentX;
        for(const auto& child:children){
            const float cx=child.rect.x+child.rect.w/2;
            lo=std::min(lo,cx);hi=std::max(hi,cx);
        }
        line(parentX,256,parentX,railY,family,1.5f);
        line(lo,railY,hi,railY,family,1.5f);
        for(const auto& child:children){
            const auto z=child.rect;const auto p=child.page;
            const float cx=z.x+z.w/2;const bool on=p==pg;
            const uint32_t col=pageColor(p);
            line(cx,railY,cx,z.y,family,1.5f);
            bevel(z,on?col:SURFACE,on);
            text(cx,271.5f,10,on?contrastInk(col):col,fit(child.label,z.w-4,10),1,on);
            tap(z,[this,p]{app_.gotoPage(p);});
        }
    }
    // Single-page groups (and SAMPLE in non-granular modes) have only the caption.
    for(int i=0;i<4;i++){float x=418+i*132.25f;const auto& p=k[i];Rect bar{x,305,125.25f,22};uint32_t col=encoderColor(i);if(!p.valid){bevel(bar,TRACK,true);continue;}
        float valueWidth=std::min(65.f,float(p.value.size())*6.1f+4);text(x,295,10,TEXT,fit(p.name,bar.w-valueWidth-3,10),0,true);text(x+bar.w,295,10,DIM,fit(p.value,valueWidth,10),2);
        bevel(bar,TRACK,true);const bool action=pg==P::P_LEN&&(i==1||i==2);if(action){bevel(bar,SURFACE);text(x+bar.w/2,316,10,TEXT,i==1?"X2":"/2",1,true);tap(bar,[this,pg,i]{auto q=app_.knobs(pg)[i];if(q.step)q.step(1);});continue;}
        float v=clamp01(p.norm),ww=bar.w-4;if(v>0){rrect({x+2,307,ww*v,18},0,col);line(x+2,307,x+2+ww*v,307,withAlpha(HI,92));line(x+2,324,x+2+ww*v,324,withAlpha(SHADOW,120));}
        for(int j=1;j<=4;j++)line(x+ww*j/4,320,x+ww*j/4,325,withAlpha(HI,140));rrect({x+std::clamp(2+ww*v,2.f,bar.w-4),307,2,18},0,RAISED);
        if(pg==P::P_LEN&&i==3)continue;
        if(pg==P::P_SAVE&&i==0){tap(bar,[this]{app_.micSample();});continue;}
        auto set=[this,t,pg,i,bar](float px,float){if(app_.track()!=t||app_.page()!=pg)return;auto q=app_.knobs(pg)[i];if(q.set)q.set(clamp01((px-bar.x-2)/(bar.w-4)));};
        if(pg==P::P_LEN&&i==0)hit(bar,set);else hit(bar,set,set);}
    studyTimeline({20,337,920,101});
}

// R10 drawTimeline: continuous nested envelopes, real stored loop peaks.
// An unsigned peak overview cannot supply the study's synthetic signed centre trace.
void SamplerUI::studyTimeline(Rect r) {
    const int t=app_.track();const bool has=app_.loopExists()&&st_.loopSamples>0,rec=st_.freeRecording||st_.track_clip_rec[t];
    bevel(r,WELL,true);
    float x=r.x+9,y=r.y+55,ww=r.w-18;
    float sec=has?float(st_.loopSamples/app_.session().sampleRate()):st_.freeRecording?st_.freeSeconds:0.f;
    auto clock=[](float v){char b[24];snprintf(b,sizeof b,"%d:%02d",int(v)/60,int(v)%60);return std::string(b);};
    const int bars=std::max(1,st_.loopBars);
    if(has) {
        for(int i=0;i<=bars;i++){float xx=x+ww*i/bars;line(xx,r.y+18,xx,r.y+93,GRID,i==0?1.2f:.8f);if(i<bars&&(bars<=16||i%2==0))text(xx+3,r.y+10,9,WELL_DIM,(i<9?"0":"")+std::to_string(i+1));}
        for(int i=0;i<=bars*4;i++){float xx=x+ww*i/(bars*4);line(xx,r.y+17,xx,r.y+23+(i%4==0?3:0),WELL_DIM,.8f);}
    }
    line(x,y,x+ww,y,GRID);line(x,r.y+28,x+ww,r.y+28,GRID,.7f);line(x,r.y+82,x+ww,r.y+82,GRID,.7f);
    std::vector<float> p;float fraction=1.f;
    if(has&&st_.track_has_clip[t]){float pk[Session::kLoopBins];int n=app_.session().loopPeaks(t,pk,Session::kLoopBins);p.assign(pk,pk+std::max(0,n));}
    else if(st_.freeRecording){static float fp[18500];int n=app_.session().freePeaks(fp,18500);int bins=std::min(450,std::max(1,n));p.assign(bins,0.f);for(int i=0;i<n;i++)p[std::min(bins-1,i*bins/std::max(1,n))]=std::max(p[std::min(bins-1,i*bins/std::max(1,n))],fp[i]);fraction=std::min(1.f,n*.01f/std::max(1.f,sec));}
    if(p.size()>1){float mx=.08f;for(float v:p)mx=std::max(mx,v);uint32_t col=rec?TRACE_RED:TRACE_BLUE;
        const float scales[]={1,.68f,.34f},alphas[]={.19f,.24f,.34f};
        for(int layer=0;layer<3;layer++){float h=30*scales[layer];uint32_t color=withAlpha(col,int(255*alphas[layer]));for(size_t j=1;j<p.size();j++){float xa=x+ww*fraction*(j-1)/(p.size()-1),xb=x+ww*fraction*j/(p.size()-1),va=h*clamp01(p[j-1]/mx),vb=h*clamp01(p[j]/mx);tri(xa,y-va,xb,y-vb,xb,y+vb,color);tri(xa,y-va,xb,y+vb,xa,y+va,color);}}
        // Preserve the centre of the envelope, without fabricating signed PCM.
        line(x,y,x+ww*fraction,y,withAlpha(WELL_TEXT,100),.8f);
    }else text(r.x+r.w/2,y,12,WELL_DIM,st_.freeArmed?"WAITING FOR THE FIRST SOUND":has?"TRACK HAS NO LOOP":"NO LOOP",1);
    if(has||st_.freeRecording){float xx=x+ww*(has?clamp01(loopPos()):fraction);uint32_t col=rec?TRACE_RED:TRACE_AMBER;rrect({xx-3,r.y+17,6,76},0,withAlpha(col,28));line(xx,r.y+16,xx,r.y+94,col,1.7f);tri(xx-4,r.y+15,xx+4,r.y+15,xx,r.y+21,col);}
    text(x,r.y+94,9,WELL_DIM,"0:00");text(x+ww,r.y+94,9,WELL_DIM,clock(sec),2);
    if(has&&!app_.recordingNow())hit({x,r.y,ww,r.h},[this,x,ww](float px,float){app_.scrub(0,clamp01((px-x)/ww));},[this,x,ww](float px,float){app_.scrub(1,clamp01((px-x)/ww));},[this]{app_.scrub(2,0);});
}
