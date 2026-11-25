// Native R10 comparison frames, using the mock-up's parameter presets only.
// This is an isolated host session, not a change to real user preset defaults.
// Render with tools/render_ui.py study_ops.json study_frames 1
#include <Session.h>
#include <SamplerApp.h>
#include <SamplerUI.h>
#include <Common.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
static std::string escaped(const std::string& s) {
    std::string o;for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c<32){char b[8];snprintf(b,sizeof b,"\\u%04x",unsigned(c));o+=b;}else o+=char(c);}return o;
}
int main(int argc,char** argv) {
    using P=SamplerApp;
    Denormals::enableOnThisThread();
    const int sr=SAMPLERATE;Session s(sr);
    auto dir=std::filesystem::temp_directory_path()/("fm1-study-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    s.setDataDir(dir.string());
    const char* home=std::getenv("HOME");
    s.setSampleDir(argc>2?argv[2]:std::string(home?home:".")+"/fm1-sampler/samples");
    SamplerApp app(s,[](const uint8_t*,size_t){});
    SamplerUI ui(app,false);ui.setSize(960,450,0,0);ui.setEditor(false);ui.setLink(true,true);
    std::vector<float> buf(512),in(512,0);uint32_t now=0;
    auto run=[&](double seconds){int blocks=std::max(1,int(seconds*sr/256));for(int i=0;i<blocks;i++){std::fill(buf.begin(),buf.end(),0);s.process(buf.data(),in.data(),256);if(i%5==0){now+=27;app.tick(now);std::this_thread::sleep_for(std::chrono::microseconds(300));}}};
    run(4);
    for(int i=0;i<30&&!app.hasSample(0);++i){std::this_thread::sleep_for(std::chrono::milliseconds(50));run(.1);}
    if(!app.hasSample(0)) {std::fprintf(stderr,"No sample in track 1; check the sample directory.\n");return 1;}
    app.selectTrack(0);app.setAlgo(0);run(.3);
    app.loopRecord();run(.1);
    for(int note:{60,64,67,72,67,64,60,55}){app.playNote(note,true);run(.65);app.playNote(note,false);run(.35);}
    app.loopRecord();run(.5);s.setLoopBars(4);s.setLoopBpm(120);run(.5);s.seek(.37f);run(.05);
    struct Preset{P::Page pg;const char* name;float v[4];unsigned mask;};
    const Preset p[]={
       {P::P_HOME,"sample",{.07f,.89f,.50f,.50f},15},
       {P::P_ECHO,"echo",{.54f,.35f,.36f,.46f},15},
       {P::P_GRAIN,"grain",{.45f,.60f,.20f,0},15},
       {P::P_MOTION,"motion",{.68f,.28f,.42f,0},15},
       {P::P_EFILTER,"filter",{0,.53f,.34f,.45f},15},
       {P::P_EDELAY,"delay",{.25f,.58f,.56f,1},15},
       {P::P_EREVERB,"reverb",{.47f,.64f,.38f,.31f},15},
       {P::P_ETAPE,"tape",{.46f,.37f,.56f,.30f},15},
       {P::P_EMIX,"emix",{.63f,.70f,.46f,.58f},15},
       {P::P_ENV,"env",{.28f,.35f,.66f,.47f},15},
       {P::P_LFO,"lfo",{.42f,.65f,.33f,0},15},
       {P::P_ARP,"arp",{.46f,0,.66f,.63f},15},
       {P::P_MIX,"mix",{.72f,.28f,.48f,.52f},15},
       {P::P_KEY,"key",{2.f/7,0,.5f,1},15},
       {P::P_SAVE,"loop",{0,1,0,.45f},10},
       {P::P_LEN,"length",{0,0,0,0},0},
       {P::P_TEMPO,"tempo",{0,0,0,0},0}};
    FILE* f=std::fopen(argc>1?argv[1]:"study_ops.json","w");if(!f)return 1;
    std::fprintf(f,"{\"W\":960,\"H\":450,\"screens\":[");
    int frames=0;
    auto dump=[&](const std::string& label){
        ui.frame(now);std::fprintf(f,"%s{\"name\":\"%s\",\"ops\":[",frames++?",":"",label.c_str());
        bool first=true;int depth=0;
        for(const auto& op:ui.ops()){
            for(float v:op.f)if(!std::isfinite(v))throw std::runtime_error(label+": non-finite draw op");
            if(op.type==UiOp::CLIP)++depth;
            if(op.type==UiOp::ENDCLIP&&--depth<0)throw std::runtime_error(label+": unbalanced clip");
            std::fprintf(f,"%s[%d,%g,%g,%g,%g,%g,%g,%u,\"%s\"]",first?"":",",int(op.type),op.f[0],op.f[1],op.f[2],op.f[3],op.f[4],op.f[5],op.color,escaped(op.text).c_str());first=false;
        }
        if(depth)throw std::runtime_error(label+": unclosed clip");
        std::fprintf(f,"]}");
    };
    for(const auto& preset:p){
        app.setAlgo(preset.pg==P::P_ECHO||preset.pg==P::P_GRAIN||preset.pg==P::P_MOTION?4:0);
        app.gotoPage(preset.pg);
        for(int i=0;i<4;i++)if(preset.mask&(1u<<i)){auto k=app.knobs();if(k[i].valid&&k[i].set)k[i].set(preset.v[i]);}
        run(.25);s.seek(.37f);run(.03);
        theme::apply(true);dump(preset.name);
        theme::apply(false);dump(std::string(preset.name)+"_dark");
    }
    std::fprintf(f,"]}\n");std::fclose(f);std::printf("Wrote %d native 960x450 study frames\n",frames);return 0;
}
