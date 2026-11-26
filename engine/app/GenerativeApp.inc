// Included by SamplerApp.cpp to reuse the existing SysEx link and draw helpers.
void SamplerApp::genChanged(){
    auto& g=gen();
    if(g.running)s_.generatorConfigure(g.config,track_);
    else{g.phrase=generative::makePhrase(g.config,g.pass);++g.version;}
    lastLed_=~0ull;
}
void SamplerApp::genRefresh(){
    for(int t=0;t<kTracks;t++){
        auto& g=genTracks_[t];
        if(!g.running)continue;
        auto snap=s_.generatorSnapshot(t);
        if(!snap.running){
            if(s_.generatorLoadState(t)!=2 || snap.serial!=g.audioSerial){
                g.running=false;g.active=0;g.cursor=-1;
            }
            continue;
        }
        if(snap.serial!=g.audioSerial){g.audioSerial=snap.serial;g.phrase=snap.phrase;g.pass=snap.phrase.pass;++g.version;}
        g.active=snap.active;g.cursor=snap.cursor;g.seconds=snap.seconds;
    }
}
void SamplerApp::setEditor(bool on){
    if(editor_==on)return;editor_=on;
    if(genDeck_){
        pending_.clear();pendingBytes_=0;needClear_=true;lastHeroLayout_=-1;lastLed_=~0ull;
        if(!pageAvailable(page_))page_=P_HOME;
    }
}
void SamplerApp::genEnsureSound(){
    if(!gen().path.empty()||lib_.empty())return;
    int choice=0;for(int i=0;i<int(lib_.size());i++)if(lib_[i].label=="Tape Key"){choice=i;break;}
    genLoadEntry(choice);
}
void SamplerApp::selectSavedTake(){
    if(latestTake_.empty()||latestTake_==seenTake_)return;
    for(int i=0;i<int(lib_.size());i++)if(lib_[i].path==latestTake_){
        seenTake_=latestTake_;loadEntry(i);previewAfterLoad_=false;say("RESAMPLE LOADED");break;
    }
}
void SamplerApp::outputRecordToggle(){
    const int state=outputRecordState();
    if(state==1){s_.outputRecordStop();say("SAVING RESAMPLE");}
    else if(state!=2){s_.outputRecordStart();say("RECORDING MASTER OUTPUT");}
}
void SamplerApp::setGenerativeDeck(bool on){
    if(on==genDeck_)return;
    if(recordingNow()||mic_||outputRecordState()==1||outputRecordState()==2||s_.status().freeRecording||s_.status().freeArmed){
        say("STOP RECORDING BEFORE SWITCHING DECKS");return;
    }
    if(on){
        for(int raw=0;raw<128;raw++)if(keyCount_[raw])keyEvent(raw,false);
        for(int note=0;note<128;note++)if(heldNote_[note]>=0)playNote(note,false);
        auditionAt_=0;previewAfterLoad_=false;
        if(previewNote_>=0){s_.noteOff(previewTrack_,previewNote_);previewNote_=-1;}
    }else{
        for(int t=0;t<kTracks;t++){auto& g=genTracks_[t];g.running=false;g.cursor=-1;g.active=0;g.seconds=0;}
    }
    genDeck_=on;s_.generatorEnable(on);
    pending_.clear();pendingBytes_=0;needClear_=true;fwDirty_=true;lastLed_=~0ull;lastHeroLayout_=-1;
    genDrawTrack_=-1;
    if(on){genEnsureSound();genChanged();if(!pageAvailable(page_))page_=P_HOME;}
    else{if(!pageAvailable(page_))page_=P_HOME;selectSavedTake();}
}
void SamplerApp::genLoadEntry(int i){
    if(i<0||i>=int(lib_.size()))return;
    genStop();gen().entry=i;gen().source=lib_[i].label;gen().path=lib_[i].path;
    applyEnv(track_);
    s_.generatorLoad(gen().path,track_);
}
void SamplerApp::genBrowse(int d){
    if(lib_.empty())return;
    const int n=int(lib_.size());genLoadEntry((std::max(0,gen().entry)+d%n+n)%n);
}
void SamplerApp::genPlay(){
    if(!genDeck_||gen().running)return;
    if(generative::count(gen().config)==0){say("CUSTOM: ENTER STARTING NOTES");return;}
    if(s_.generatorLoadState(track_)!=2){say("SELECT A READY SAMPLE");return;}
    gen().audioSerial=s_.generatorSnapshot(track_).serial;
    gen().running=true;gen().cursor=-1;gen().active=0;gen().seconds=0;
    gen().phrase=generative::makePhrase(gen().config,gen().pass);++gen().version;
    s_.generatorPlay(gen().config,gen().pass,track_);
}
void SamplerApp::genStop(bool reset){
    s_.generatorStop(track_);gen().running=false;gen().cursor=-1;gen().active=0;gen().seconds=0;
    if(reset)gen().pass=0;genChanged();
}
void SamplerApp::genHarmonic(int k,int d){
    if(k==0)gen().config.root=generative::pc(gen().config.root+d);
    else if(k==1)gen().config.scale=(gen().config.scale+d%generative::scaleCount+generative::scaleCount)%generative::scaleCount;
    else {gen().config.octave=std::clamp(gen().config.octave+d,2,6);fwDirty_=true;}
    genChanged();
}
void SamplerApp::genApply(int a){gen().config.apply=std::clamp(a,0,2);genChanged();}
void SamplerApp::genMotif(int m){
    m=std::clamp(m,0,3);if(m==gen().config.motif)return;
    if(gen().running)genStop();
    gen().config.motif=m;gen().config.pins.fill(-1);genChanged();
}
void SamplerApp::genSteps(int n){
    if(gen().config.motif==3)return;
    gen().config.steps=std::clamp(n,1,generative::maxEvents);
    for(int i=gen().config.steps;i<generative::maxEvents;i++)gen().config.pins[i]=-1;
    genChanged();
}
void SamplerApp::genRelocate(float x,float y){gen().config.cx=cl(x);gen().config.cy=cl(y);genChanged();}
void SamplerApp::genTransform(float span,float angle){gen().config.span=cl(span,.12f,.94f);gen().config.angle=std::fmod(std::fmod(angle,360.f)+360.f,360.f);genChanged();}
void SamplerApp::genTogglePin(int i){
    if(i<0||i>=gen().phrase.n||i>=generative::count(gen().config))return;
    auto& pin=gen().config.pins[i];
    pin=pin>=0?-1:std::clamp(gen().phrase.event[i].midi-generative::rootMidi(gen().phrase.config),0,12);
    genChanged();
}
void SamplerApp::genPinEnds(){
    int n=generative::count(gen().config);if(!n)return;
    gen().config.pins.fill(-1);gen().config.pins[0]=generative::baseSemi(gen().config,0);
    gen().config.pins[n-1]=generative::baseSemi(gen().config,n-1);genChanged();
}
void SamplerApp::genClearPins(){gen().config.pins.fill(-1);genChanged();}
int SamplerApp::genKeyState(int pitch)const{
    using namespace generative;pitch=pc(pitch);
    for(int i=0;i<count(gen().config);i++)if(gen().config.pins[i]>=0&&pc(rootMidi(gen().config)+gen().config.pins[i])==pitch)return 3;
    if(!gen().running&&gen().config.motif==3){
        for(int i=0;i<gen().config.customCount;i++)if(gen().config.custom[i]==pitch)return 2;
        return 0;
    }
    int result=0;
    for(int i=0;i<gen().phrase.n;i++)if(pc(gen().phrase.event[i].midi)==pitch)result=std::max(result,(gen().active&(1u<<i))?2:1);
    return result;
}
void SamplerApp::genKey(int pitch){
    using namespace generative;pitch=pc(pitch);
    // A flashing pinned key always releases its pin, including between notes.
    for(int i=count(gen().config)-1;i>=0;i--)if(gen().config.pins[i]>=0&&pc(rootMidi(gen().config)+gen().config.pins[i])==pitch){
        gen().config.pins[i]=-1;genChanged();return;
    }
    if(!gen().running&&gen().config.motif==3){
        if(customKey(gen().config,pitch))genChanged();return;
    }
    // Prefer the newest sounding occurrence, then the most recent played
    // occurrence, then the first upcoming occurrence. Never pin every repeat.
    for(int i=gen().phrase.n-1;i>=0;i--)if((gen().active&(1u<<i))&&pc(gen().phrase.event[i].midi)==pitch){genTogglePin(i);return;}
    for(int i=std::min(gen().cursor,gen().phrase.n-1);i>=0;i--)if(pc(gen().phrase.event[i].midi)==pitch){genTogglePin(i);return;}
    for(int i=0;i<gen().phrase.n;i++)if(pc(gen().phrase.event[i].midi)==pitch){genTogglePin(i);return;}
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
    auto& c=gen().config;
    switch(gen().page){
    case 0:k={f("ANGLE",&c.angle,0,360,1,"%.0f DEG"),percent("SPAN",&c.span,.12f,.94f),f("DETAIL",&c.detail,1,12,.25f,"%.2f"),integer("SEED",&c.seed,1,999)};break;
    case 1:k={percent("PITCH DEPTH",&c.depth),percent("EVOLUTION",&c.evolution),integer("OCTAVES",&c.octaves,1,5),f("PERSISTENCE",&c.persistence,.15f,.85f,.01f,"%.2f")};break;
    case 2:k={f("PACE",&c.bpm,40,180,1,"%.0f BPM"),percent("START VAR",&c.spacing),percent("GATE",&c.gate,.15f,.95f),percent("END VAR",&c.hold)};break;
    default:k={percent("WANDER",&c.wander),f("RATE",&c.warpRate,.05f,1,.05f,"%.2f"),percent("FEEDBACK",&c.feedback),integer("DEPTH",&c.iterations,1,4)};break;
    }
    return k;
}
void SamplerApp::genPalette(){
    std::vector<uint8_t> pal;
    for(int i=0;i<14;i++)cat(pal,col(rgb565(generative::color(-1+2.f*i/13))));
    cat(pal,col(rgb565(0xffedf2ee)));cat(pal,col(rgb565(0xffffc361)));
    sysex(8,pal,false,920);
}
void SamplerApp::drawGenerator(uint32_t now){
    using namespace generative;
    const bool full=needClear_;
    if(full){
        clear(WELL);
        // Same five-stop heatmap as the phone, quantized to 14 bins;
        // two spare palette slots are the white path and amber traversal.
        genPalette();genTileSig_.fill(~0ull);genMapVersion_=0;genTileCursor_=0;
        needClear_=false;fwDirty_=true;
    }
    if(!full&&(now-genDrawAt_<80||pending_.size()>3))return;
    genDrawAt_=now;
    if(genMapVersion_!=gen().version||genDrawMap_!=gen().map||genDrawTrack_!=track_){
        genMapVersion_=gen().version;genDrawMap_=gen().map;genDrawTrack_=track_;
        for(int y=0;y<20;y++)for(int x=0;x<20;x++)
            genCells_[y*20+x]=uint8_t(std::clamp(int(std::lround((field(gen().phrase,(x+.5f)/20,1-(y+.5f)/20,gen().map)+1)*6.5f)),0,13));
    }
    auto image=genCells_;
    auto put=[&](float x,float y,uint8_t c){
        int ix=std::clamp(int(x*20),0,19),iy=std::clamp(int((1-y)*20),0,19);
        image[iy*20+ix]=c;
    };
    for(int i=1;i<gen().phrase.n;i++){
        const auto a=gen().phrase.event[i-1].point,b=gen().phrase.event[i].point;
        for(int j=0;j<=100;j++){
            float q=j/100.f;
            if(i<=gen().cursor||j%12<7)put(std::lerp(a.x,b.x,q),std::lerp(a.y,b.y,q),i<=gen().cursor?15:14);
        }
    }
    for(int i=0;i<gen().phrase.n;i++){
        auto q=gen().phrase.event[i].point;put(q.x,q.y,(i==gen().cursor||gen().config.pins[i]>=0)?15:14);
    }
    // Entire 240x240 screen is the very same normalized map. 12x12 pixels
    // per sample; 60x12 tiles keep even worst-case RLE below 300 bytes.
    const int start=genTileCursor_;
    for(int step=0;step<80;step++){
        int idx=(start+step)%80,x=(idx%4)*60,y=(idx/4)*12;
        genTileCursor_=(idx+1)%80;
        std::array<uint8_t,720> pix{};uint64_t h=1469598103934665603ull;int n=0;
        for(int j=0;j<12;j++)for(int i=0;i<60;i++){uint8_t v=image[((y+j)/12)*20+(x+i)/12];pix[n++]=v;h=(h^v)*1099511628211ull;}
        if(genTileSig_[idx]==h)continue;genTileSig_[idx]=h;
        std::vector<uint8_t> p;cat(p,n14(x));cat(p,n14(y));cat(p,n14(60));cat(p,n14(12));
        for(int i=0;i<n;){int run=1;while(i+run<n&&pix[i+run]==pix[i]&&run<135)++run;
            p.push_back(uint8_t((pix[i]<<3)|std::min(7,run-1)));if(run>=8)p.push_back(uint8_t(run-8));i+=run;}
        sysex(9,p,false,3000+idx);
        if(pending_.size()>=4)return; // pace the bitmap; never flood the controller
    }
}

const char* SamplerApp::contextualPageName(Page p)const{
    if(genDeck_)switch(p){
    case P_HOME:return "FIELD";case P_ECHO:return "MELODY";case P_GRAIN:return "TIME";case P_MOTION:return "EVOLVE";
    case P_ARP:case P_KEY:return "GLOBAL";case P_SAVE:return "RESAMPLE";default:break;
    }
    return pageName(p);
}
SamplerApp::Knobs SamplerApp::genGlobalKnobs(){
    auto& c=gen().config;Knobs k;
    std::vector<std::string> roots,scales,octs,apply;
    for(int i=0;i<12;i++)roots.push_back(noteName(i));
    for(auto n:generative::scaleNames)scales.push_back(n);
    for(auto n:generative::applyNames)apply.push_back(n);
    k[0]=choice("ROOT",&c.root,roots,[this]{genChanged();});
    k[1]=choice("SCALE",&c.scale,scales,[this]{genChanged();});
    Knob o;o.name="OCTAVE";o.valid=o.isChoice=true;o.value=std::to_string(c.octave);o.norm=(c.octave-2)/4.f;
    o.step=[this](int d){genHarmonic(2,d);};o.set=[this](float v){genHarmonic(2,2+int(std::lround(cl(v)*4))-gen().config.octave);};k[2]=o;
    k[3]=choice("APPLY",&c.apply,apply,[this]{genChanged();});return k;
}
