// Project and USER-library operations; included by SamplerApp.cpp.
// Versioned text settings, independent of C++ struct padding/ABI.
namespace {
namespace fs=std::filesystem;
struct ProjectWrite{
    std::ostream& o;
    template<class... T> void operator()(const T&... v){((o<<v<<' '),...);o<<'\n';}
};
struct ProjectRead{
    std::istream& i;bool ok=true;
    template<class T> void one(T& v){
        if(!ok)return;
        if(!(i>>v)){ok=false;return;}
        if constexpr(std::is_floating_point_v<T>)if(!std::isfinite(v))ok=false;
    }
    template<class... T> void operator()(T&... v){(one(v),...);}
};
template<class A,class T> void trackFields(A& a,T& t){
    a(t.algo,t.a,t.d,t.s,t.r,t.lfoRate,t.lfoDepth,t.lfoTarget,t.lfoShape,t.start,t.end,t.speed,t.pitch,
      t.kitThr,t.paulStretch,t.paulWidth,t.echoLatch,t.echoDelay,t.echoRoom,t.arpRate,t.arpPatt,t.arpOct,t.arpLen,
      t.vol,t.pan,t.sendR,t.sendD,t.scSrc,t.scThr,t.scRatio,t.root,t.tuned);
    auto& g=t.echo;
    a(g.mode,g.window,g.freeze,g.size,g.density,g.scan,g.reverse,g.pitchRandom,g.position,g.texture,g.scatter,g.drift,g.evolve);
    auto& e=t.echoInsert;
    a(e.filter,e.cutoff,e.resonance,e.drive,e.filterMix,e.tape,e.wow,e.flutter,e.age,e.hiss,e.width,e.level);
}
template<class A,class G> void generatorFields(A& a,G& g){
    a(g.page,g.map,g.pass);
    auto& c=g.config;
    a(c.root,c.scale,c.octave,c.seed,c.octaves,c.iterations,c.steps,c.motif,c.apply,c.angle,c.span,c.detail,c.depth,
      c.evolution,c.persistence,c.bpm,c.spacing,c.gate,c.hold,c.wander,c.warpRate,c.feedback,c.cx,c.cy,c.customCount);
    for(auto& x:c.custom)a(x);
    for(auto& x:c.pins)a(x);
}
template<class A,class E> void returnFields(A& a,E& p){a(p.time,p.feedback,p.size,p.damp,p.shimmer,p.ping);}
bool between(double x,double a,double b){return std::isfinite(x)&&x>=a&&x<=b;}
bool validTrack(const SamplerApp::TrackState& t){
    if(!between(t.algo,0,5)||!between(t.lfoTarget,0,3)||!between(t.lfoShape,0,4)||!between(t.arpRate,0,11)||
       !between(t.arpPatt,0,4)||!between(t.arpOct,0,3)||!between(t.scSrc,0,8)||!between(t.root,0,127)||!between(t.echoLatch,0,1)||t.start>t.end)return false;
    const float vals[]={t.a,t.d,t.s,t.r,t.lfoRate,t.lfoDepth,t.start,t.end,t.speed,t.pitch,t.kitThr,t.paulStretch,t.paulWidth,
        t.echoDelay,t.echoRoom,t.arpLen,t.vol,t.pan,t.sendR,t.sendD,t.scThr,t.scRatio};
    for(float v:vals)if(!between(v,0,1))return false;
    const auto& g=t.echo;const auto& e=t.echoInsert;
    if(!between(g.mode,0,EchoParams::ModeCount-1)||!between(g.window,0,5)||!between(g.freeze,0,1)||!between(e.filter,0,2))return false;
    const float params[]={g.size,g.density,g.scan,g.reverse,g.pitchRandom,g.position,g.texture,g.scatter,g.drift,g.evolve,
        e.cutoff,e.resonance,e.drive,e.filterMix,e.tape,e.wow,e.flutter,e.age,e.hiss,e.width,e.level};
    for(float v:params)if(!between(v,0,1))return false;
    return true;
}
bool validGenerator(const generative::Config& c){
    if(!between(c.root,0,11)||!between(c.scale,0,generative::scaleCount-1)||!between(c.octave,2,6)||
       !between(c.seed,1,999)||!between(c.octaves,1,5)||!between(c.iterations,1,4)||!between(c.steps,1,16)||
       !between(c.motif,0,3)||!between(c.apply,0,2)||!between(c.customCount,0,12)||!between(c.angle,0,360)||
       !between(c.span,.119,.941)||!between(c.detail,1,12)||!between(c.persistence,.149,.851)||!between(c.bpm,40,180)||
       !between(c.gate,.149,.951)||!between(c.warpRate,.049,1))return false;
    for(float v:{c.depth,c.evolution,c.spacing,c.hold,c.wander,c.feedback,c.cx,c.cy})if(!between(v,0,1))return false;
    for(int v:c.custom)if(!between(v,0,11))return false;
    for(int v:c.pins)if(!between(v,-1,127))return false;
    return true;
}
bool validName(const std::string& n){
    if(n.empty()||n.size()>100||n=="."||n==".."||n.front()=='.')return false;
    for(unsigned char c:n)if(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='|'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>')return false;
    return n.find_first_not_of(' ')!=std::string::npos;
}
std::string shortName(const std::string& p){auto n=fs::path(p).stem().string();return n.rfind("sample_",0)==0?n.substr(7):n;}
bool directProject(const std::string& root,const std::string& path){
    std::error_code ec;fs::path p(path);
    const auto id=p.filename().string();
    if(root.empty()||p.parent_path()!=fs::path(root)||id.rfind("project_",0)!=0||id.size()<=8||id.find_first_not_of("0123456789",8)!=std::string::npos)return false;
    return fs::is_directory(p,ec)&&!fs::is_symlink(p,ec)&&fs::is_regular_file(p/"state.txt",ec)&&fs::is_regular_file(p/"loops.bin",ec);
}
}
std::string SamplerApp::sampleLabel(int t)const{
    if(t<0||t>=kTracks||!hasSample(t))return "";
    if(!samplePaths_[t].empty())return shortName(samplePaths_[t]);
    return s_.trackLabel(t);
}
void SamplerApp::beginSaveNotice(const std::string& label){
    saveNotice_=1;saveNoticeLabel_=label;saveNoticeUntil_=0;
}
void SamplerApp::endSaveNotice(bool ok){
    saveNotice_=ok?2:3;saveNoticeUntil_=nowMs_+1500;
    say(ok?"SAVED":"SAVE FAILED");
}
bool SamplerApp::saveProject(const std::string& name){
    if(!validName(name)||s_.dataDir().empty()||nativeFileSaving_||saveNoticeState()!=0||recordingNow()||mic_||fieldMode_||
       outputRecordState()==1||outputRecordState()==2||st_.sampleState==2)return false;
    std::ostringstream o;o<<std::setprecision(std::numeric_limits<float>::max_digits10)<<"FM1PROJECT 1\n";
    ProjectWrite w{o};
    w(track_,int(page_),int(genDeck_),int(editor_),int(fieldHubSelected_),scale_.scale,scale_.root,scale_.octave,scale_.mode,loopSrc_,micThr_,
      reverbSize_,reverbDw_,delayTime_,delayFb_);
    returnFields(w,echoReturns_);
    std::vector<std::pair<std::string,std::string>> sources;
    auto add=[&](const std::string& path,const std::string& folder){
        if(path.empty()){o<<std::quoted(std::string())<<'\n';return true;}
        std::error_code ec;if(!fs::is_regular_file(path,ec))return false;
        const std::string filename=fs::path(path).filename().string(),relative=folder+"/"+filename;
        sources.emplace_back(path,relative);o<<std::quoted(relative)<<'\n';return true;
    };
    for(int t=0;t<kTracks;++t){
        if(!add(samplePaths_[t],"t"+std::to_string(t)))return false;
        trackFields(w,ts_[t]);
        if(!add(genTracks_[t].path,"g"+std::to_string(t)))return false;
        generatorFields(w,genTracks_[t]);
    }
    const auto id=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string dir=s_.dataDir()+"/project_"+std::to_string(id);
    if(!s_.saveProject(dir,name,o.str(),sources))return false;
    nativeFileSaving_=true;beginSaveNotice("PROJECT / "+name);return true;
}
bool SamplerApp::loadProject(const std::string& dir){
    if(!directProject(s_.dataDir(),dir)||nativeFileSaving_||recordingNow()||mic_||fieldMode_||st_.sampleState==2||outputRecordState()==1||outputRecordState()==2)return false;
    std::ifstream in(dir+"/state.txt");std::string magic;int version=0;
    if(!(in>>magic>>version)||magic!="FM1PROJECT"||version!=1)return false;
    ProjectRead r{in};int track=0,page=0,deck=0,editor=0,hub=0,loop=1;ScaleState scale;
    float mic=0,rev=0,dw=0,delay=0,fb=0;EchoReturnParams returns;
    r(track,page,deck,editor,hub,scale.scale,scale.root,scale.octave,scale.mode,loop,mic,rev,dw,delay,fb);
    returnFields(r,returns);
    if(!r.ok||!between(track,0,kTracks-1)||!between(page,0,kPages-1)||!between(deck,0,1)||!between(editor,0,1)||!between(hub,0,1)||
       !between(scale.scale,0,7)||!between(scale.root,0,11)||!between(scale.octave,-3,3)||!between(scale.mode,0,1)||!between(loop,0,1)||!between(returns.ping,0,1))return false;
    for(float v:{mic,rev,dw,delay,fb,returns.time,returns.feedback,returns.size,returns.damp,returns.shimmer})if(!between(v,0,1))return false;
    std::array<TrackState,kTracks> tracks;
    std::array<GenTrack,kTracks> gens;
    std::array<std::string,kTracks> paths;
    auto readPath=[&](std::string& path,const std::string& folder){
        std::string rel;if(!(in>>std::quoted(rel)))return false;
        if(rel.empty()){path.clear();return true;}
        fs::path q(rel);std::error_code ec;
        if(q.parent_path()!=fs::path(folder)||q.filename()=="."||q.filename()==".."||q.filename().empty())return false;
        auto full=fs::path(dir)/q;if(!fs::is_regular_file(full,ec)||fs::is_symlink(full,ec))return false;
        path=full.string();return true;
    };
    for(int t=0;t<kTracks;++t){
        if(!readPath(paths[t],"t"+std::to_string(t)))return false;
        trackFields(r,tracks[t]);
        if(!r.ok||!validTrack(tracks[t])||!readPath(gens[t].path,"g"+std::to_string(t)))return false;
        generatorFields(r,gens[t]);
        if(!r.ok||!validGenerator(gens[t].config)||!between(gens[t].page,0,3)||!between(gens[t].map,0,2))return false;
        gens[t].source=gens[t].path.empty()?"":shortName(gens[t].path);
        gens[t].phrase=generative::makePhrase(gens[t].config,gens[t].pass);
    }
    if(!s_.loadProjectLoops(dir))return false;
    releaseSurfaceKeys();auditionAt_=commitAt_=0;previewAfterLoad_=false;
    if(previewNote_>=0){s_.noteOff(previewTrack_,previewNote_);previewNote_=-1;}
    ts_=tracks;genTracks_=std::move(gens);samplePaths_=std::move(paths);
    track_=track;page_=static_cast<Page>(page);genDeck_=deck!=0;editor_=editor!=0;fieldHubSelected_=hub!=0;
    scale_=scale;loopSrc_=loop;micThr_=mic;echoReturns_=returns;reverbSize_=rev;reverbDw_=dw;delayTime_=delay;delayFb_=fb;
    s_.generatorEnable(genDeck_);s_.setLoopSource(loopSrc_);s_.setMicThreshold(micThreshold());applyFx();
    for(int t=0;t<kTracks;++t){
        s_.generatorStop(t);s_.clearSample(t);s_.generatorClear(t);
        // clearSample is ticketed: loading a replacement invalidates its pending clear.
        if(!samplePaths_[t].empty())s_.loadSamplePath(samplePaths_[t],ts_[t].algo==3,t);
        if(!genTracks_[t].path.empty())s_.generatorLoad(genTracks_[t].path,t);
        applyAll(t);editEchoFx(t);
    }
    refreshLibrary();if(!pageAvailable(page_))page_=P_HOME;
    if(ts_[track_].entry>=0)libCursor_=ts_[track_].entry;
    pending_.clear();pendingBytes_=0;needClear_=fwDirty_=true;lastLed_=~0ull;genDrawTrack_=-1;lastHeroLayout_=-1;
    say("PROJECT OPENED");return true;
}
// File-manager commands: paths are restricted to listed user assets/projects.
// Never modify bundled samples, arbitrary paths, or active project source directories.
std::string SamplerApp::manageFile(int op,const std::string& path,const std::string& name){
    if(nativeFileSaving_||recordingNow()||mic_||fieldMode_||outputRecordState()==1||outputRecordState()==2)return "STOP RECORDING OR SAVING FIRST";
    if(op==3)return saveProject(name)?"":"COULD NOT SAVE PROJECT";
    if(op==4)return loadProject(path)?"":"COULD NOT OPEN PROJECT";
    std::error_code ec;
    const bool project=directProject(s_.dataDir(),path);
    auto users=s_.sampleLibrary();
    const bool user=std::find(users.begin(),users.end(),path)!=users.end()&&!fs::is_symlink(path,ec)&&fs::is_regular_file(path,ec);
    if(!project&&!user)return "FILE NOT FOUND";
    if(op==0&&user){
        refreshLibrary();for(int i=0;i<int(lib_.size());++i)if(lib_[i].path==path){loadEntry(i);return "";}
        return "FILE NOT FOUND";
    }
    if(op!=1&&op!=2)return "INVALID ACTION";
    if(st_.sampleState==2)return "WAIT FOR SAMPLE LOAD";
    for(int t=0;t<kTracks;++t)if(s_.generatorLoadState(t)==1)return "WAIT FOR SAMPLE LOAD";
    if(op==1&&!validName(name))return "INVALID NAME";
    auto used=[&](const std::string& p){return project?p.rfind(path+"/",0)==0:p==path;};
    bool active=false;for(int t=0;t<kTracks;++t)active|=used(samplePaths_[t])||used(genTracks_[t].path);
    if(op==2&&active)return "FILE IN USE — SELECT ANOTHER SOURCE FIRST";
    try{
        if(op==2){if(project)fs::remove_all(path);else if(!fs::remove(path))return "DELETE FAILED";}
        else if(project){
            auto tmp=path+"/name.tmp";std::ofstream out(tmp);out<<name;out.close();if(!out)return "RENAME FAILED";fs::rename(tmp,path+"/name.txt");
        }else{
            std::string stem=name;if(stem.size()>4&&stem.substr(stem.size()-4)==".wav")stem.resize(stem.size()-4);
            if(!validName(stem))return "INVALID NAME";
            auto dst=s_.dataDir()+"/sample_"+stem+".wav";
            if(dst!=path){
                if(fs::exists(dst))return "NAME ALREADY EXISTS";
                fs::rename(path,dst);
                for(int t=0;t<kTracks;++t){
                    if(samplePaths_[t]==path)samplePaths_[t]=dst;
                    if(genTracks_[t].path==path){genTracks_[t].path=dst;genTracks_[t].source=stem;}
                }
                if(latestTake_==path)latestTake_=dst;if(seenTake_==path)seenTake_=dst;
            }
        }
    }catch(...){return "FILE OPERATION FAILED";}
    refreshLibrary();say(op==1?"RENAMED":"DELETED");return "";
}
