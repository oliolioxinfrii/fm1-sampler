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
 // Real app-side hardware routing and full-screen RLE transfer.
 Session hw(44100);
 bool pal=false;std::array<bool,240*240> coverage{};uint64_t leds=0;
 SamplerApp a(hw,[&](const uint8_t* m,size_t n){
   assert(n<=300);if(n<8)return;
   auto n14=[](const uint8_t* b){return int(b[0])*128+b[1];};
   auto cmd=[&](const uint8_t* b,size_t len){
      if(!len)return;
      if(b[0]==2){coverage.fill(false);pal=false;}
      if(b[0]==8){assert(len==49);pal=true;}
      if(b[0]==1&&len==7){leds=0;for(int i=0;i<6;i++)leds|=uint64_t(b[i+1])<<(7*i);}
      if(b[0]==9){
       assert(pal&&len>=9);
       int x=n14(b+1),y=n14(b+3),w=n14(b+5),h=n14(b+7);
       assert(w>0&&h>0&&x+w<=240&&y+h<=240&&w*h<=768);
       int pos=0;
       for(size_t i=9;i<len;i++){
        int run=(b[i]&7)+1;if(run==8){assert(i+1<len);run+=b[++i];}
        assert(pos+run<=w*h);
        while(run--){coverage[(y+pos/w)*240+x+pos%w]=true;pos++;}
       }assert(pos==w*h);
      }
   };
   if(m[6]!=10){cmd(m+6,n-7);return;}
   for(size_t i=7;i+3<n;){size_t len=n14(m+i);assert(len>0&&i+2+len<n);cmd(m+i+2,len);i+=2+len;}
 });
 uint32_t now=0;
 auto run=[&](int ms){uint32_t end=now+ms;while(now<end){now+=16;hw.process(out,nullptr,256);a.tick(now);}};
 auto button=[&](int k,bool down){const uint8_t m[]={uint8_t(down?0x91:0x81),uint8_t(k),uint8_t(down?100:0)};a.onMidi(m,3);};
 auto key=[&](int pc){const uint8_t down[]={0x90,uint8_t(60+pc),100},up[]={0x80,uint8_t(60+pc),0};a.onMidi(down,3);a.onMidi(up,3);};
 button(SamplerApp::REC,true);run(800);assert(a.generativeDeck());button(SamplerApp::REC,false);
 run(9000);assert(std::all_of(coverage.begin(),coverage.end(),[](bool v){return v;}));
 assert(!a.recordingNow()&&!a.loopExists());
 a.genMotif(3);
 key(9);key(5);key(7);key(0);key(7);assert(a.genConfig().customCount==4);
 run(1000);
 for(int k=0;k<27;k++)if(pc(53+k)==9||pc(53+k)==5||pc(53+k)==7||pc(53+k)==0)assert(leds&(1ull<<(14+k)));
 key(0);key(7);key(11);assert(a.genConfig().customCount==3&&a.genConfig().custom[2]==11);
 a.genTogglePin(1);int pinnedPc=pc(a.genPhrase().event[1].midi);assert(a.genKeyState(pinnedPc)==3);
 key(pinnedPc);assert(a.genConfig().pins[1]<0);
 SamplerUI view(a,false);view.setSize(960,450,0,0);view.frame(now);
 assert(!view.ops().empty());
 button(SamplerApp::REC,true);run(800);assert(!a.generativeDeck());button(SamplerApp::REC,false);
 assert(!a.recordingNow());
 std::puts("generative field, constraints, timing, custom keys and isolated PCM passed");
}
