// Project snapshot / library regression. Uses only generated test audio.
#include "SamplerApp.h"
#include "Wav.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>
namespace fs=std::filesystem;
int main(){
    const auto id=std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path dir=fs::temp_directory_path()/("fm1-project-test-"+std::to_string(id));
    fs::create_directories(dir);
    {
        constexpr int rate=24000;
        std::vector<float> wave(rate*2);
        for(int i=0;i<rate;++i)wave[2*i]=wave[2*i+1]=.2f*std::sin(2*3.141592653589793*220*i/rate);
        const auto user=(dir/"sample_Original.wav").string();
        assert(writeWav16(user,wave,rate));
        Session s(rate);s.setDataDir(dir.string());SamplerApp app(s,[](const uint8_t*,size_t){});
        uint32_t now=0;float out[512],in[512];
        std::fill_n(in,512,.2f);
        auto tick=[&](int blocks){for(int i=0;i<blocks;++i){s.process(out,in,256);now+=11;app.tick(now);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
        auto wait=[&](auto ready){for(int i=0;i<3000&&!ready();++i)tick(1);assert(ready());};
        assert(app.library().size()==1);
        app.loadEntry(0);wait([&]{return app.hasSample(0)&&s.status().sampleState==0;});
        app.setVolume(0,.37f);app.setPan01(0,.23f);
        auto lfo=app.knobs(SamplerApp::P_LFO);lfo[2].set(1);lfo[3].set(1);
        app.setGenerativeDeck(true);wait([&]{return app.genSourceState()==2;});
        app.genSetPage(2);app.genSetMap(1);app.genRelocate(.4f,.7f);
        app.setGenerativeDeck(false);
        s.setLoopSource(0);s.freeRecord(0,true);tick(75);s.freeRecord(0,false);tick(10);
        const auto len=s.status().loopSamples;assert(len>0&&s.status().track_has_clip[0]);
        assert(app.saveProject("Test arrangement"));
        assert(!app.saveProject("Duplicate"));
        wait([&]{return s.fileSaveState()!=1;});assert(s.fileSaveState()==2);
        tick(180);assert(app.saveNoticeState()==0);
        fs::path project;
        for(auto& e:fs::directory_iterator(dir))if(e.is_directory()&&e.path().filename().string().rfind("project_",0)==0)project=e.path();
        assert(!project.empty()&&fs::is_regular_file(project/"loops.bin")&&fs::is_regular_file(project/"t0"/"sample_Original.wav")&&fs::is_regular_file(project/"g0"/"sample_Original.wav"));
        assert(app.manageFile(1,user,"Renamed").empty());
        auto renamed=(dir/"sample_Renamed.wav").string();assert(fs::exists(renamed)&&!fs::exists(user));
        assert(!app.manageFile(2,renamed,"").empty()); // active sources are protected
        s.clipClearAll();app.setVolume(0,.9f);tick(10);
        assert(app.loadProject(project.string()));
        wait([&]{return s.status().sampleState==0&&app.genSourceState()==2&&s.status().track_has_clip[0];});
        assert(s.status().loopSamples==len&&!s.status().playing);
        assert(std::fabs(app.state(0).vol-.37f)<1e-6f&&std::fabs(app.state(0).pan-.23f)<1e-6f);
        assert(app.state(0).lfoTarget==3&&app.state(0).lfoShape==4);
        assert(app.genPage()==2&&app.genMap()==1&&std::fabs(app.genConfig().cx-.4f)<1e-6f);
        assert(app.manageFile(2,renamed,"").empty()&&!fs::exists(renamed));
        assert(fs::exists(project/"t0"/"sample_Original.wav"));
        assert(app.manageFile(1,project.string(),"New title").empty());
        std::ifstream title(project/"name.txt");std::string name;std::getline(title,name);assert(name=="New title");
        assert(!app.manageFile(2,project.string(),"").empty()); // loaded project copy is in use
        assert(!app.manageFile(2,"/tmp","").empty());
        assert(app.saveProject("Second snapshot"));
        wait([&]{return s.fileSaveState()!=1;});assert(s.fileSaveState()==2);
        // Exact loop PCM and transport metadata survive save/load, not a rendered mix.
        fs::path second;
        for(auto& e:fs::directory_iterator(dir))if(e.is_directory()&&e.path()!=project&&e.path().filename().string().rfind("project_",0)==0)second=e.path();
        assert(!second.empty());
        auto bytes=[](const fs::path& p){std::ifstream in(p,std::ios::binary);return std::vector<char>(std::istreambuf_iterator<char>(in),{});};
        assert(bytes(project/"loops.bin")==bytes(second/"loops.bin"));
        tick(180);app.loopSave();app.loopSave();wait([&]{return s.fileSaveState()!=1;});assert(s.fileSaveState()==2);
        assert(s.sampleLibrary().size()==1);
    }
    fs::remove_all(dir);std::puts("project files OK");
}
