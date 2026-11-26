// Included by SamplerApp.cpp to reuse the existing SysEx link and draw helpers.
void SamplerApp::genChanged(){
    if(genRunning_)s_.generatorConfigure(genConfig_);
    else{genPhrase_=generative::makePhrase(genConfig_,genPass_);++genVersion_;}
    lastLed_=~0ull;
}
void SamplerApp::genRefresh(){
    if(!genRunning_)return;
    auto snap=s_.generatorSnapshot();
    if(!snap.running){
        if(s_.generatorLoadState()!=2){genRunning_=false;genActive_=0;genCursor_=-1;}
        return;
    }
    if(snap.serial!=genAudioSerial_){
        genAudioSerial_=snap.serial;genPhrase_=snap.phrase;genPass_=snap.phrase.pass;++genVersion_;
    }
    genActive_=snap.active;genCursor_=snap.cursor;genSeconds_=snap.seconds;
    // A custom phrase can be cleared by a pending edit at a pass boundary.
}
void SamplerApp::setGenerativeDeck(bool on){
    if(on==genDeck_)return;
    if(on&&(recordingNow()||mic_||s_.status().freeRecording||s_.status().freeArmed)){
        say("STOP RECORDING BEFORE SWITCHING DECKS");return;
    }
    // Release physical notes without changing the REC hold bookkeeping.
    if(on){
        for(int raw=0;raw<128;raw++)if(keyCount_[raw])keyEvent(raw,false);
        for(int note=0;note<128;note++)if(heldNote_[note]>=0)playNote(note,false);
        auditionAt_=0;previewAfterLoad_=false;
        if(previewNote_>=0){s_.noteOff(previewTrack_,previewNote_);previewNote_=-1;}
    }else genStop();
    genDeck_=on;s_.generatorEnable(on);
    pending_.clear();pendingBytes_=0;needClear_=true;fwDirty_=true;lastLed_=~0ull;lastHeroLayout_=-1;
    if(on){
        if(genPath_.empty()&&!lib_.empty()){
            int choice=0;for(int i=0;i<int(lib_.size());i++)if(lib_[i].label=="Tape Key"){choice=i;break;}
            genLoadEntry(choice);
        }
        genChanged();
    }
}
void SamplerApp::genLoadEntry(int i){
    if(i<0||i>=int(lib_.size()))return;
    genStop();genEntry_=i;genSource_=lib_[i].label;genPath_=lib_[i].path;
    s_.generatorLoad(genPath_);
}
void SamplerApp::genBrowse(int d){
    if(lib_.empty())return;
    const int n=int(lib_.size());genLoadEntry((std::max(0,genEntry_)+d%n+n)%n);
}
void SamplerApp::genPlay(){
    if(!genDeck_||genRunning_)return;
    if(generative::count(genConfig_)==0){say("CUSTOM: ENTER STARTING NOTES");return;}
    if(s_.generatorLoadState()!=2){say("SELECT A READY SAMPLE");return;}
    genRunning_=true;genCursor_=-1;genActive_=0;genSeconds_=0;
    genPhrase_=generative::makePhrase(genConfig_,genPass_);++genVersion_;
    s_.generatorPlay(genConfig_,genPass_);
}
void SamplerApp::genStop(bool reset){
    s_.generatorStop();genRunning_=false;genCursor_=-1;genActive_=0;genSeconds_=0;
    if(reset)genPass_=0;genChanged();
}
void SamplerApp::genHarmonic(int k,int d){
    if(k==0)genConfig_.root=generative::pc(genConfig_.root+d);
    else if(k==1)genConfig_.scale=(genConfig_.scale+d%generative::scaleCount+generative::scaleCount)%generative::scaleCount;
    else genConfig_.octave=std::clamp(genConfig_.octave+d,2,6);
    genChanged();
}
void SamplerApp::genApply(int a){genConfig_.apply=std::clamp(a,0,2);genChanged();}
void SamplerApp::genMotif(int m){
    m=std::clamp(m,0,3);if(m==genConfig_.motif)return;
    if(genRunning_)genStop();
    genConfig_.motif=m;genConfig_.pins.fill(-1);genChanged();
}
void SamplerApp::genSteps(int n){
    if(genConfig_.motif==3)return;
    genConfig_.steps=std::clamp(n,1,generative::maxEvents);
    for(int i=genConfig_.steps;i<generative::maxEvents;i++)genConfig_.pins[i]=-1;
    genChanged();
}
void SamplerApp::genRelocate(float x,float y){genConfig_.cx=cl(x);genConfig_.cy=cl(y);genChanged();}
void SamplerApp::genTogglePin(int i){
    if(i<0||i>=genPhrase_.n||i>=generative::count(genConfig_))return;
    auto& pin=genConfig_.pins[i];
    pin=pin>=0?-1:std::clamp(genPhrase_.event[i].midi-generative::rootMidi(genPhrase_.config),0,12);
    genChanged();
}
void SamplerApp::genPinEnds(){
    int n=generative::count(genConfig_);if(!n)return;
    genConfig_.pins.fill(-1);genConfig_.pins[0]=generative::baseSemi(genConfig_,0);
    genConfig_.pins[n-1]=generative::baseSemi(genConfig_,n-1);genChanged();
}
void SamplerApp::genClearPins(){genConfig_.pins.fill(-1);genChanged();}
int SamplerApp::genKeyState(int pitch)const{
    using namespace generative;pitch=pc(pitch);
    for(int i=0;i<count(genConfig_);i++)if(genConfig_.pins[i]>=0&&pc(rootMidi(genConfig_)+genConfig_.pins[i])==pitch)return 3;
    if(!genRunning_&&genConfig_.motif==3){
        for(int i=0;i<genConfig_.customCount;i++)if(genConfig_.custom[i]==pitch)return 2;
        return 0;
    }
    int result=0;
    for(int i=0;i<genPhrase_.n;i++)if(pc(genPhrase_.event[i].midi)==pitch)result=std::max(result,(genActive_&(1u<<i))?2:1);
    return result;
}
void SamplerApp::genKey(int pitch){
    using namespace generative;pitch=pc(pitch);
    // A flashing pinned key always releases its pin, including between notes.
    for(int i=count(genConfig_)-1;i>=0;i--)if(genConfig_.pins[i]>=0&&pc(rootMidi(genConfig_)+genConfig_.pins[i])==pitch){
        genConfig_.pins[i]=-1;genChanged();return;
    }
    if(!genRunning_&&genConfig_.motif==3){
        if(customKey(genConfig_,pitch))genChanged();return;
    }
    // Prefer the newest sounding occurrence, then the most recent played
    // occurrence, then the first upcoming occurrence. Never pin every repeat.
    for(int i=genPhrase_.n-1;i>=0;i--)if((genActive_&(1u<<i))&&pc(genPhrase_.event[i].midi)==pitch){genTogglePin(i);return;}
    for(int i=std::min(genCursor_,genPhrase_.n-1);i>=0;i--)if(pc(genPhrase_.event[i].midi)==pitch){genTogglePin(i);return;}
    for(int i=0;i<genPhrase_.n;i++)if(pc(genPhrase_.event[i].midi)==pitch){genTogglePin(i);return;}
}
SamplerApp::Knobs SamplerApp::genKnobs(){
    Knobs k;
    auto f=[this](const char* name,float* value,float lo,float hi,float step,const char* format){
        Knob z;z.name=name;z.valid=true;z.norm=cl((*value-lo)/(hi-lo));z.value=fmt(format,*value);
        z.set=[=,this](float n){*value=cl(lo+(hi-lo)*cl(n),lo,hi);if(step>=1.f)*value=std::round(*value);genChanged();};
        z.step=[=,this](int d){*value=cl(*value+step*d,lo,hi);if(step>=1.f)*value=std::round(*value);genChanged();};return z;
    };
    auto percent=[&](const char* name,float* value,float lo=0.f,float hi=1.f){
        auto z=f(name,value,lo,hi,.01f,"%.2f");z.value=pct(*value);return z;
    };
    auto integer=[this](const char* name,int* value,int lo,int hi){
        Knob z;z.name=name;z.valid=z.isChoice=true;z.norm=float(*value-lo)/(hi-lo);z.value=std::to_string(*value);
        z.set=[=,this](float n){*value=std::clamp(lo+int(std::lround(cl(n)*(hi-lo))),lo,hi);genChanged();};
        z.step=[=,this](int d){*value=std::clamp(*value+d,lo,hi);genChanged();};return z;
    };
    auto& c=genConfig_;
    switch(genPage_){
    case 0:k={f("ANGLE",&c.angle,0,360,1,"%.0f DEG"),percent("SPAN",&c.span,.12f,.94f),f("DETAIL",&c.detail,1,12,.25f,"%.2f"),integer("SEED",&c.seed,1,999)};break;
    case 1:k={percent("PITCH DEPTH",&c.depth),percent("EVOLUTION",&c.evolution),integer("OCTAVES",&c.octaves,1,5),f("PERSISTENCE",&c.persistence,.15f,.85f,.01f,"%.2f")};break;
    case 2:k={f("PACE",&c.bpm,40,180,1,"%.0f BPM"),percent("START VAR",&c.spacing),percent("GATE",&c.gate,.15f,.95f),percent("END VAR",&c.hold)};break;
    default:k={percent("WANDER",&c.wander),f("RATE",&c.warpRate,.05f,1,.05f,"%.2f"),percent("FEEDBACK",&c.feedback),integer("DEPTH",&c.iterations,1,4)};break;
    }
    return k;
}
void SamplerApp::drawGenerator(uint32_t now){
    using namespace generative;
    const bool full=needClear_;
    if(full){
        clear(WELL);
        // Same five-stop heatmap as the phone, quantized to 14 bins;
        // two spare palette slots are the white path and amber traversal.
        std::vector<uint8_t> pal;
        for(int i=0;i<14;i++)cat(pal,col(rgb565(color(-1+2.f*i/13))));
        cat(pal,col(rgb565(0xffedf2ee)));cat(pal,col(rgb565(0xffffc361)));
        sysex(8,pal,false,920);genTileSig_.fill(~0ull);genMapVersion_=0;
        needClear_=false;fwDirty_=true;
    }
    if(!full&&now-genDrawAt_<160)return;
    genDrawAt_=now;
    if(genMapVersion_!=genVersion_||genDrawMap_!=genMap_){
        genMapVersion_=genVersion_;genDrawMap_=genMap_;
        for(int y=0;y<40;y++)for(int x=0;x<40;x++)
            genCells_[y*40+x]=uint8_t(std::clamp(int(std::lround((field(genPhrase_,(x+.5f)/40,1-(y+.5f)/40,genMap_)+1)*6.5f)),0,13));
    }
    auto image=genCells_;
    auto put=[&](float x,float y,uint8_t c){
        int ix=std::clamp(int(x*40),0,39),iy=std::clamp(int((1-y)*40),0,39);
        image[iy*40+ix]=c;
    };
    for(int i=1;i<genPhrase_.n;i++){
        const auto a=genPhrase_.event[i-1].point,b=genPhrase_.event[i].point;
        for(int j=0;j<=100;j++){
            float q=j/100.f;
            if(i<=genCursor_||j%12<7)put(std::lerp(a.x,b.x,q),std::lerp(a.y,b.y,q),i<=genCursor_?15:14);
        }
    }
    for(int i=0;i<genPhrase_.n;i++){
        auto q=genPhrase_.event[i].point;put(q.x,q.y,(i==genCursor_||genConfig_.pins[i]>=0)?15:14);
    }
    // Entire 240x240 screen is the very same normalized map. 6x6 pixels
    // per sample; 60x12 tiles keep even worst-case RLE below 300 bytes.
    int idx=0;
    for(int y=0;y<240;y+=12)for(int x=0;x<240;x+=60,++idx){
        std::array<uint8_t,720> pix{};uint64_t h=1469598103934665603ull;int n=0;
        for(int j=0;j<12;j++)for(int i=0;i<60;i++){uint8_t v=image[((y+j)/6)*40+(x+i)/6];pix[n++]=v;h=(h^v)*1099511628211ull;}
        if(genTileSig_[idx]==h)continue;genTileSig_[idx]=h;
        std::vector<uint8_t> p;cat(p,n14(x));cat(p,n14(y));cat(p,n14(60));cat(p,n14(12));
        for(int i=0;i<n;){int run=1;while(i+run<n&&pix[i+run]==pix[i]&&run<135)++run;
            p.push_back(uint8_t((pix[i]<<3)|std::min(7,run-1)));if(run>=8)p.push_back(uint8_t(run-8));i+=run;}
        sysex(9,p,false,3000+idx);
    }
}
