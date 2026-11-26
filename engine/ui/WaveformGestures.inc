// Native touch-only precision editor. All coordinates are in layout units.
SamplerUI::WaveView& SamplerUI::waveView(){
    auto& v=waveViews_[app_.track()+(app_.generativeDeck()?SamplerApp::kTracks:0)];
    auto source=app_.session().sampleSource(app_.track(),app_.generativeDeck());
    if(v.source.lock()!=source){v.source=source;v.lo=0;v.span=1;}
    return v;
}
float SamplerUI::waveMinimum()const{
    auto source=app_.session().sampleSource(app_.track(),app_.generativeDeck());
    return source?std::min(1.f,64.f/std::max(64,source->frames)):.001f;
}
void SamplerUI::waveSet(float lo,float span){
    auto& v=waveView();v.span=std::clamp(span,waveMinimum(),1.f);v.lo=std::clamp(lo,0.f,1-v.span);
}
void SamplerUI::waveZoom(float factor,float center){
    auto& v=waveView();const float span=std::clamp(v.span*factor,waveMinimum(),1.f);
    waveSet(v.lo+(v.span-span)*center,span);
}
void SamplerUI::waveFit(){
    if(app_.generativeDeck()){waveSet(0,1);return;}
    auto t=app_.state(app_.track());const float span=std::clamp((t.end-t.start)*1.16f,waveMinimum(),1.f);
    waveSet((t.start+t.end-span)*.5f,span);
}
void SamplerUI::waveTools(Rect r,bool editable){
    if(r.h<14)return;
    r.h=std::min(28.f,r.h);
    auto& v=waveView();const int n=editable?4:3;const float w=(r.w-(n-1)*4)/n;
    button({r.x,r.y,w,r.h},"FULL",v.span>.9999f,[this]{waveSet(0,1);},10);
    if(editable)button({r.x+w+4,r.y,w,r.h},"FIT",false,[this]{waveFit();},10);
    button({r.x+(n-2)*(w+4),r.y,w,r.h},"ZOOM -",false,[this]{waveZoom(2);},10);
    button({r.x+(n-1)*(w+4),r.y,w,r.h},"ZOOM +",false,[this]{waveZoom(.5f);},10);
}
void SamplerUI::waveform(Rect r,bool editable){
    auto& v=waveView();const auto source=v.source.lock();
    waveRect_=r;waveTrack_=app_.track();
    if(!source||source->frames<=0){text(r.x+r.w*.5f,r.y+r.h*.5f,11,WELL_DIM,"NO SAMPLE",1);return;}
    const int t=app_.track();const auto ts=app_.state(t);
    const float left=r.x+2,w=r.w-4,cy=r.y+r.h*.5f,half=r.h*.5f-17;
    auto X=[&](float p){return left+(p-v.lo)*w/v.span;};
    auto in=[&](float p){return p>=v.lo&&p<=v.lo+v.span;};
    const int bins=std::max(20,int(w/1.4f));
    if(waveCacheSource_.lock()!=source||waveCacheLo_!=v.lo||waveCacheSpan_!=v.span||waveCacheBins_!=bins){
        waveCacheSource_=source;waveCacheLo_=v.lo;waveCacheSpan_=v.span;waveCacheBins_=bins;
        waveCache_.resize(bins);
        for(int i=0;i<bins;i++){
            int a=std::clamp(int((v.lo+v.span*i/bins)*source->frames),0,source->frames-1);
            int b=std::clamp(int(std::ceil((v.lo+v.span*(i+1)/bins)*source->frames)),a+1,source->frames);
            float mn=1.f,mx=-1.f;
            while(a<b){
                if(a%SampleBuf::waveBlock==0&&a+SampleBuf::waveBlock<=b&&!source->waveBounds.empty()){
                    const auto q=source->waveBounds[a/SampleBuf::waveBlock];
                    mn=std::min(mn,q[0]);mx=std::max(mx,q[1]);a+=SampleBuf::waveBlock;
                }else{
                    mn=std::min({mn,source->d[2*size_t(a)],source->d[2*size_t(a)+1]});
                    mx=std::max({mx,source->d[2*size_t(a)],source->d[2*size_t(a)+1]});++a;
                }
            }
            waveCache_[i]={mn,mx};
        }
    }
    clip(r);
    if(editable){
        float a=std::max(left,X(ts.start)),b=std::min(left+w,X(ts.end));
        if(b>a)rrect({a,r.y+2,b-a,r.h-4},0,withAlpha(TRACE_GREEN,22));
    }
    line(left,cy,left+w,cy,GRID,.7f);
    for(int i=0;i<bins;i++){
        const float x=left+(i+.5f)*w/bins,p=v.lo+v.span*(i+.5f)/bins;
        const auto q=waveCache_[i];
        line(x,cy-half*std::clamp(q[0],-1.f,1.f),x,cy-half*std::clamp(q[1],-1.f,1.f),!editable||(p>=ts.start&&p<=ts.end)?TRACE_GREEN:WELL_DIM,1.2f);
    }
    // A normalized source position remains absolute even when zoomed.
    for(int j=0;j<st_.voiceN[t];j++){float p=clamp01(st_.voicePos[t][j]);if(in(p))line(X(p),r.y+19,X(p),r.y+r.h-14,encoderTrace(j),1.3f);}
    if(editable&&ts.algo==4){
        const float p=ts.start+ts.echo.position*(ts.end-ts.start);
        if(in(p)){line(X(p),r.y+17,X(p),r.y+r.h-16,TRACE_AMBER,1.5f);circle(X(p),r.y+r.h-16,3,TRACE_AMBER);}
    }
    const float seconds=float(source->frames)/source->sr;
    for(int i=0;i<5;i++){
        float x=left+w*i/4;line(x,r.y+r.h-13,x,r.y+r.h-10,GRID);
        text(x,r.y+r.h-6,8,WELL_DIM,num(v.span*seconds<1?"%.3f":"%.2f",(v.lo+v.span*i/4)*seconds),i==0?0:i==4?2:1);
    }
    const std::string info=editable?("S "+num("%.3f",ts.start*seconds)+" / E "+num("%.3f",ts.end*seconds)+" S"):("VIEW "+num("%.3f S",v.span*seconds));
    text(left+3,r.y+9,9,WELL_TEXT,fit(info,w-52,9));text(left+w-3,r.y+9,9,WELL_DIM,num("%.1fX",1/v.span),2);
    endClip();
    // One finger pans the view, not the audio. Trim handles take precedence.
    hit(r,[this](float x,float){wavePanX_=x;wavePanLo_=waveView().lo;},
        [this,r,t](float x,float){if(app_.track()!=t)return;auto& v=waveView();waveSet(wavePanLo_-(x-wavePanX_)*v.span/std::max(1.f,r.w-4),v.span);});
    if(editable){
        for(int edge=0;edge<2;edge++){
            const float p=edge?ts.end:ts.start;if(!in(p))continue;
            const float x=X(p);
            line(x,r.y+18,x,r.y+r.h-12,encoderTrace(edge),2);
            // Put the two handles on opposing sides when the selection is narrow.
            const float hX=edge?x-11:x;
            rrect({std::clamp(hX,r.x,r.x+r.w-11),r.y+18,11,12},0,encoderTrace(edge));
            text(std::clamp(hX,r.x,r.x+r.w-11)+5.5f,r.y+24,8,contrastInk(encoderTrace(edge)),edge?"E":"S",1,true);
            const float hx=std::max(r.x,x-12),hr=std::min(r.x+r.w,x+12);
            hit({hx,r.y+16,std::max(1.f,hr-hx),r.h-28},[](float,float){},
                [this,t,edge,left,w](float x,float){if(app_.track()!=t||app_.generativeDeck())return;auto& v=waveView();auto k=app_.knobs(SamplerApp::P_HOME);k[edge].set(clamp01(v.lo+(x-left)*v.span/w));});
        }
    }
}
void SamplerUI::gesture(int action,float x1,float y1,float x2,float y2,uint32_t now){
    now_=now;
    if(action==2){gestureKind_=0;haveActive_=false;return;}
    const float mid=(x1+x2)*.5f,dist=std::max(1.f,std::hypot(x2-x1,y2-y1));
    const float angle=std::atan2(y1-y2,x2-x1);
    if(action==0){
        if(haveActive_&&active_.up)active_.up();haveActive_=false;
        gestureKind_=0;
        if(sheetOpen_||genSelector_>=0||projectOpen_)return;
        if(fieldRect_.has(x1,y1)&&fieldRect_.has(x2,y2)&&app_.generativeDeck()&&!app_.editor()){
            gestureKind_=1;gestureSpan_=app_.genConfig().span;gestureRouteAngle_=app_.genConfig().angle;
        }else if(waveRect_.has(x1,y1)&&waveRect_.has(x2,y2)&&app_.editor()&&waveTrack_==app_.track()){
            gestureKind_=2;auto& v=waveView();gestureSpan_=v.span;
            gestureAnchor_=v.lo+v.span*clamp01((mid-waveRect_.x-2)/(waveRect_.w-4));
        }
        gestureTrack_=app_.track();gestureGen_=app_.generativeDeck();gestureDist_=dist;gestureAngle_=angle;gestureMid_=mid;
        return;
    }
    if(gestureTrack_!=app_.track()||gestureGen_!=app_.generativeDeck()){gestureKind_=0;return;}
    if(gestureKind_==1&&!app_.editor()&&fieldRect_.w>0){
        // Preserve the apparent rotation despite the field's rectangular projection.
        const float a=gestureRouteAngle_*generative::tau/360;
        const float screen=std::atan2(std::sin(a)*fieldRect_.h,std::cos(a)*fieldRect_.w)+angle-gestureAngle_;
        const float result=std::atan2(std::sin(screen)/fieldRect_.h,std::cos(screen)/fieldRect_.w)*360/generative::tau;
        app_.genTransform(gestureSpan_*dist/gestureDist_,result);
    }else if(gestureKind_==2&&app_.editor()&&waveRect_.w>4){
        const float span=std::clamp(gestureSpan_*gestureDist_/dist,waveMinimum(),1.f);
        const float q=clamp01((mid-waveRect_.x-2)/(waveRect_.w-4));waveSet(gestureAnchor_-q*span,span);
    }
}
