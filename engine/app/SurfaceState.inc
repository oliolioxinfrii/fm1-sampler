// The semantic surface is an independent client of the existing knob definitions.
// No change to phone UI, DSP, project persistence or v1 renderer.
namespace {
unsigned svNorm(float v){return unsigned(std::lround(cl(v)*SV_NORM));}
void svString(std::vector<uint8_t>& p,const std::string& s,size_t n){
    for(size_t i=0;i<n;++i){unsigned char q=i<s.size()?s[i]:0;p.push_back(q>=32&&q<=126?q:0);}
}
void svKnob(std::vector<uint8_t>& p,const SamplerApp::Knob& k,bool accel){
    p.push_back(k.valid?(SV_VALID|(k.isChoice?SV_CHOICE:0)|(accel&&!k.isChoice&&k.surfaceStep>0?SV_ACCEL:0)|(k.surfaceStep<=0?SV_ACTION:0)):0);
    cat(p,n14(svNorm(k.norm)));cat(p,n14(svNorm(k.surfaceStep)));
    svString(p,k.name,SV_NAME);svString(p,k.value,SV_VALUE);
}
uint64_t svHash(const std::vector<uint8_t>& p){uint64_t h=1469598103934665603ull;for(auto x:p)h=(h^x)*1099511628211ull;return h;}
}
void SamplerApp::resetStateSurface(){
    surfaceV2_=false;surfaceAck_=0;surfaceAt_=surfaceDataAt_=surfaceCacheAt_=0;surfaceDataGen_=0;
    surfaceLast_.clear();surfaceData_.clear();surfaceDataContext_=-1;surfaceCacheCursor_=0;surfaceCacheSig_.fill(0);surfaceBlobs_={};surfaceNextGen_=0;
}
int SamplerApp::surfaceContext()const{
    unsigned mode=fieldMode_?SV_FIELD:genDeck_?(editor_?SV_GEN_HUB:SV_GENERATOR):SV_TAPE;
    unsigned page=fieldMode_?std::clamp(std::atoi(fieldLines_[12].c_str())+1,0,10):mode==SV_GENERATOR?gen().page:int(page_);
    if(fieldMode_&&fieldLines_[12].empty())page=0;
    return sv_context(mode,mode==SV_FIELD?0:track_,page,mode==SV_TAPE?algo():0);
}
void SamplerApp::surfaceEdit(const uint8_t* p,size_t n){
    if(n!=7||p[4]>=4||p[5]>126||p[6]>1)return;
    unsigned ctx=sv_read14(p),seq=sv_read14(p+2);int slot=p[4],delta=int(p[5])-63;
    if(!sv_after(seq,surfaceAck_))return;
    surfaceAck_=seq;surfaceLast_.clear();
    if(ctx!=unsigned(surfaceContext())||!delta)return;
    if(fieldMode_){
        // Acceleration is already applied by the surface. The WebView remains
        // authoritative for marker constraints and the actual restoration settings.
        fieldEvent(300+slot+2,delta*16+1);return;
    }
    if(!genDeck_&&mic_==1&&page_!=P_SAVE&&slot==0){setMicThreshold01(micThr_+delta*.03f);return;}
    auto k=genDeck_&&!editor_?genKnobs():knobs();
    if(k[slot].valid&&k[slot].step){k[slot].step(delta);touchedKnob_=slot;touchedAt_=nowMs_;}
}
void SamplerApp::drawStateSurface(uint32_t now){
    const int mode=fieldMode_?SV_FIELD:genDeck_?(editor_?SV_GEN_HUB:SV_GENERATOR):SV_TAPE;
    const int page=fieldMode_?(fieldLines_[12].empty()?0:std::clamp(std::atoi(fieldLines_[12].c_str())+1,0,10)):mode==SV_GENERATOR?gen().page:int(page_);
    const int track=fieldMode_?0:track_,ctx=surfaceContext();
    const bool rec=fieldMode_?(fieldLeds_&(1u<<REC)):genDeck_?outputRecordState()==1:st_.freeRecording||st_.track_clip_rec[track_];
    const bool play=fieldMode_?(fieldLeds_&(1u<<PLAY)):genDeck_?gen().running:st_.playing;
    std::string title=fieldMode_?fieldLines_[11]:genDeck_?gen().source:sampleLabel(track_);
    std::string name=fieldMode_?fieldLines_[2]:mode==SV_GENERATOR?generative::pageNames[gen().page]:contextualPageName(page_);
    std::string status=fieldMode_?fieldLines_[0]+"  "+fieldLines_[1]:!toast().empty()?toast():loopText();
    Knobs k;
    unsigned kind=0,mask=0,progress=0,ma=0,mb=127,mp=0;
    std::vector<uint8_t> data;
    if(fieldMode_){
        auto num=[&](int i,int max){return std::clamp(std::atoi(fieldLines_[i].c_str()),0,max);};
        for(int i=0;i<4;++i){k[i].name=fieldLines_[3+i];k[i].value=fieldLines_[7+i];k[i].valid=!k[i].name.empty();k[i].norm=num(13+i,1000)/1000.f;k[i].surfaceStep=num(19+i,SV_NORM)/float(SV_NORM);k[i].isChoice=(page==3||page==5&&i>=2);}
        mask=(fieldLeds_>>FX)&1023;progress=unsigned(num(17,1000))*SV_NORM/1000;
        ma=num(23,127);mb=num(24,127);mp=num(25,127);
        if(page==0){kind=SV_SPECTRUM;data.assign(fieldSpectrum_.begin(),fieldSpectrum_.end());cat(data,n14(num(27,SV_NORM)));cat(data,n14(num(28,SV_NORM)));}
        else if(page==4){kind=SV_NOISE;data.assign(fieldPixels_.begin(),fieldPixels_.end());}
        // Fade graph needs the selected duration, sent as a compact auxiliary value.
        if(page==9){kind=SV_NOISE;auto ms=std::clamp(std::atoi(fieldLines_[26].c_str()),1,2097151);data={uint8_t(ms>>14&127),uint8_t(ms>>7&127),uint8_t(ms&127)};}
    }else{
        k=mode==SV_GENERATOR?genKnobs():knobs();
        if(mode==SV_GENERATOR){
            kind=SV_FIELD_MAP;
            if(genMapVersion_!=gen().version||genDrawMap_!=gen().map||genDrawTrack_!=track_){
                genMapVersion_=gen().version;genDrawMap_=gen().map;genDrawTrack_=track_;
                for(int y=0;y<20;++y)for(int x=0;x<20;++x)genCells_[20*y+x]=uint8_t(std::clamp(int(std::lround((generative::field(gen().phrase,(x+.5f)/20,1-(y+.5f)/20,gen().map)+1)*6.5f)),0,13));
            }
            data.assign(genCells_.begin(),genCells_.end());
            data.push_back(uint8_t(gen().phrase.n));
            for(int i=0;i<gen().phrase.n;++i){const auto& e=gen().phrase.event[i];data.push_back(uint8_t(std::lround(cl(e.point.x)*127)));data.push_back(uint8_t(std::lround(cl(1-e.point.y)*127)));data.push_back(uint8_t(gen().config.pins[i]>=0));}
            mp=unsigned(std::clamp(gen().cursor+1,0,127));progress=svNorm(gen().phrase.total>0?gen().seconds/gen().phrase.total:0);
            mask=1u<<page;
        }else{
            for(int i=0;i<kPrimaryMenus;++i)if(menuParent(page_)==primaryMenuPage(i))mask|=1u<<i;
            progress=svNorm(st_.clipPos[track_]);
            ma=unsigned(std::lround(cl(ts_[track_].start)*127));mb=unsigned(std::lround(cl(ts_[track_].end)*127));
            mp=st_.voiceN[track_]>0?unsigned(std::lround(cl(st_.voicePos[track_][0])*127)):ma;
            if(page_==P_HOME&&!genDeck_){kind=SV_WAVE;auto pk=peaks(track_,216);for(float v:pk)data.push_back(uint8_t(std::lround(cl(v)*127)));}
            else if(page_==P_MIX||page_==P_SAVE){mp=unsigned(std::lround(cl(page_==P_SAVE?st_.inputPeak:st_.track_peak[track_])*127));}
            else if(page_==P_LFO){mp=unsigned(std::lround(cl(st_.lfoPhase[track_])*127));}
        }
    }
    if(data.size()>SV_DATA_MAX)data.resize(SV_DATA_MAX);
    // Each track waveform and each field-map selection retains a stable data
    // generation when revisiting pages. Parameter/marker/theme changes do not
    // invalidate an unchanged signal. IDs are scoped to the USB session.
    const int key=kind==SV_WAVE?track:kind==SV_SPECTRUM?8:kind==SV_NOISE?(page==9?10:9):kind==SV_FIELD_MAP?11+3*track+gen().map:35;
    auto& blob=surfaceBlobs_[key];
    if(blob.key!=key||blob.bytes!=data){blob.key=key;blob.bytes=data;blob.gen=(++surfaceNextGen_)&16383;if(!blob.gen)blob.gen=(++surfaceNextGen_)&16383;}
    const bool dataChanged=surfaceDataContext_!=ctx||blob.gen!=surfaceDataGen_;
    if(dataChanged){surfaceDataContext_=ctx;surfaceData_=data;surfaceDataGen_=blob.gen;surfaceDataAt_=0;discardPending(5000,5010);}
    std::vector<uint8_t> p={1};cat(p,n14(ctx));cat(p,n14(surfaceAck_));
    p.insert(p.end(),{uint8_t(theme::light),uint8_t(mode),uint8_t(track),uint8_t(page),uint8_t(mode==SV_TAPE?algo():0),uint8_t(rec?2:play?1:0)});
    cat(p,n14(mask));cat(p,n14(progress));cat(p,n14(surfaceDataGen_));
    p.insert(p.end(),{uint8_t(kind),uint8_t(ma),uint8_t(mb),uint8_t(mp)});
    svString(p,title.empty()?"EMPTY":title,SV_TITLE);svString(p,name,SV_TITLE);svString(p,status,SV_STATUS);
    for(auto& q:k)svKnob(p,q,mode==SV_TAPE||(mode==SV_FIELD&&page==0)||mode==SV_GEN_HUB||mode==SV_GENERATOR);
    if(p!=surfaceLast_||now-surfaceAt_>=500){surfaceLast_=p;surfaceAt_=now;sysex(SV_STATE,p,false,4900);}
    // A generation-tagged cache transfer, no drawing coordinates or colors.
    // Periodic retransmit repairs any incomplete transfer without changing the generation.
    if(!data.empty()&&(!surfaceDataAt_||now-surfaceDataAt_>=5000)){
        surfaceDataAt_=now?now:1;
        for(size_t off=0;off<data.size();off+=SV_CHUNK){
            std::vector<uint8_t> q;cat(q,n14(ctx));cat(q,n14(surfaceDataGen_));q.push_back(uint8_t(kind));
            cat(q,n14(unsigned(data.size())));cat(q,n14(unsigned(off)));
            q.insert(q.end(),data.begin()+off,data.begin()+std::min(data.size(),off+SV_CHUNK));
            sysex(SV_DATA,q,false,5000+int(off/SV_CHUNK));
        }
    }
    // Populate/update the other track/page caches opportunistically. The active
    // screen and input acknowledgements always take precedence.
    if(!fieldMode_&&pending_.size()<2&&now-surfaceCacheAt_>=100){
        surfaceCacheAt_=now;
        int i=surfaceCacheCursor_++%int(surfaceCacheSig_.size()),cm=i/(8*SV_PAGES),ct=(i/SV_PAGES)%8,cp=i%SV_PAGES;
        const int savedTrack=track_;const bool savedGen=genDeck_;
        track_=ct;genDeck_=cm!=SV_TAPE;
        if((cm==SV_GENERATOR?cp<4:cp<kPages&&pageAvailable(Page(cp)))){
            int oldPage=gen().page;if(cm==SV_GENERATOR)gen().page=cp;
            auto ks=cm==SV_GENERATOR?genKnobs():knobs(Page(cp));gen().page=oldPage;
            unsigned cc=sv_context(cm,ct,cp,cm==SV_TAPE?algo():0);
            std::vector<uint8_t> cache=p;cache[0]=0;sv_write14(cache.data()+1,cc);cache[6]=cm;cache[7]=ct;cache[8]=cp;cache[9]=cm==SV_TAPE?algo():0;
            cache.resize(SV_HEADER_BYTES+2*SV_TITLE+SV_STATUS);
            for(auto& q:ks)svKnob(cache,q,true);
            // Hash only parameter definitions/values: telemetry must not cause a background resend.
            std::vector<uint8_t> signature(cache.begin()+SV_HEADER_BYTES+2*SV_TITLE+SV_STATUS,cache.end());signature.push_back(cache[9]);
            auto hash=svHash(signature);if(surfaceCacheSig_[i]!=hash){surfaceCacheSig_[i]=hash;sysex(SV_STATE,cache,false,5100);}
        }
        track_=savedTrack;genDeck_=savedGen;
    }
}
