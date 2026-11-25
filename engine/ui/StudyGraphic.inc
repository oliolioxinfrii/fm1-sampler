// Compact pictographs transcribed independently from the R10 96x48 SVG
// library. Fit, rather than stretch, into the actual 216x35 FM-1 plot.
// Parameter readouts/meters are drawn separately by SamplerApp.
bool SamplerUI::studyGraphic(Rect w,SamplerApp::Page pg,bool) {
    using P=SamplerApp;using Pt=std::pair<float,float>;using Path=std::vector<Pt>;
    const int t=app_.track();const auto& ts=app_.state(t);const auto k=app_.knobs(pg);
    const float a=clamp01(k[0].norm),b=clamp01(k[1].norm),c=clamp01(k[2].norm),d=clamp01(k[3].norm);
    const float s=std::min(w.w/96.f,w.h/48.f),ox=w.x+(w.w-96*s)/2,oy=w.y+(w.h-48*s)/2;
    const uint32_t B=TRACE_BLUE,V=TRACE_VIOLET,A=TRACE_AMBER,G=TRACE_GREEN,D=WELL_DIM,F=GRID,W=WELL_TEXT;
    auto X=[&](float x){return ox+x*s;};auto Y=[&](float y){return oy+y*s;};
    auto L=[&](float x,float y,float xx,float yy,uint32_t col,float thick=1.6f){line(X(x),Y(y),X(xx),Y(yy),col,std::max(2.f,thick*s));};
    auto path=[&](const Path& q,uint32_t col,float thick=2.f){for(size_t i=1;i<q.size();i++)L(q[i-1].first,q[i-1].second,q[i].first,q[i].second,col,thick);};
    auto rect=[&](float x,float y,float ww,float hh,uint32_t col,uint32_t fill=0,float sw=1.5f){if(ww<=0||hh<=0)return;Rect z{X(x),Y(y),ww*s,hh*s};if(fill)rrect(z,0,fill);stroke(z,0,col,std::max(2.f,sw*s));};
    auto ring=[&](float x,float y,float rr,uint32_t col,uint32_t fill=0,float sw=1.5f){if(fill)circle(X(x),Y(y),rr*s,fill);Path q;for(int i=0;i<=20;i++){float ph=2*kPi*i/20;q.push_back({x+rr*std::cos(ph),y+rr*std::sin(ph)});}path(q,col,sw);};
    auto jack=[&](float x,float y,uint32_t col){ring(x,y,2.4f,col,WELL);};
    auto grid=[&](float x,float y,float ww,float hh,int n,int m){for(int i=0;i<=n;i++)L(x+ww*i/n,y,x+ww*i/n,y+hh,F,1);for(int j=0;j<=m;j++)L(x,y+hh*j/m,x+ww,y+hh*j/m,F,1);};
    auto cubic=[&](Pt p,Pt q,Pt r,Pt z,uint32_t col,float sw=1.6f){Path out;for(int i=0;i<=20;i++){float v=i/20.f,u=1-v;out.push_back({u*u*u*p.first+3*u*u*v*q.first+3*u*v*v*r.first+v*v*v*z.first,u*u*u*p.second+3*u*u*v*q.second+3*u*v*v*r.second+v*v*v*z.second});}path(out,col,sw);};
    auto arrow=[&](float x,float y,float xx,float yy,uint32_t col){L(x,y,xx,yy,col);float q=std::atan2(yy-y,xx-x);path({{xx-4*std::cos(q-.65f),yy-4*std::sin(q-.65f)},{xx,yy},{xx-4*std::cos(q+.65f),yy-4*std::sin(q+.65f)}},col,1.6f);};
    auto wave=[&](float x,float y,float ww,float hh,const std::vector<float>& p){if(p.empty()){L(x,y,x+ww,y,F);return;}Path top,bot;for(size_t i=0;i<p.size();i++){float xx=x+ww*i/std::max<size_t>(1,p.size()-1),v=hh*clamp01(p[i]);top.push_back({xx,y-v});bot.push_back({xx,y+v});}path(top,B,1.3f);path(bot,B,1.3f);};
    auto mix=[](uint32_t x,uint32_t y,float v){uint32_t o=0xff000000u;v=clamp01(v);for(int sh=0;sh<=16;sh+=8)o|=uint32_t(std::lround(float(x>>sh&255)*(1-v)+float(y>>sh&255)*v))<<sh;return o;};
    switch(pg){
    case P::P_ECHO:
        for(int j=0;j<4;j++){float dx=(j==0?0:j==1?5:j==2?-3:8)*b*1.8f,y=7+j*10.f+(j-1.5f)*c;uint32_t col=j==0?D:j==3?V:B;float ww=62+6*d*(j-1);rect(16+dx,y,ww,6,col);int n=3+int(a*4);for(int i=1;i<n;i++)L(16+dx+ww*i/n,y,16+dx+ww*i/n,y+6,col,1.2f);}return true;
    case P::P_GRAIN:{
        L(7,36,89,36,D);int count=3+int(b*4);float width=16+15*a;
        for(int j=0;j<count;j++){float x=8+(80-width)*j/std::max(1,count-1);uint32_t col=j%3==0?D:j%3==2?V:B;Path q;for(int i=0;i<=24;i++){float z=i/24.f;float v=0.5f*(1-std::cos(2*kPi*z));if(ts.echo.window==2)v=std::min({1.f,z/.15f,(1-z)/.15f});if(ts.echo.window==3||ts.echo.window==4){float u=ts.echo.window==4?1-z:z;v=u<.03f?std::sin(.5f*kPi*u/.03f):std::exp(-5*(u-.03f)/.97f);if(u>.92f)v*=.5f+.5f*std::cos(kPi*(u-.92f)/.08f);}if(ts.echo.window==5){float u=(z-.5f)/.16f;v=clamp01((std::exp(-.5f*u*u)-.00752f)/(1-.00752f));}q.push_back({x+width*z,36-26*v});}path(q,col);}
        if(d>.5f)L(7,43,89,43,A,2);return true;}
    case P::P_MOTION:{
        rect(7,7,82,34,D);L(7,24,89,24,F,1);float travel=10+24*std::abs(2*a-1),sign=a<.5f?-1.f:1.f,ex=48+sign*travel,ey=12+9*c;
        cubic({13,24},{30,9},{47,10},{ex,ey},B,2);arrow(ex-sign*5,ey+4,ex,ey,B);
        cubic({13,24},{32,37},{52,38},{ex,31},B,2);arrow(ex-sign*5,31,ex,31,B);
        if(b>.01f){cubic({73,15},{57,26},{44,31},{17,33},V,1.5f+1.3f*b);arrow(23,31,17,33,V);}
        if(d>.5f)rect(7,7,82,34,A);return true;}
    case P::P_EFILTER:{
        grid(7,7,82,34,4,2);Path p;for(int i=0;i<=36;i++){float q=i/36.f,z=(q-b)*14,low=1/(1+std::exp(z)),band=std::exp(-z*z*.22f),v=ts.echoInsert.filter==0?low:ts.echoInsert.filter==1?band:1-low;p.push_back({8+80*q,40-32*clamp01(.08f+.73f*v+.24f*c*band)});}path(p,A,2.2f);L(8+80*b,6,8+80*b,41,D,1.3f);return true;}
    case P::P_EDELAY:{
        L(9,13,88,13,F,1);L(9,31,88,31,F,1);
        float step=14+10*a;int count=std::min(5,1+int(70/step));uint32_t fb=mix(D,G,b);
        for(int i=0;i<count;i++){float x=12+i*step,y=d>.5f?(i%2?26.f:8.f):17.f;rect(x,y,11,10,mix(D,V,i==0?.25f+.75f*c:c*std::pow(.86f,float(i))),0,1.8f);if(i<count-1)arrow(x+11,y+5,x+step,d>.5f?(i%2?13.f:31.f):22.f,fb);}
        float end=12+(count-1)*step+5.5f,yy=d>.5f?((count-1)%2?36.f:18.f):27.f;path({{end,yy},{end,43},{9,43},{9,d>.5f?13.f:22.f}},fb,1.6f);arrow(9,18,9,13,fb);return true;}
    case P::P_EREVERB:{
        for(int j=0;j<4;j++){float y=10+9*j;Path q;for(int i=0;i<=40;i++){float u=i/40.f;q.push_back({7+82*u,y+5*b*std::sin(4*kPi*u+j*.5f)});}path(q,mix(D,B,.25f+.75f*a),1.6f);}
        for(int i=0;i<5;i++){float x=18+16*i;uint32_t col=i%2==0?mix(D,V,c):i==1?mix(D,A,d):D;Path q;for(int j=0;j<=24;j++){float u=j/24.f;q.push_back({x+6*b*std::sin(2*kPi*u+i*.4f),5+38*u});}path(q,col,1.4f);}return true;}
    case P::P_ETAPE:{
        Path top,bot;for(int i=0;i<=48;i++){float q=i/48.f,xx=7+82*q,y=28-6*a*std::sin(2*kPi*q)+2*b*std::sin(10*kPi*q);top.push_back({xx,y});bot.push_back({xx,y+11});if(i>0){auto p=top[i-1],z=bot[i-1];tri(X(p.first),Y(p.second),X(xx),Y(y),X(xx),Y(y+11),F);tri(X(p.first),Y(p.second),X(xx),Y(y+11),X(z.first),Y(z.second),F);}}
        path(top,A,2.4f);path(bot,B,1.7f);float head=28-6*a*std::sin(2*kPi*43/82.f)+2*b*std::sin(10*kPi*43/82.f);
        tri(X(40),Y(6),X(60),Y(6),X(57),Y(head),F);tri(X(40),Y(6),X(57),Y(head),X(43),Y(head),F);path({{40,6},{60,6},{57,head},{43,head},{40,6}},D);L(43,head,57,head,V,2.3f);for(int i=0;i<4+int(d*9);i++){float x=14+(i*17%68),y=top[int((x-7)*48/82)].second+4;L(x,y,x+3,y-1,mix(W,V,c),1.4f);}return true;}
    case P::P_EMIX:{
        jack(5,24,B);L(8,24,12,24,B);rect(12,15,12,18,V,0,1.8f);L(16,19,20,29,mix(D,V,c));L(24,24,29,24,B);rect(29,15,12,18,A,0,1.8f);path({{31,21},{35,21},{39,28}},mix(D,A,d),1.5f);L(41,24,46,24,B);
        rect(46,6,39,36,D);rect(50,10,31,28,D,0,1);L(65.5f,12,65.5f,36,F);L(52,24,79,24,F);
        float m[Session::kScopePoints],side[Session::kScopePoints];int n=app_.session().insertScope(t,m,side,Session::kScopePoints);
        for(int i=1;i<n;i++){float q=float(i)/(n-1),v=.5f+.5f*std::sin(2*kPi*q*6),u=.5f+.5f*std::sin(2*kPi*(q*6+.34f));L(65.5f+12*side[i-1],24-12*m[i-1],65.5f+12*side[i],24-12*m[i],mix(mix(B,V,c*std::pow(v,1.35f)),A,d*std::pow(1-u,1.35f)),1.4f);}
        L(85,17,91,17,B);L(85,31,91,31,B);jack(92,17,B);jack(92,31,B);return true;}
    case P::P_ENV:{
        grid(8,7,80,34,4,2);float total=3*a*a+b+.65f+5*d*d,xa=8+80*3*a*a/total,xd=xa+80*b/total,xs=xd+80*.65f/total,yy=41-33*c;path({{8,41},{xa,8},{xd,yy},{xs,yy},{88,41}},B,2.4f);ring(xa,8,2.3f,W,WELL);ring(xd,yy,2.3f,W,WELL);ring(xs,yy,2.3f,W,WELL);return true;}
    case P::P_LFO:{
        L(7,24,66,24,F);Path q;for(int i=0;i<=48;i++){float z=i/48.f*(1+int(a*4)),ph=z-std::floor(z);float v=ts.lfoShape==1?1-4*std::abs(ph-.5f):ts.lfoShape==2?(ph<.5f?1.f:-1.f):ts.lfoShape==3?std::sin(int(z)*3.1f):std::sin(2*kPi*z);q.push_back({7+59.f*i/48,24-13*b*v});}path(q,B);float y=8+9.3f*ts.lfoTarget;path({{66,24},{75,24},{75,y},{84,y}},B,1.6f);for(int i=0;i<4;i++)jack(87,8+9.3f*i,i==ts.lfoTarget?B:D);return true;}
    case P::P_ARP:{
        grid(8,7,80,34,6,3);int count=ts.arpRate?7:0,oct=ts.arpOct+1;for(int i=0;i<count;i++){int idx=ts.arpPatt==0?i:ts.arpPatt==1?6-i:ts.arpPatt==2?std::min(i,6-i):ts.arpPatt==3?(i%2?6-i/2:i/2):(i*5)%7;float y=34-27.f*(idx%(oct+2))/std::max(1,oct+1);uint32_t col=y<13?V:y>28?B:D;rect(9+i*11,y+1,4+5*d,6,col,col,1);}if(count&&st_.loopSamples>0){float x=9+11*std::min(6,int(loopPos()*7));L(x,43,x+8,43,G,2.2f);}return true;}
    case P::P_MIX:{
        int src=ts.scSrc-1;bool on=src>=0&&src<8&&src!=t;for(int i=0;i<8;i++)jack(8,7+i*4.8f,on&&i==src?G:D);
        if(on)path({{11,7+src*4.8f},{25,7+src*4.8f},{25,24},{35,24}},G,1.8f);rect(35,17,15,14,on?G:D);path({{39,27},{43,21},{46,24}},on?G:D,1.5f);L(50,24,56,24,on?G:D);rect(56,7,28,34,D);float q=(20+40*c)/60,kx=60+20*q,ky=36-20*q;path({{60,36},{kx,ky},{80,ky-20*(1-q)/(on?1+19*d*d:1.f)}},V,2);L(84,17,91,17,B);L(84,31,91,31,B);jack(92,17,B);jack(92,31,B);return true;}
    case P::P_KEY:{
        auto scale=app_.scale();auto mask=app_.chordGhostMask();std::vector<float> xs;for(int i=0;i<12;i++){float x=7+7*i;bool valid=P::inScale(scale.scale,(i-scale.root+12)%12);rect(x,13,5,23,valid?B:D,i==scale.root?B:0,1.4f);if(mask&(1u<<i))xs.push_back(x+2);}
        if(!xs.empty()){L(xs.front(),6,xs.back(),6,V);for(float x:xs)L(x,6,x,10,V);}return true;}
    case P::P_SAVE:{
        float level=clamp01(st_.inputPeak*2),th=clamp01(app_.micThreshold()*2);L(7,24,73,24,B);if(level>0)rect(7,24-15*level,66,30*level,B,0,1.2f);L(7,24-15*th,73,24-15*th,A);L(7,24+15*th,73,24+15*th,A);path({{76,13},{76,35},{88,35},{88,13},{76,13}},D);rect(79,32-16*level,6,16*level,G,G,1);ring(13,7,3,a>.5f?TRACE_RED:V,a>.5f?TRACE_RED:0);return true;}
    case P::P_LEN:{
        rect(8,12,39,24,D);rect(49,12,39,24,B);float pk[Session::kLoopBins];int n=app_.session().loopPeaks(t,pk,Session::kLoopBins);std::vector<float> p,q;if(n>0&&st_.track_has_clip[t]){float mx=.08f;for(int i=0;i<n;i++)mx=std::max(mx,pk[i]);for(int j=0;j<20;j++){p.push_back(pk[j*n/40]/mx);q.push_back(pk[(j+20)*n/40]/mx);}}wave(10,24,35,8,p);wave(51,24,35,8,q);path({{8,8},{8,5},{88,5},{88,8}},W);L(40,41,56,41,G);L(48,38,48,44,G);return true;}
    case P::P_TEMPO:
        path({{8,9},{88,9}},D);L(8,5,8,14,D);L(88,5,88,14,D);for(int i=0;i<=8;i++)L(8+i*10,9,8+i*10,15,i%4==0?B:D,1.4f);rect(8,20,80,8,D);rect(8,33,35+45*(1-b),8,B,0,1.7f);path({{43+45*(1-b),30},{50+45*(1-b),37},{43+45*(1-b),44}},V);return true;
    default:return false;
    }
}
