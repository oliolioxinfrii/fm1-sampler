#include <GenerativeField.h>
#include <Session.h>
#include <SamplerApp.h>
#include <SamplerUI.h>
#include <Wav.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <chrono>
using namespace generative;
int main(){
 Config c;c.evolution=0;c.wander=0;
 auto p=makePhrase(c,0),q=makePhrase(c,17);
 for(int i=0;i<p.n;i++){assert(p.event[i].midi==q.event[i].midi);assert(p.event[i].on==q.event[i].on);assert(p.event[i].off>p.event[i].on);}
 c.apply=1;p=makePhrase(c,0);for(int i=0;i<p.n;i++)assert(p.event[i].on==p.event[i].nominalOn);
 c.apply=2;p=makePhrase(c,0);for(int i=0;i<p.n;i++)assert(p.event[i].midi==rootMidi(c)+p.event[i].base);
 c.apply=0;c.pins[2]=7;p=makePhrase(c,0);assert(p.event[2].midi==rootMidi(c)+7&&std::abs(p.event[2].pitch)<.001f);
 c.wander=1;assert(std::abs(raw(.43f,.37f,c,0,0)-raw(.43f,.37f,c,5,0))>.00001f);
 c.motif=3;c.customCount=0;c.pins.fill(-1);
 assert(customKey(c,9)&&customKey(c,5)&&customKey(c,7)&&customKey(c,0));
 assert(!customKey(c,7)&&c.customCount==4);assert(customKey(c,0)&&c.customCount==3);
 assert(customKey(c,7)&&c.customCount==2);assert(customKey(c,11)&&c.customCount==3);
 c.depth=0;c.octave=4;p=makePhrase(c,0);c.octave=5;q=makePhrase(c,0);
 for(int i=0;i<p.n;i++)assert(q.event[i].midi-p.event[i].midi==12);
 // Real PCM through the independent session voice, without tape track mutation.
 auto path=std::filesystem::temp_directory_path()/"fm1-generator-test.wav";
 std::vector<float> pcm(44100*2);
 for(int i=0;i<44100;i++)pcm[2*i]=pcm[2*i+1]=.3f*std::sin(tau*220*i/44100);
 assert(writeWav16(path.string(),pcm,44100));
 Session s(44100);s.generatorEnable(true);s.generatorLoad(path.string());
 float out[512]{};
 for(int i=0;i<2000&&s.generatorLoadState()!=2;i++){s.process(out,nullptr,256);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 assert(s.generatorLoadState()==2);s.generatorPlay(c);
 double sum=0;for(int i=0;i<400;i++){s.process(out,nullptr,256);for(float v:out)sum+=v*v;}
 assert(sum>1&&s.generatorSnapshot().running);
 s.generatorStop();s.process(out,nullptr,256);for(float v:out)assert(v==0);
 s.generatorEnable(false);s.process(out,nullptr,256);assert(!s.generatorSnapshot().enabled);
 std::filesystem::remove(path);
 std::puts("generative field, constraints, timing, custom keys and isolated PCM passed");
}
